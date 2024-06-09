#pragma once

#include "Global.h"

namespace snd { namespace internal {

class WaveFileReader
{
public:
    struct OffsetContextInfo
    {
        OffsetContextInfo()
            : offsetSample(0)
        {
        }

        u32 offsetSample;
        AdpcmContext contexts[cWaveChannelMax];
    };

public:
    WaveFileReader(const void* waveFile, s8 waveType /*= WaveType::NwWav*/);

    //bool isOriginalLoopAvailable() const;

    //bool readWaveInfo(WaveInfo* info, const void* waveDataOffsetOrigin = nullptr) const;

    //bool calcOffsetContextInfo(u32 offsetSample, OffsetContextInfo* info) const;

    //static SampleFormat getSampleFormat(u8 encodeMethod);

private:
    //const void* getWaveDataAddress(const WaveFile::ChannelInfo* info, const void* waveDataOffsetOrigin) const;

private:
    //const WaveFile::FileHeader* mHeader;
    //const WaveFile::InfoBlockBody* mInfoBlockBody;
    //const void* mDataBlockBody;

    //DspadpcmReader mDspadpcmReader;
    //s8 mWaveType;
};

} } // namespace snd::internal
