#pragma once

#include <snd/snd_Global.h>

namespace nw { namespace snd { namespace internal {

class DspadpcmReader
{
public:
    DspadpcmReader()
        : mDspadpcmData(nullptr)
    {
    }

    void Initialize(const void* dspadpcmData)
    {
        mDspadpcmData = dspadpcmData;
    }

    bool ReadWaveInfo(WaveInfo* info) const;

    const void* mDspadpcmData;
};

} } } // namespace nw::snd::internal
