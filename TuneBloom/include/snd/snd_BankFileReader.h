#pragma once

#include <snd/snd_BankFile.h>

namespace nw { namespace snd { namespace internal {

struct VelocityRegionInfo
{
    u32 waveArchiveId;
    u32 waveIndex;

    f32 pitch;
    AdshrCurve adshrCurve;

    u8 originalKey;
    u8 volume;
    u8 pan;
    bool isIgnoreNoteOff;
    u8 keyGroup;
    u8 interpolationType;
};

class BankFileReader
{
public:
    BankFileReader();
    explicit BankFileReader(const void* bankFile);

    void Initialize(const void* bankFile);
    void Finalize();

    bool IsInitialized() const { return mIsInitialized; }

    bool ReadVelocityRegionInfo(VelocityRegionInfo* info, s32 programNo, s32 key, s32 velocity) const;

    const Util::WaveIdTable* GetWaveIdTable() const;
    const void* GetBankFileAddress() const { return mHeader; }

    s32 GetInstrumentCount() const
    {
        if (!mIsInitialized)
            return 0;

        return mInfoBlockBody->GetInstrumentCount();
    }

    const nw::snd::internal::BankFile::Instrument* GetInstrument(s32 programNo)
    {
        if (!mIsInitialized)
            return nullptr;

        return mInfoBlockBody->GetInstrument(programNo);
    }

    const BankFile::FileHeader* mHeader;
    const BankFile::InfoBlockBody* mInfoBlockBody;
    bool mIsInitialized;
};

} } } // namespace nw::snd::internal
