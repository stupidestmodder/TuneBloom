#pragma once

#include <snd/snd_Adpcm.h>
#include <snd/snd_DspadpcmReader.h>
#include <snd/snd_WaveFile.h>

namespace nw { namespace snd { namespace internal {

class WaveFileReader
{
public:
    static SampleFormat GetSampleFormat(u8 encodeMethod);

    WaveFileReader(const void* waveFile, s8 waveType = WAVE_TYPE_NWWAV);

    bool IsOriginalLoopAvailable() const;

    bool ReadWaveInfo(WaveInfo* info, const void* waveDataOffsetOrigin = nullptr) const;

    struct OffsetContextInfo
    {
        u32 offsetSample;
        AdpcmContext contexts[WAVE_CHANNEL_MAX];

        OffsetContextInfo()
            : offsetSample(0)
        {
        }
    };
    //bool CalcOffsetContextInfo(u32 offsetSample, OffsetContextInfo* info) const;

    const void* GetWaveDataAddress(const WaveFile::ChannelInfo* info, const void* waveDataOffsetOrigin) const;

    const WaveFile::FileHeader* mHeader;
    const WaveFile::InfoBlockBody* mInfoBlockBody;
    const void* mDataBlockBody;

    DspadpcmReader mDspadpcmReader;
    s8 mWaveType;
};

} } } // namespace nw::snd::internal
