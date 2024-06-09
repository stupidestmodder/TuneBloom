#include <snd/snd_DspadpcmReader.h>

#include <prim/seadMemUtil.h>
#include <prim/seadPtrUtil.h>

namespace nw { namespace snd { namespace internal {

bool DspadpcmReader::ReadWaveInfo(WaveInfo* info) const
{
    SEAD_ASSERT(info);

    SEAD_ASSERT(false);
    return false;
/*
    const InternalDSPADPCMInfo& data = *reinterpret_cast<const InternalDSPADPCMInfo*>(mDspadpcmData);

    info->sampleFormat = SAMPLE_FORMAT_DSP_ADPCM;
    info->loopFlag = false; // Looping not yet supported. When supported, data.loop_flag == 1 ? true: false;.
    info->channelCount = 1;
    info->sampleRate = data.sample_rate;
    info->loopStartFrame = 0;
    info->loopEndFrame = data.num_samples;

    // dspadpcm outputs only mono waveforms.
    info->channelParam[0].dataAddress = sead::PtrUtil::addOffset(mDspadpcmData, sizeof(InternalDSPADPCMInfo));
    sead::MemUtil::copy(info->channelParam[0].adpcmParam.coef, &data.coef, sizeof(u16) * 16);
    sead::MemUtil::copy(&info->channelParam[0].adpcmParam.predScale, &data.ps, sizeof(u16) * 3);

    return true;
*/
}

} } } // namespace nw::snd::internal
