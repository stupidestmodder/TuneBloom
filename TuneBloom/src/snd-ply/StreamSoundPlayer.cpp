#include "snd/StreamSoundPlayer.h"

#include "snd/snd_StreamSoundFileReader.h"
#include "snd/snd_WaveFileReader.h"

#include "snd/VoiceImpl.h"

#include <basis/seadWarning.h>

namespace {

const u8 DEFAULT_LPF_FREQ     = 64; // No filter is applied.
const u8 DEFAULT_BIQUAD_TYPE  = 0;  // No filter is yet used.
const u8 DEFAULT_BIQUAD_VALUE = 0;  // No filter is applied.

}

StreamSoundPlayer::StreamSoundPlayer()
    : mSetup(false)
    , mPrepared(false)
    , mIsRegisterPlayerCallback(false)
    , mChannelCount(0)
    , mTrackCount(0)
{
}

void StreamSoundPlayer::init()
{
    BasicSoundPlayer::init();

    mSetup = false;
    mPrepared = false;

    // Initialize item (track parent) data parameters.
    mItemData.pitch = 1.0f;
    mItemData.mainSend = 1.0f;
    for (u32 i = 0; i < nw::snd::AUX_BUS_NUM; i++)
    {
        mItemData.fxSend[i] = 0.0f;
    }

    // Track initialization.
    for (s32 trackIndex = 0; trackIndex < cStrmTrackNum; trackIndex++)
    {
        StreamTrack& track = mTracks[trackIndex];
        track.mActiveFlag  = false;
        track.mVolume      = 1.0f;
        track.mOutputLine  = -1;

        track.mParam.initialize();
    }

    // Channel initialization.
    for (s32 channelIndex = 0; channelIndex < cStrmChannelNum; channelIndex++)
    {
        StreamChannel& channel = mChannels[channelIndex];
        channel.mBufferAddress = nullptr;
        channel.mVoice = nullptr;
    }

    mSampleRate = 0;
    mSampleCount = 0;
}

void StreamSoundPlayer::deinit(bool freeBuffers)
{
    for (u32 ch = 0; ch < mChannelCount; ch++)
    {
        if (freeBuffers)
        {
            void* buffer = mChannels[ch].mBufferAddress;
            if (buffer)
            {
                delete[] buffer;
                mChannels[ch].mBufferAddress = nullptr;
            }
        }

        snd::internal::driver::Channel* voice = mChannels[ch].mVoice;
        if (voice)
        {
            voice->stop();
            snd::internal::driver::Channel::detachChannel(voice);
            mChannels[ch].mVoice = nullptr;
        }
    }

    if (mIsRegisterPlayerCallback)
    {
        snd::internal::driver::SoundThread::instance()->unregisterPlayerCallback(this);
        mIsRegisterPlayerCallback = false;
    }

    mSetup = false;
    mPrepared = false;

    mActiveFlag = false;
}

void StreamSoundPlayer::setup(const SetupArg& arg)
{
    mItemData.set(arg);

    // SetupTrack
    {
        // Archive active tracks in order.
        u32 bitMask = arg.allocTrackFlag;
        u32 trackIndex = 0;

        while (bitMask != 0)
        {
            if (bitMask & 0x1)
            {
                if (trackIndex >= cStrmTrackNum)
                {
                    SEAD_WARNING("Too large track index (%d). Max track index is %d.", trackIndex, cStrmTrackNum - 1);
                    break;
                }

                mTracks[trackIndex].mActiveFlag = true;
            }

            bitMask >>= 1;
            trackIndex++;
        }

        mTrackCount = sead::Mathu::min(trackIndex, cStrmTrackNum);
        if (mTrackCount == 0)
        {
            //finalize();
            return;
        }

        mChannelCount = sead::Mathu::min(arg.allocChannelCount, cStrmChannelNum);
        if (mChannelCount == 0)
        {
            //finalize();
            return;
        }

        // Reflects track information embedded in sound archive (*.bXsar).
        for (s32 i = 0; i < cStrmTrackNum; i++)
        {
            mStreamDataInfoTracks[i] = arg.tracks[i];
        }
    }

    mSetup = true;
}

void StreamSoundPlayer::prepare(const void* strmFile, snd::UpdateType updateType)
{
    if (!mSetup)
        return;

    if (!mIsRegisterPlayerCallback)
    {
        snd::internal::driver::SoundThread::instance()->registerPlayerCallback(this);
        mIsRegisterPlayerCallback = true;
    }

    mUpdateType = updateType;

    nw::snd::internal::StreamSoundFileReader reader;
    reader.Initialize(strmFile);

    mChannelCount = sead::Mathu::min(reader.GetChannelCount(), cStrmChannelNum);

    // ReadTrackInfoFromStreamSoundFile
    {
        // If track information is embedded in bXstm (up to binary version 0.2.0.0)
        if (reader.IsTrackInfoAvailable())
        {
            u32 trackCount = reader.GetTrackCount();
            if (trackCount > cStrmTrackNum)
                trackCount = cStrmTrackNum;

            // Read track information.
            for (u32 i = 0; i < trackCount; i++)
            {
                nw::snd::internal::StreamSoundFileReader::TrackInfo trackInfo;
                if (!reader.ReadStreamTrackInfo(&trackInfo, i))
                {
                    SEAD_PRINT("Failed to read TrackInfo\n");
                    return;
                }

                mStreamDataInfoTracks[i].volume = trackInfo.volume;
                mStreamDataInfoTracks[i].pan = trackInfo.pan;
                mStreamDataInfoTracks[i].channelCount = trackInfo.channelCount;
                for (s32 ch = 0; ch < trackInfo.channelCount; ch++)
                {
                    mStreamDataInfoTracks[i].globalChannelIndex[ch] = trackInfo.globalChannelIndex[ch];
                }

                // Since there is no span or flag in bXstrm 0.2.0.0, it will not load.
                // mStreamDataInfoTracks[i].span = trackInfo.span;
                // mStreamDataInfoTracks[i].flags = trackInfo.flags;
                mStreamDataInfoTracks[i].span = 0;
                mStreamDataInfoTracks[i].flags = 0;

                mStreamDataInfoTracks[i].mainSend = 127;
                for(u32 j = 0; j < nw::snd::AUX_BUS_NUM; j++)
                {
                    mStreamDataInfoTracks[i].fxSend[j] = 0;
                }

                mStreamDataInfoTracks[i].lpfFreq = DEFAULT_LPF_FREQ;
                mStreamDataInfoTracks[i].biquadType = DEFAULT_BIQUAD_TYPE;
                mStreamDataInfoTracks[i].biquadValue = DEFAULT_BIQUAD_VALUE;
            }
        }
    }

    // ApplyTrackDataInfo
    {
        for (u32 i = 0; i < mTrackCount; i++)
        {
            const nw::snd::SoundArchive::StreamTrackInfo& trackInfo = mStreamDataInfoTracks[i];
            mTracks[i].volume = trackInfo.volume;
            mTracks[i].pan = trackInfo.pan;
            mTracks[i].span = trackInfo.span;
            mTracks[i].mainSend = trackInfo.mainSend;
            for (u32 j = 0; j < nw::snd::AUX_BUS_NUM; j++)
            {
                mTracks[i].fxSend[j] = trackInfo.fxSend[j];
            }
            mTracks[i].lpfFreq = trackInfo.lpfFreq;
            mTracks[i].biquadType = trackInfo.biquadType;
            mTracks[i].biquadValue = trackInfo.biquadValue;
            mTracks[i].flags = trackInfo.flags;
            mTracks[i].channelCount = trackInfo.channelCount;

            // Allocates StreamChannel to StreamTrack.
            for (s32 ch = 0; ch < trackInfo.channelCount; ch++)
            {
                s8 globalChannelIndex = trackInfo.globalChannelIndex[ch];
                SEAD_ASSERT(0 <= globalChannelIndex && globalChannelIndex < cStrmChannelNum);

                mTracks[i].mChannels[ch] = &mChannels[globalChannelIndex];
            }
        }
    }

    for (u32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        StreamChannel& channel = mChannels[channelIndex];

        snd::internal::driver::Channel* voice = snd::internal::driver::Channel::allocChannel(1, snd::internal::Voice::cPriorityNoDrop, &channelCallbackFunc, &channel);

        if (!voice)
        {
            for (u32 i = 0; i < channelIndex; i++)
            {
                StreamChannel& c = mChannels[i];
                if (c.mVoice)
                {
                    c.mVoice->stop();
                    snd::internal::driver::Channel::detachChannel(c.mVoice);
                    c.mVoice = nullptr;
                }
            }

            return;
        }

        channel.mVoice = voice;
    }

    nw::snd::internal::StreamSoundFile::StreamSoundInfo streamSoundInfo;
    bool success = reader.ReadStreamSoundInfo(&streamSoundInfo);
    SEAD_ASSERT(success);

    //SEAD_PRINT("SampleDataOffset: 0x%08X\n", reader.GetSampleDataOffset());
    //SEAD_PRINT("channelCount: %d\n", streamSoundInfo.channelCount);
    //SEAD_PRINT("sampleRate: %d\n", (u32)streamSoundInfo.sampleRate);
    //SEAD_PRINT("loopStart: %d\n", (u32)streamSoundInfo.loopStart);
    //SEAD_PRINT("frameCount: %d\n", (u32)streamSoundInfo.frameCount);
    //SEAD_PRINT("originalLoopStart: %d\n", (u32)streamSoundInfo.originalLoopStart);
    //SEAD_PRINT("originalLoopEnd: %d\n", (u32)streamSoundInfo.originalLoopEnd);
    //SEAD_PRINT("blockCount: %d\n", (u32)streamSoundInfo.blockCount);
    //SEAD_PRINT("oneBlockBytes: %d\n", (u32)streamSoundInfo.oneBlockBytes);
    //SEAD_PRINT("oneBlockSamples: %d\n", (u32)streamSoundInfo.oneBlockSamples);
    //SEAD_PRINT("lastBlockBytes: %d\n", (u32)streamSoundInfo.lastBlockBytes);
    //SEAD_PRINT("lastBlockSamples: %d\n", (u32)streamSoundInfo.lastBlockSamples);
    //SEAD_PRINT("lastBlockPaddedBytes: %d\n", (u32)streamSoundInfo.lastBlockPaddedBytes);
    //SEAD_PRINT("sampleDataOffset: 0x%08X\n", (s32)streamSoundInfo.sampleDataOffset.offset);

    nw::snd::internal::WaveInfo waveInfo;
    waveInfo.endian = nw::ut::GetFileEndian(*reader.mHeader);
    waveInfo.sampleFormat = nw::snd::internal::WaveFileReader::GetSampleFormat(streamSoundInfo.encodeMethod);
    waveInfo.loopFlag = streamSoundInfo.isLoop;
    waveInfo.channelCount = streamSoundInfo.channelCount;
    waveInfo.sampleRate = streamSoundInfo.sampleRate;
    waveInfo.loopStartFrame = streamSoundInfo.loopStart;
    waveInfo.loopEndFrame = streamSoundInfo.frameCount;
    waveInfo.originalLoopStartFrame = streamSoundInfo.originalLoopStart;

    nw::snd::internal::WaveInfo waveInfos[cStrmChannelNum];
    for (u32 i = 0; i < waveInfo.channelCount; i++)
    {
        waveInfos[i] = waveInfo;
    }

    {
        u32 sampleSize = waveInfo.sampleFormat == nw::snd::SAMPLE_FORMAT_PCM_S16 ? sizeof(s16) : sizeof(u8);

        u8* channelBuffers[cStrmChannelNum];
        for (u32 i = 0; i < waveInfo.channelCount; i++)
        {
            channelBuffers[i] = new u8[streamSoundInfo.frameCount * sampleSize];
            mChannels[i].mBufferAddress = channelBuffers[i];
        }

        const u8* sampleData = (u8*)strmFile + reader.GetSampleDataOffset();

        for (u32 blockNo = 0; blockNo < streamSoundInfo.blockCount - 1; blockNo++)
        {
            for (u32 channelNo = 0; channelNo < waveInfo.channelCount; channelNo++)
            {
                u32 blockBytes = streamSoundInfo.oneBlockBytes;

                const void* src = sampleData;
                sampleData += blockBytes;

                void* dst = channelBuffers[channelNo] + (blockNo * blockBytes);

                sead::MemUtil::copy(dst, src, blockBytes);
            }
        }

        // Last block
        for (u32 channelNo = 0; channelNo < waveInfo.channelCount; channelNo++)
        {
            u32 blockBytes = streamSoundInfo.lastBlockBytes;

            const void* src = sampleData;
            sampleData += blockBytes;

            void* dst = channelBuffers[channelNo] + ((streamSoundInfo.blockCount - 1) * streamSoundInfo.oneBlockBytes);

            sead::MemUtil::copy(dst, src, blockBytes);

            sampleData += streamSoundInfo.lastBlockPaddedBytes - streamSoundInfo.lastBlockBytes;
        }

        for (u32 i = 0; i < waveInfo.channelCount; i++)
        {
            nw::snd::internal::WaveInfo& info = waveInfos[i];
            info.channelCount = 1;

            nw::snd::internal::WaveInfo::ChannelParam& channelParam = info.channelParam[0];
            channelParam.dataAddress = channelBuffers[i];

            if (waveInfo.sampleFormat == nw::snd::SAMPLE_FORMAT_DSP_ADPCM)
            {
                reader.ReadDspAdpcmChannelInfo(&channelParam.adpcmParam, &channelParam.adpcmLoopParam, i);
            }
        }
    }

    startPlayer(waveInfos);

    mSampleRate = streamSoundInfo.sampleRate;
    mSampleCount = streamSoundInfo.frameCount;

    mActiveFlag = true;
    mPrepared = true;
}

void StreamSoundPlayer::prepare(const Sound::StreamSoundInfo& streamSoundInfo, snd::UpdateType updateType)
{
    if (!mSetup)
        return;

    if (!mIsRegisterPlayerCallback)
    {
        snd::internal::driver::SoundThread::instance()->registerPlayerCallback(this);
        mIsRegisterPlayerCallback = true;
    }

    mUpdateType = updateType;

    // ApplyTrackDataInfo
    {
        for (u32 i = 0; i < mTrackCount; i++)
        {
            const nw::snd::SoundArchive::StreamTrackInfo& trackInfo = mStreamDataInfoTracks[i];
            mTracks[i].volume = trackInfo.volume;
            mTracks[i].pan = trackInfo.pan;
            mTracks[i].span = trackInfo.span;
            mTracks[i].mainSend = trackInfo.mainSend;
            for (u32 j = 0; j < nw::snd::AUX_BUS_NUM; j++)
            {
                mTracks[i].fxSend[j] = trackInfo.fxSend[j];
            }
            mTracks[i].lpfFreq = trackInfo.lpfFreq;
            mTracks[i].biquadType = trackInfo.biquadType;
            mTracks[i].biquadValue = trackInfo.biquadValue;
            mTracks[i].flags = trackInfo.flags;
            mTracks[i].channelCount = trackInfo.channelCount;

            // Allocates StreamChannel to StreamTrack.
            for (s32 ch = 0; ch < trackInfo.channelCount; ch++)
            {
                s8 globalChannelIndex = trackInfo.globalChannelIndex[ch];
                SEAD_ASSERT(0 <= globalChannelIndex && globalChannelIndex < cStrmChannelNum);

                mTracks[i].mChannels[ch] = &mChannels[globalChannelIndex];
            }
        }
    }

    for (u32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        StreamChannel& channel = mChannels[channelIndex];

        snd::internal::driver::Channel* voice = snd::internal::driver::Channel::allocChannel(1, snd::internal::Voice::cPriorityNoDrop, &channelCallbackFunc, &channel);

        if (!voice)
        {
            for (u32 i = 0; i < channelIndex; i++)
            {
                StreamChannel& c = mChannels[i];
                if (c.mVoice)
                {
                    c.mVoice->stop();
                    snd::internal::driver::Channel::detachChannel(c.mVoice);
                    c.mVoice = nullptr;
                }
            }

            return;
        }

        channel.mVoice = voice;
    }

    const WaveFile::Channel* channels[cStrmChannelNum];
    for (u32 i = 0; i < cStrmChannelNum; i++)
    {
        channels[i] = nullptr;
    }

    u32 channelNum = 0;
    for (s32 i = 0; i < streamSoundInfo.getTrackList().size(); i++)
    {
        const Sound::StreamSoundInfo::Track* track = static_cast<const Sound::StreamSoundInfo::Track*>(streamSoundInfo.getTrackList().nth(i)->val());

        SEAD_ASSERT(track->getWaveFileRef().isAttached());
        const WaveFile& wave = *static_cast<const WaveFile*>(track->getWaveFileRef().getItem());
        for (s32 ch = 0; ch < wave.getChannels().size(); ch++)
        {
            channels[channelNum] = wave.getChannels().nth(ch);
            channelNum++;
        }
    }

    const Sound::StreamSoundInfo::Track& mainTrack = *static_cast<const Sound::StreamSoundInfo::Track*>(streamSoundInfo.getTrackList().front()->val());

    SEAD_ASSERT(mainTrack.getWaveFileRef().isAttached());
    const WaveFile& mainWave = *static_cast<const WaveFile*>(mainTrack.getWaveFileRef().getItem());

    nw::snd::internal::WaveInfo waveInfo;
    waveInfo.endian = mainWave.getDataEndian();
    waveInfo.sampleFormat = nw::snd::internal::WaveFileReader::GetSampleFormat(static_cast<u8>(mainWave.getEncoding()));
    waveInfo.loopFlag = mainWave.getIsLoop();
    waveInfo.channelCount = streamSoundInfo.getAllocateChannelCount();
    waveInfo.sampleRate = mainWave.getSampleRate();
    waveInfo.loopStartFrame = mainWave.getLoopStartFrame();
    waveInfo.loopEndFrame = mainWave.getLoopEndFrame();
    waveInfo.originalLoopStartFrame = mainWave.getOriginalLoopStartFrame();

    nw::snd::internal::WaveInfo waveInfos[cStrmChannelNum];
    for (u32 i = 0; i < waveInfo.channelCount; i++)
    {
        waveInfos[i] = waveInfo;
    }

    {
        u32 sampleSize = waveInfo.sampleFormat == nw::snd::SAMPLE_FORMAT_PCM_S16 ? sizeof(s16) : sizeof(u8);
        u32 bufferSize = mainWave.getLoopEndFrame() * sampleSize;

        u8* channelBuffers[cStrmChannelNum];
        for (u32 i = 0; i < waveInfo.channelCount; i++)
        {
            channelBuffers[i] = new u8[bufferSize];
            mChannels[i].mBufferAddress = channelBuffers[i];

            const WaveFile::Channel& channel = *channels[i];
            u32 dataSize = sead::Mathu::min(bufferSize, channel.getDataSize());

            const void* src = channel.getData();
            u8* dst = channelBuffers[i];

            sead::MemUtil::copy(dst, src, dataSize);

            //? If other channels are smaller pad out
            if (channel.getDataSize() < bufferSize)
            {
                u32 remainSize = bufferSize - dataSize;

                dst += dataSize;
                sead::MemUtil::fillZero(dst, remainSize);
            }
        }

        for (u32 i = 0; i < waveInfo.channelCount; i++)
        {
            nw::snd::internal::WaveInfo& info = waveInfos[i];
            info.channelCount = 1;

            nw::snd::internal::WaveInfo::ChannelParam& channelParam = info.channelParam[0];
            channelParam.dataAddress = channelBuffers[i];

            if (mainWave.getEncoding() == WaveFile::Encoding::DspAdpcm)
            {
                const WaveFile::Channel& channel = *channels[i];

                const snd::DspAdpcmParam& adpcmParam = channel.getAdpcmParam();
                const snd::internal::DspAdpcmLoopParam& adpcmLoopParam = channel.getAdpcmLoopParam();

                for (u32 j = 0; j < 8; j++)
                {
                    channelParam.adpcmParam.coef[j][0] = adpcmParam.coef[j][0];
                    channelParam.adpcmParam.coef[j][1] = adpcmParam.coef[j][1];
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

    startPlayer(waveInfos);

    mSampleRate = mainWave.getSampleRate();
    mSampleCount = mainWave.getSampleCount();

    mActiveFlag = true;
    mPrepared = true;
}

void StreamSoundPlayer::pause(bool flag)
{
    bool pause = mPauseFlag != flag;

    mPauseFlag = flag;

    if (pause)
    {
        for (u32 ch = 0; ch < mChannelCount; ch++)
        {
            snd::internal::driver::Channel* voice = mChannels[ch].mVoice;
            if (voice)
            {
                voice->pause(flag);
            }
        }
    }
}

void StreamSoundPlayer::startPlayer(const nw::snd::internal::WaveInfo* waveInfos)
{
    for (u32 trackIndex = 0; trackIndex < mTrackCount; trackIndex++)
    {
        StreamTrack& track = mTracks[trackIndex];
        if (!track.mActiveFlag)
            continue;

        for (s32 ch = 0; ch < track.channelCount; ch++)
        {
            StreamChannel* channel = track.mChannels[ch];
            if (!channel)
                continue;

            snd::internal::driver::Channel* voice = channel->mVoice;
            if (voice)
            {
                s8 globalChannelIndex = mStreamDataInfoTracks[trackIndex].globalChannelIndex[ch];
                SEAD_ASSERT(0 <= globalChannelIndex && globalChannelIndex < cStrmChannelNum);

                const nw::snd::internal::WaveInfo& waveInfo = waveInfos[globalChannelIndex];

                snd::internal::WaveInfo waveInfoS;
                nw::snd::internal::ConvertWaveInfo(&waveInfoS, waveInfo);

                voice->setUpdateType(mUpdateType);
                voice->start(waveInfoS, -1, 0);
            }
        }
    }
}

void StreamSoundPlayer::update()
{
    if (!mSetup)
        return;

    if (!mPrepared)
        return;

    for (u32 ch = 0; ch < mChannelCount; ch++)
    {
        StreamChannel& channel = mChannels[ch];
        if (!channel.mVoice)
        {
            deinit(false);
            //mFinishFlag = true;
            return;
        }
    }

    for (u32 trackIndex = 0; trackIndex < mTrackCount; trackIndex++)
    {
        updateVoiceParams(&mTracks[trackIndex]);
    }
}

void StreamSoundPlayer::updateVoiceParams(StreamTrack* track)
{
    if (!track->mActiveFlag)
        return;

    TrackData trackData;
    trackData.set(track);

    // volume
    f32 volume = 1.0f;
    volume *= getVolume();
    volume *= trackData.volume;
    volume *= track->mVolume;

    // pitch
    f32 pitchRatio = 1.0f;
    pitchRatio *= getPitch();
    pitchRatio *= mItemData.pitch;

    // lpf freq
    f32 lpfFreq = trackData.lpfFreq;
    lpfFreq += getLpfFreq();

    // biquad filter
    s32 biquadType = trackData.biquadType;
    f32 biquadValue = trackData.biquadValue;
    s32 handleBiquadType = getBiquadFilterType();
    if (handleBiquadType != static_cast<s32>(snd::BiquadFilterType::Inherit))
    {
        biquadType  = handleBiquadType;
        biquadValue = getBiquadFilterValue();
    }

    // outputLine
    u32 outputLine = 0;
    if (track->mOutputLine != -1)
        outputLine = track->mOutputLine;
    else
        outputLine = getOutputLine();

    // Parameters for output to TV
    snd::internal::OutputParam tvParam;
    {
        tvParam = getParam();
        setOutputParam(&tvParam, track->mParam, trackData);
    }

    for (s32 ch = 0; ch < track->channelCount; ch++)
    {
        snd::internal::driver::Channel* voice = track->mChannels[ch]->mVoice;

        if (voice)
        {
            voice->setUserVolume(volume);
            voice->setUserPitchRatio(pitchRatio);
            voice->setUserLpfFreq(lpfFreq);
            voice->setBiquadFilter(biquadType, biquadValue);
            //voice->setFrontBypass(track->flags != 0);
            //voice->setRemoteFilter(getRemoteFilter());
            voice->setOutputLine(outputLine);

            if (track->channelCount == 1)
                voice->setParam(tvParam);
            else if (track->channelCount == 2)
                applyTvOutputParamForMultiChannel(&tvParam, voice, ch, static_cast<snd::MixMode>(tvParam.mixMode));
        }
    }
}

void StreamSoundPlayer::setOutputParam(snd::internal::OutputParam* pOutputParam, const snd::internal::OutputParam& trackParam, const TrackData& trackData)
{
    pOutputParam->volume *= trackParam.volume;

    for (s32 i = 0; i < snd::cWaveChannelMax; i++)
    {
        for (s32 j = 0; j < (s32)snd::ChannelIdx::Num; j++)
        {
            pOutputParam->mixParameter[i].ch[j] *= trackParam.mixParameter[i].ch[j];
        }
    }

    pOutputParam->pan += trackData.pan + trackParam.pan;
    //pOutputParam->span += trackData.span + trackParam.span;

    pOutputParam->mainSend += trackParam.mainSend;
    pOutputParam->mainSend += trackData.mainSend + mItemData.mainSend;

    for (s32 i = 0; i < (s32)snd::AuxBus::Num; i++)
    {
        pOutputParam->fxSend[i] += trackParam.fxSend[i];
        pOutputParam->fxSend[i] += trackData.fxSend[i] + mItemData.fxSend[i];
    }
}

void StreamSoundPlayer::mixSettingForOutputParam(snd::internal::OutputParam* pOutputParam, s32 channelIndex, snd::MixMode mixMode)
{
    if (mixMode == snd::MixMode::Pan)
    {
        if (channelIndex == 0)
            pOutputParam->pan -= 1.0f;
        else if (channelIndex == 1)
            pOutputParam->pan += 1.0f;
    }
    else
    {
        if (channelIndex == 1)
        {
            for (u32 i = 0; i < (u32)snd::ChannelIdx::Num; i++)
            {
                pOutputParam->mixParameter[0].ch[i] = pOutputParam->mixParameter[channelIndex].ch[i];
                pOutputParam->mixParameter[channelIndex].ch[i] = 0.0f;
            }
        }
    }
}

void StreamSoundPlayer::applyTvOutputParamForMultiChannel(snd::internal::OutputParam* pOutputParam, snd::internal::driver::Channel* pVoice, s32 channelIndex, snd::MixMode mixMode)
{
    // Save Progress
    f32 originalPan = pOutputParam->pan;

    snd::MixParameter originalMixParameter[snd::cWaveChannelMax];
    for (u32 i = 0; i < snd::cWaveChannelMax; i++)
    {
        for (u32 j = 0; j < (u32)snd::ChannelIdx::Num; j++)
        {
            originalMixParameter[i].ch[j] = pOutputParam->mixParameter[i].ch[j];
        }
    }

    mixSettingForOutputParam(pOutputParam, channelIndex, mixMode);

    // Set the parameter to pVoice.
    pVoice->setParam(*pOutputParam);

    // write back
    pOutputParam->pan = originalPan;
    for (u32 i = 0; i < snd::cWaveChannelMax; i++)
    {
        for (u32 j = 0; j < (u32)snd::ChannelIdx::Num; j++)
        {
            pOutputParam->mixParameter[i].ch[j] = originalMixParameter[i].ch[j];
        }
    }
}

void StreamSoundPlayer::channelCallbackFunc(snd::internal::driver::Channel* dropChannel, snd::internal::driver::Channel::ChannelCallbackStatus, void* arg)
{
    SEAD_ASSERT(arg);
    StreamChannel* channel = reinterpret_cast<StreamChannel*>(arg);

    SEAD_ASSERT(channel->mVoice == dropChannel);

    channel->mVoice = nullptr;
}

void StreamSoundPlayer::setTrackVolume(u32 trackBitFlag, f32 volume)
{
    for (u32 trackNo = 0; trackNo < mTrackCount && trackBitFlag != 0; trackNo++, trackBitFlag >>= 1)
    {
        if ((trackBitFlag & 0x1) == 0)
            continue;

        mTracks[trackNo].mVolume = volume;
    }
}

s32 StreamSoundPlayer::getPlaySamplePosition(bool isOriginalSamplePosition) const
{
    snd::internal::driver::SoundThreadLock lock;

    if (!mActiveFlag)
        return -1;

    if (!mTracks[0].mActiveFlag)
        return -1;

    //if (!mIsPrepared)
    //    return 0;

    const StreamChannel& strmChannel = mChannels[0];
    if (strmChannel.mVoice)
        return static_cast<long>(strmChannel.mVoice->getCurrentPlayingSample(isOriginalSamplePosition));

    return 0;
}

StreamTrack* StreamSoundPlayer::getPlayerTrack(s32 trackNo)
{
    if (trackNo > cStrmTrackNum - 1)
        return nullptr;

    return &mTracks[trackNo];
}

const StreamTrack* StreamSoundPlayer::getPlayerTrack(s32 trackNo) const
{
    if (trackNo > cStrmTrackNum - 1)
        return nullptr;

    return &mTracks[trackNo];
}
