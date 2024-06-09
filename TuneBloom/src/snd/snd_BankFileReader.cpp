#include <snd/snd_BankFileReader.h>

#include <basis/seadWarning.h>
#include <prim/seadMemUtil.h>

namespace nw { namespace snd { namespace internal {

BankFileReader::BankFileReader()
    : mHeader(nullptr)
    , mInfoBlockBody(nullptr)
    , mIsInitialized(false)
{
}

BankFileReader::BankFileReader(const void* bankFile)
    : mHeader(nullptr)
    , mInfoBlockBody(nullptr)
    , mIsInitialized(false)
{
    Initialize(bankFile);
}

void BankFileReader::Initialize(const void* bankFile)
{
    if (!bankFile)
        return;

    {
        const ut::BinaryFileHeader* header = reinterpret_cast<const ut::BinaryFileHeader*>(bankFile);

        //if (sead::MemUtil::compare(header->signature, "CBNK", 4) != 0)
        if (sead::MemUtil::compare(header->signature, "FBNK", 4) != 0)
        {
            SEAD_ASSERT_MSG(false, "not a BANK file");
            return;
        }

        //if (false)
        if (header->version != 0x00010000)
        {
            SEAD_ASSERT_MSG(false, "BANK version not supported (0x%08X)", (u32)header->version);
            return;
        }
    }

    mHeader = reinterpret_cast<const BankFile::FileHeader*>(bankFile);

    const BankFile::InfoBlock* infoBlock = mHeader->GetInfoBlock();
    SEAD_ASSERT(infoBlock);

    if (sead::MemUtil::compare(infoBlock->header.kind, "INFO", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "BANK: INFO block is invalid");
        return;
    }

    mInfoBlockBody = &infoBlock->body;

    mIsInitialized = true;
}

void BankFileReader::Finalize()
{
    if (mIsInitialized)
    {
        mHeader = nullptr;
        mInfoBlockBody = nullptr;
        mIsInitialized = false;
    }
}

bool BankFileReader::ReadVelocityRegionInfo(VelocityRegionInfo* info, s32 programNo, s32 key, s32 velocity) const
{
    SEAD_ASSERT(info);

    if (!mIsInitialized)
        return false;

    if (programNo < 0 || programNo >= mInfoBlockBody->GetInstrumentCount())
        return false;

    const BankFile::Instrument* instrument = mInfoBlockBody->GetInstrument(programNo);
    if (!instrument)
        return false;

    const BankFile::KeyRegion* keyRegion = instrument->GetKeyRegion(key);
    if (!keyRegion)
        return false;

    const BankFile::VelocityRegion* velocityRegion = keyRegion->GetVelocityRegion(velocity);
    if (!velocityRegion)
        return false;

    SEAD_ASSERT(velocityRegion->waveIdTableIndex < mInfoBlockBody->GetWaveIdCount());
    const Util::WaveId* pWaveId = mInfoBlockBody->GetWaveId(velocityRegion->waveIdTableIndex);

    if (!pWaveId)
        return false;

    if (pWaveId->waveIndex == 0xFFFFFFFF)
    {
        SEAD_WARNING("This region [programNo(%d) key(%d) velocity(%d)] is not assigned wave file.", programNo, key, velocity);
        return false;
    }

    info->waveArchiveId = pWaveId->waveArchiveId;
    info->waveIndex = pWaveId->waveIndex;

    const BankFile::RegionParameter* regionParameter = velocityRegion->GetRegionParameter();
    if (!regionParameter)
    {
        info->originalKey       = velocityRegion->GetOriginalKey();
        info->volume            = velocityRegion->GetVolume();
        info->pan               = velocityRegion->GetPan();
        info->pitch             = velocityRegion->GetPitch();
        info->isIgnoreNoteOff   = velocityRegion->IsIgnoreNoteOff();
        info->keyGroup          = velocityRegion->GetKeyGroup();
        info->interpolationType = velocityRegion->GetInterpolationType();
        info->adshrCurve        = velocityRegion->GetAdshrCurve();
    }
    else
    {
        info->originalKey       = regionParameter->originalKey;
        info->volume            = regionParameter->volume;
        info->pan               = regionParameter->pan;
        info->pitch             = regionParameter->pitch;
        info->isIgnoreNoteOff   = regionParameter->isIgnoreNoteOff;
        info->keyGroup          = regionParameter->keyGroup;
        info->interpolationType = regionParameter->interpolationType;
        info->adshrCurve        = regionParameter->adshrCurve;
    }

    return true;
}

const Util::WaveIdTable* BankFileReader::GetWaveIdTable() const
{
    if (!mIsInitialized)
        return nullptr;

    return &mInfoBlockBody->GetWaveIdTable();
}

} } } // namespace nw::snd::internal
