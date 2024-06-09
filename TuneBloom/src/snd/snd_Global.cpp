#include <snd/snd_Global.h>

#include <snd/snd_WaveFileReader.h>

#include <bfsar/WaveFile.h>

namespace nw { namespace snd { namespace internal {

void GetWaveInfoFromWaveFile(WaveInfo* out, const ::WaveFile& waveFile, s32 channelIdx)
{
    SEAD_ASSERT(out);

    out->endian = waveFile.getDataEndian();
    out->sampleFormat = nw::snd::internal::WaveFileReader::GetSampleFormat((u8)waveFile.getEncoding());
    out->loopFlag = waveFile.getIsLoop();
    out->channelCount = channelIdx == -1 ? waveFile.getChannels().size() : 1;
    out->sampleRate = waveFile.getSampleRate();
    out->loopStartFrame = waveFile.getLoopStartFrame();
    out->loopEndFrame = waveFile.getLoopEndFrame();
    out->originalLoopStartFrame = waveFile.getOriginalLoopStartFrame();

    for (u32 i = 0; i < out->channelCount; i++)
    {
        ::WaveFile::Channel* channel = waveFile.getChannels().nth(channelIdx == -1 ? i : channelIdx);

        WaveInfo::ChannelParam& channelParam = out->channelParam[channelIdx == -1 ? i : 0];

        channelParam.dataAddress = channel->getData();

        if (out->sampleFormat == nw::snd::SAMPLE_FORMAT_DSP_ADPCM)
        {
            const ::snd::DspAdpcmParam& adpcmParam = channel->getAdpcmParam();
            const ::snd::internal::DspAdpcmLoopParam& adpcmLoopParam = channel->getAdpcmLoopParam();

            for (u32 i = 0; i < 8; i++)
            {
                channelParam.adpcmParam.coef[i][0] = adpcmParam.coef[i][0];
                channelParam.adpcmParam.coef[i][1] = adpcmParam.coef[i][1];
            }

            channelParam.adpcmParam.predScale = adpcmParam.predScale;
            channelParam.adpcmParam.yn1 = adpcmParam.yn1;
            channelParam.adpcmParam.yn2 = adpcmParam.yn2;

            channelParam.adpcmLoopParam.loopPredScale = adpcmLoopParam.loopPredScale;
            channelParam.adpcmLoopParam.loopYn1 = adpcmLoopParam.loopYn1;
            channelParam.adpcmLoopParam.loopYn2 = adpcmLoopParam.loopYn2;
        }
    }
}

void ConvertWaveInfo(::snd::internal::WaveInfo* out, const WaveInfo& waveInfo)
{
    SEAD_ASSERT(out);

    out->endian = waveInfo.endian; // Custom

    out->sampleFormat = (::snd::SampleFormat)waveInfo.sampleFormat;
    out->loopFlag = waveInfo.loopFlag;
    out->channelCount = waveInfo.channelCount;
    out->sampleRate = waveInfo.sampleRate;
    out->loopStartFrame = waveInfo.loopStartFrame;
    out->loopEndFrame = waveInfo.loopEndFrame;
    out->originalLoopStartFrame = waveInfo.originalLoopStartFrame;

    for (u32 i = 0; i < waveInfo.channelCount; i++)
    {
        out->channelParam[i].dataAddress = waveInfo.channelParam[i].dataAddress;

        out->channelParam[i].adpcmParam.predScale = waveInfo.channelParam[i].adpcmParam.predScale;
        out->channelParam[i].adpcmParam.yn1 = waveInfo.channelParam[i].adpcmParam.yn1;
        out->channelParam[i].adpcmParam.yn2 = waveInfo.channelParam[i].adpcmParam.yn2;
        for (u32 j = 0; j < 8; j++)
        {
            out->channelParam[i].adpcmParam.coef[j][0] = waveInfo.channelParam[i].adpcmParam.coef[j][0];
            out->channelParam[i].adpcmParam.coef[j][1] = waveInfo.channelParam[i].adpcmParam.coef[j][1];
        }

        out->channelParam[i].adpcmLoopParam.loopPredScale = waveInfo.channelParam[i].adpcmLoopParam.loopPredScale;
        out->channelParam[i].adpcmLoopParam.loopYn1 = waveInfo.channelParam[i].adpcmLoopParam.loopYn1;
        out->channelParam[i].adpcmLoopParam.loopYn2 = waveInfo.channelParam[i].adpcmLoopParam.loopYn2;
    }
}

} } } // namespace nw::snd::internal
