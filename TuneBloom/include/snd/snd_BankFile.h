#pragma once

#include <snd/snd_ElementType.h>
#include <snd/snd_Global.h>
#include <snd/snd_Util.h>

#include <basis/seadWarning.h>
#include <math/seadMathCalcCommon.h>

namespace nw { namespace snd { namespace internal {

enum RegionType
{
    REGION_TYPE_DIRECT,
    REGION_TYPE_RANGE,
    REGION_TYPE_INDEX,
    REGION_TYPE_UNKNOWN
};

inline RegionType GetRegionType(u16 typeId)
{
    switch (typeId)
    {
        case ElementType_BankFile_DirectReferenceTable:
            return REGION_TYPE_DIRECT;

        case ElementType_BankFile_RangeReferenceTable:
            return REGION_TYPE_RANGE;

        case ElementType_BankFile_IndexReferenceTable:
            return REGION_TYPE_INDEX;

        default:
            return REGION_TYPE_UNKNOWN;
    }
}

struct DirectChunk
{
    Util::Reference toRegion;

    const void* GetRegion() const
    {
        return sead::PtrUtil::addOffset(this, toRegion.offset);
    }
};

struct RangeChunk
{
    Util::Table<u8> borderTable;

    const Util::Reference& GetRegionTableAddress(s32 index) const
    {
        return *reinterpret_cast<const Util::Reference*>(
            sead::PtrUtil::addOffset(
                this, sizeof(borderTable.count) + sead::Mathu::roundUpPow2(borderTable.count, 4) + sizeof(Util::Reference) * index
           )
       );
    }

    const void* GetRegion(u32 index) const
    {
        SEAD_ASSERT(index <= 127);

        bool isFoundRangeChunkIndex = false;
        u32 regionTableIndex = 0;
        for (u32 i = 0; i < borderTable.count; i++)
        {
            if (index <= borderTable.item[i])
            {
                regionTableIndex = i;
                isFoundRangeChunkIndex = true;
                break;
            }
        }

        if (!isFoundRangeChunkIndex)
            return nullptr;

        const Util::Reference& ref = GetRegionTableAddress(regionTableIndex);
        return sead::PtrUtil::addOffset(this, ref.offset);
    }
};

struct IndexChunk
{
    u8 min;
    u8 max;
    u8 reserved[2];
    Util::Reference toRegion[1];

    const void* GetRegion(u32 index) const
    {
        if (!(index >= min))
            SEAD_WARNING("out of region value[%d] < min[%d]\n", index, min);

        if (!(index <= max))
            SEAD_WARNING("out of region value[%d] > max[%d]\n", index, max);

        if (index < min)
            return nullptr;
        else if (index > max)
            return nullptr;

        return sead::PtrUtil::addOffset(this, toRegion[index - min].offset);
    }
};

struct BankFile
{
    struct InfoBlock;

    struct FileHeader : public Util::SoundFileHeader
    {
        const InfoBlock* GetInfoBlock() const;
    };

    struct Instrument;

    struct InfoBlockBody
    {
        Util::Reference toWaveIdTable;
        Util::Reference toInstrumentReferenceTable;

        const Util::WaveIdTable& GetWaveIdTable() const;
        const Util::ReferenceTable& GetInstrumentReferenceTable() const;

        u32 GetWaveIdCount() const
        {
            return GetWaveIdTable().GetCount();
        }
        s32 GetInstrumentCount() const
        {
            return GetInstrumentReferenceTable().count;
        }

        const Util::WaveId* GetWaveId(u32 index) const
        {
            return GetWaveIdTable().GetWaveId(index);
        }
        const Instrument* GetInstrument(s32 programNo) const;
    };

    struct InfoBlock
    {
        ut::BinaryBlockHeader header;
        InfoBlockBody body;
    };

    struct KeyRegion;
    struct Instrument
    {
        Util::Reference toKeyRegionChunk;

        const KeyRegion* GetKeyRegion(u32 key) const;
        void GetKeyRegionCount(s32* min, s32* max) const;

        // Added

        const void* GetRegionChunk() const
        {
            return sead::PtrUtil::addOffset(this, toKeyRegionChunk.offset);
        }

        const DirectChunk& GetDirectChunk() const
        {
            SEAD_ASSERT(toKeyRegionChunk.typeId == ElementType_BankFile_DirectReferenceTable);
            return *reinterpret_cast<const DirectChunk*>(GetRegionChunk());
        }

        const RangeChunk& GetRangeChunk() const
        {
            SEAD_ASSERT(toKeyRegionChunk.typeId == ElementType_BankFile_RangeReferenceTable);
            return *reinterpret_cast<const RangeChunk*>(GetRegionChunk());
        }

        const IndexChunk& GetIndexChunk() const
        {
            SEAD_ASSERT(toKeyRegionChunk.typeId == ElementType_BankFile_IndexReferenceTable);
            return *reinterpret_cast<const IndexChunk*>(GetRegionChunk());
        }
    };

    struct VelocityRegion;
    struct KeyRegion
    {
        Util::Reference toVelocityRegionChunk;

        const VelocityRegion* GetVelocityRegion(u32 velocity) const;
        void GetVelocityRegionCount(s32* min, s32* max) const;

        // Added

        const void* GetRegionChunk() const
        {
            return sead::PtrUtil::addOffset(this, toVelocityRegionChunk.offset);
        }

        const DirectChunk& GetDirectChunk() const
        {
            SEAD_ASSERT(toVelocityRegionChunk.typeId == ElementType_BankFile_DirectReferenceTable);
            return *reinterpret_cast<const DirectChunk*>(GetRegionChunk());
        }

        const RangeChunk& GetRangeChunk() const
        {
            SEAD_ASSERT(toVelocityRegionChunk.typeId == ElementType_BankFile_RangeReferenceTable);
            return *reinterpret_cast<const RangeChunk*>(GetRegionChunk());
        }

        const IndexChunk& GetIndexChunk() const
        {
            SEAD_ASSERT(toVelocityRegionChunk.typeId == ElementType_BankFile_IndexReferenceTable);
            return *reinterpret_cast<const IndexChunk*>(GetRegionChunk());
        }
    };

    struct RegionParameter;
    struct VelocityRegion
    {
        nw::ut::ResU32 waveIdTableIndex;
        Util::BitFlag optionParameter;

        u8 GetOriginalKey() const;
        u8 GetVolume() const;
        u8 GetPan() const;
        f32 GetPitch() const;
        bool IsIgnoreNoteOff() const;
        u8 GetKeyGroup() const;
        u8 GetInterpolationType() const;
        const AdshrCurve& GetAdshrCurve() const;

        const RegionParameter* GetRegionParameter() const;
    };

    struct RegionParameter
    {
        u8 padding1[3];
        u8 originalKey;
        u8 padding2[3];
        u8 volume;
        u8 padding3[2];
        s8 surroundPan;
        u8 pan;
        ut::ResF32 pitch;
        u8 padding4[1];
        u8 interpolationType;
        u8 keyGroup;
        bool isIgnoreNoteOff;
        u32 offset;
        Util::Reference refToAdshrCurve;
        AdshrCurve adshrCurve;
    };
};

} } } // namespace nw::snd::internal
