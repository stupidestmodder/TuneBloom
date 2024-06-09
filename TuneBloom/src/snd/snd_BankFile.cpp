#include <snd/snd_BankFile.h>

namespace nw { namespace snd { namespace internal {

namespace {

const u8 DEFAULT_ORIGINAL_KEY = 60;
const u8 DEFAULT_VOLUME = 127;
const u8 DEFAULT_PAN = 64;
const f32 DEFAULT_PITCH = 1.0f;
const bool DEFAULT_IGNORE_NOTE_OFF = false;
const u8 DEFAULT_KEY_GROUP = 0;
const u8 DEFAULT_INTERPOLATION_TYPE = 0;
const AdshrCurve DEFAULT_ADSHR_CURVE(
    127,    // u8 attack
    127,    // u8 decay
    127,    // u8 sustain
    127,    // u8 hold
    127     // u8 release
);

enum VelocityRegionBitFlag
{
    VELOCITY_REGION_KEY = 0x00,
    VELOCITY_REGION_VOLUME,
    VELOCITY_REGION_PAN,
    VELOCITY_REGION_PITCH,
    VELOCITY_REGION_INSTRUMENT_NOTE_PARAM,
    VELOCITY_REGION_ENVELOPE = 0x09,
    VELOCITY_REGION_BASIC_PARAM_FLAG =
        (1 << VELOCITY_REGION_KEY)
      | (1 << VELOCITY_REGION_VOLUME)
      | (1 << VELOCITY_REGION_PAN)
      | (1 << VELOCITY_REGION_PITCH)
      | (1 << VELOCITY_REGION_INSTRUMENT_NOTE_PARAM)
      | (1 << VELOCITY_REGION_ENVELOPE)
};

inline const void* GetDirectChunk(const void* regionChunk)
{
    const DirectChunk& directChunk = *reinterpret_cast<const DirectChunk*>(regionChunk);
    return directChunk.GetRegion();
}

inline const void* GetRangeChunk(const void* regionChunk, u32 index)
{
    const RangeChunk& rangeChunk = *reinterpret_cast<const RangeChunk*>(regionChunk);
    return rangeChunk.GetRegion(index);
}

inline const void* GetIndexChunk(const void* regionChunk, u32 index)
{
    const IndexChunk& indexChunk = *reinterpret_cast<const IndexChunk*>(regionChunk);
    return indexChunk.GetRegion(index);
}

const void* GetRegion(const void* startPtr, u16 typeId, u32 offset, u32 index)
{
    SEAD_ASSERT(startPtr);

    const void* regionChunk = sead::PtrUtil::addOffset(startPtr, offset);
    const void* region = nullptr;

    switch (GetRegionType(typeId))
    {
        case REGION_TYPE_DIRECT:
            region = GetDirectChunk(regionChunk);
            break;

        case REGION_TYPE_RANGE:
            region = GetRangeChunk(regionChunk, index);
            break;

        case REGION_TYPE_INDEX:
            region = GetIndexChunk(regionChunk, index);
            break;

        case REGION_TYPE_UNKNOWN:
        default:
            region = nullptr;
    }

    return region;
}

void GetRegionCount(const void* startPtr, u16 typeId, u32 offset, s32* min, s32* max)
{
    SEAD_ASSERT(startPtr);

    const void* regionChunk = sead::PtrUtil::addOffset(startPtr, offset);

    switch (GetRegionType(typeId))
    {
        case REGION_TYPE_DIRECT:
            *min = 0;
            *max = 0;
            break;

        case REGION_TYPE_RANGE:
            *min = 0;
            *max = 127;
            break;

        case REGION_TYPE_INDEX:
        {
            const IndexChunk& indexChunk = *reinterpret_cast<const IndexChunk*>(regionChunk);
            *min = indexChunk.min;
            *max = indexChunk.max;
            break;
        }

        case REGION_TYPE_UNKNOWN:
        default:
            *min = -1;
            *max = -1;
    }
}

} // anonymous namespace

const BankFile::InfoBlock* BankFile::FileHeader::GetInfoBlock() const
{
    return reinterpret_cast<const InfoBlock*>(GetBlock(ElementType_BankFile_InfoBlock));
}

const Util::WaveIdTable& BankFile::InfoBlockBody::GetWaveIdTable() const
{
    return *reinterpret_cast<const Util::WaveIdTable*>(sead::PtrUtil::addOffset(this, toWaveIdTable.offset));
}

const Util::ReferenceTable& BankFile::InfoBlockBody::GetInstrumentReferenceTable() const
{
    return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toInstrumentReferenceTable.offset));
}

const BankFile::Instrument* BankFile::InfoBlockBody::GetInstrument(s32 programNo) const
{
    SEAD_ASSERT(programNo < GetInstrumentCount());
    const Util::ReferenceTable& table = GetInstrumentReferenceTable();
    const Util::Reference& ref = table.item[programNo];

    switch (ref.typeId)
    {
        case ElementType_BankFile_InstrumentInfo:
            break;
        case ElementType_BankFile_NullInfo:
            return nullptr;
        default:
            SEAD_ASSERT_MSG(false, "cannot support Bank::InstRef::TypeId");
            return nullptr;
    }

    return reinterpret_cast<const BankFile::Instrument*>(table.GetReferedItem(programNo));
}

const BankFile::KeyRegion* BankFile::Instrument::GetKeyRegion(u32 key) const
{
    return reinterpret_cast<const BankFile::KeyRegion*>(
        GetRegion(
            this,
            toKeyRegionChunk.typeId,
            toKeyRegionChunk.offset,
            key
        )
    );
}

void BankFile::Instrument::GetKeyRegionCount(s32* min, s32* max) const
{
    GetRegionCount(this, toKeyRegionChunk.typeId, toKeyRegionChunk.offset, min, max);
}

const BankFile::VelocityRegion* BankFile::KeyRegion::GetVelocityRegion(u32 velocity) const
{
    return reinterpret_cast<const BankFile::VelocityRegion*>(
        GetRegion(
            this,
            toVelocityRegionChunk.typeId,
            toVelocityRegionChunk.offset,
            velocity
        )
    );
}

void BankFile::KeyRegion::GetVelocityRegionCount(s32* min, s32* max) const
{
    GetRegionCount(this, toVelocityRegionChunk.typeId, toVelocityRegionChunk.offset, min, max);
}

u8 BankFile::VelocityRegion::GetOriginalKey() const
{
    u32 value;
    bool result = optionParameter.GetValue(&value, VELOCITY_REGION_KEY);
    if (!result)
        return DEFAULT_ORIGINAL_KEY;

    return Util::DevideBy8bit(value, 0);
}

u8 BankFile::VelocityRegion::GetVolume() const
{
    u32 value;
    bool result = optionParameter.GetValue(&value, VELOCITY_REGION_VOLUME);
    if (!result)
        return DEFAULT_VOLUME;

    return Util::DevideBy8bit(value, 0);
}

u8 BankFile::VelocityRegion::GetPan() const
{
    u32 value;
    bool result = optionParameter.GetValue(&value, VELOCITY_REGION_PAN);
    if (!result)
        return DEFAULT_PAN;

    return Util::DevideBy8bit(value, 0);
}

f32 BankFile::VelocityRegion::GetPitch() const
{
    f32 value;
    bool result = optionParameter.GetValueF32(&value, VELOCITY_REGION_PITCH);
    if (!result)
        return DEFAULT_PITCH;

    return value;
}

bool BankFile::VelocityRegion::IsIgnoreNoteOff() const
{
    u32 value;
    bool result = optionParameter.GetValue(&value, VELOCITY_REGION_INSTRUMENT_NOTE_PARAM);
    if (!result)
        return DEFAULT_IGNORE_NOTE_OFF;

    return Util::DevideBy8bit(value, 0) > 0;
}

u8 BankFile::VelocityRegion::GetKeyGroup() const
{
    u32 value;
    bool result = optionParameter.GetValue(&value, VELOCITY_REGION_INSTRUMENT_NOTE_PARAM);
    if (!result)
        return DEFAULT_KEY_GROUP;

    return Util::DevideBy8bit(value, 1);
}

u8 BankFile::VelocityRegion::GetInterpolationType() const
{
    u32 value;
    bool result = optionParameter.GetValue(&value, VELOCITY_REGION_INSTRUMENT_NOTE_PARAM);
    if (!result)
        return DEFAULT_INTERPOLATION_TYPE;

    return Util::DevideBy8bit(value, 2);
}

const AdshrCurve& BankFile::VelocityRegion::GetAdshrCurve() const
{
    const nw::snd::internal::BankFile::RegionParameter* regionParameter = GetRegionParameter();
    if (regionParameter)
        return regionParameter->adshrCurve;

    // The following is valid ONLY if RegionParameter does not exist
    u32 offsetToReference;
    bool result = optionParameter.GetValue(&offsetToReference, VELOCITY_REGION_ENVELOPE);
    if (!result)
        return DEFAULT_ADSHR_CURVE;

    const Util::Reference& ref = *reinterpret_cast<const Util::Reference*>(sead::PtrUtil::addOffset(this, offsetToReference));
    return *reinterpret_cast<const AdshrCurve*>(sead::PtrUtil::addOffset(&ref, ref.offset));
}

const BankFile::RegionParameter* BankFile::VelocityRegion::GetRegionParameter() const
{
    if (optionParameter.bitFlag != VELOCITY_REGION_BASIC_PARAM_FLAG)
        return nullptr;

    return reinterpret_cast<const RegionParameter*>(sead::PtrUtil::addOffset(this, sizeof(VelocityRegion)));
}

} } } // namespace nw::snd::internal
