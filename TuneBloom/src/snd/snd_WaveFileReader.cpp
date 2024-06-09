#include <snd/snd_WaveFileReader.h>

#include <prim/seadMemUtil.h>

namespace nw { namespace snd { namespace internal {

SampleFormat WaveFileReader::GetSampleFormat(u8 format)
{
    switch (format)
    {
        case WaveFile::PCM8:        return SAMPLE_FORMAT_PCM_S8;
        case WaveFile::PCM16:       return SAMPLE_FORMAT_PCM_S16;
        case WaveFile::DSP_ADPCM:   return SAMPLE_FORMAT_DSP_ADPCM;

        default:
            SEAD_ASSERT_MSG(false, "Unknown wave data format(%d)", format);
            return SAMPLE_FORMAT_DSP_ADPCM;
    }
}

WaveFileReader::WaveFileReader(const void* waveFile, s8 waveType)
    : mHeader(nullptr)
    , mInfoBlockBody(nullptr)
    , mDataBlockBody(nullptr)
    , mWaveType(waveType)
{
    SEAD_ASSERT(waveFile);

    switch (mWaveType)
    {
        case WAVE_TYPE_NWWAV:
        {
            {
                const ut::BinaryFileHeader* header = reinterpret_cast<const ut::BinaryFileHeader*>(waveFile);

                //if (sead::MemUtil::compare(header->signature, "CWAV", 4) != 0)
                if (sead::MemUtil::compare(header->signature, "FWAV", 4) != 0)
                {
                    SEAD_ASSERT_MSG(false, "not a WAVE file");
                    return;
                }

                //if (false)
                if (!(0x00010000 <= header->version && header->version <= 0x00010200))
                {
                    SEAD_ASSERT_MSG(false, "WAVE version not supported (0x%08X)", (u32)header->version);
                    return;
                }
            }

            mHeader = reinterpret_cast<const WaveFile::FileHeader*>(waveFile);

            const WaveFile::InfoBlock* infoBlock = mHeader->GetInfoBlock();
            const WaveFile::DataBlock* dataBlock = mHeader->GetDataBlock();

            if (!infoBlock)
                return;

            if (!dataBlock)
                return;

            if (sead::MemUtil::compare(infoBlock->header.kind, "INFO", 4) != 0)
            {
                SEAD_ASSERT_MSG(false, "WAVE: INFO block is invalid");
                return;
            }

            if (sead::MemUtil::compare(dataBlock->header.kind, "DATA", 4) != 0)
            {
                SEAD_ASSERT_MSG(false, "WAVE: DATA block is invalid");
                return;
            }

            mInfoBlockBody = &infoBlock->body;
            mDataBlockBody = &dataBlock->byte;

            break;
        }

        case WAVE_TYPE_DSPADPCM:
            SEAD_ASSERT_MSG(false, "not implemented");
            mDspadpcmReader.Initialize(waveFile);
            break;
    }
}

bool WaveFileReader::IsOriginalLoopAvailable() const
{
    const ut::BinaryFileHeader& header = *reinterpret_cast<const ut::BinaryFileHeader*>(mHeader);
    if (header.version >= 0x00010200)
        return true;

    return false;
}

bool WaveFileReader::ReadWaveInfo(WaveInfo* info, const void* waveDataOffsetOrigin) const
{
    switch (mWaveType)
    {
        case WAVE_TYPE_NWWAV:
        {
            SEAD_ASSERT(mInfoBlockBody);

            info->endian = ut::GetFileEndian(mHeader->header);

            const SampleFormat format = GetSampleFormat(mInfoBlockBody->encoding);
            info->sampleFormat   = format;
            info->channelCount   = mInfoBlockBody->GetChannelCount();
            info->sampleRate     = mInfoBlockBody->sampleRate;
            info->loopFlag       = mInfoBlockBody->isLoop == 1;
            info->loopStartFrame = mInfoBlockBody->loopStartFrame;
            info->loopEndFrame   = mInfoBlockBody->loopEndFrame;

            if (IsOriginalLoopAvailable())
                info->originalLoopStartFrame = mInfoBlockBody->originalLoopStartFrame;
            else
                info->originalLoopStartFrame = mInfoBlockBody->loopStartFrame;

            for (s32 i = 0; i < mInfoBlockBody->GetChannelCount(); i++)
            {
                if (i >= WAVE_CHANNEL_MAX)
                    continue;

                WaveInfo::ChannelParam& channelParam = info->channelParam[i];

                const WaveFile::ChannelInfo& channelInfo = mInfoBlockBody->GetChannelInfo(i);

                // if (channelInfo.offsetToAdpcmInfo != 0)
                if (channelInfo.referToAdpcmInfo.offset != 0)
                {
                    const WaveFile::DspAdpcmInfo& adpcmInfo = channelInfo.GetDspAdpcmInfo();
                    channelParam.adpcmParam = adpcmInfo.adpcmParam;
                    channelParam.adpcmLoopParam = adpcmInfo.adpcmLoopParam;
                }

                channelParam.dataAddress = GetWaveDataAddress(&channelInfo, waveDataOffsetOrigin);
            }

            break;
        }

        case WAVE_TYPE_DSPADPCM:
        {
            mDspadpcmReader.ReadWaveInfo(info);
            break;
        }
    }

    return true;
}

const void* WaveFileReader::GetWaveDataAddress(const WaveFile::ChannelInfo* info, const void* waveDataOffsetOrigin) const
{
    SEAD_ASSERT(mInfoBlockBody);
    SEAD_ASSERT(info);

    SEAD_UNUSED(waveDataOffsetOrigin);

    return info->GetSamplesAddress(mDataBlockBody);
}

} } } // namespace nw::snd::internal
