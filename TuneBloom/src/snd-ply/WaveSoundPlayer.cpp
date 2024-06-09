#include "snd/WaveSoundPlayer.h"

#include "snd/BankR.h"

WaveSoundPlayer::WaveSoundPlayer()
    : mIsRegisterPlayerCallback(false)
    , mChannel(nullptr)
{
}

void WaveSoundPlayer::init()
{
    BasicSoundPlayer::init();

    mReleasePriorityFixFlag = false;
    mPanRange = 1.0f;

    mWsdFile = nullptr;
    mWaveFile = nullptr;
    mWaveSoundIndex = -1;

    mUpdateType = snd::UpdateType::AudioFrame;

    mWaveSoundInfo.pitch = 1.0f;
    mWaveSoundInfo.pan = 64;
    mWaveSoundInfo.surroundPan = 0;
    for (s32 i = 0; i < (s32)snd::AuxBus::Num; i++)
    {
        mWaveSoundInfo.fxSend[i] = 0;
    }
    mWaveSoundInfo.mainSend = 127;

    mWaveSoundInfo.lpfFreq = 64;
    mWaveSoundInfo.biquadType = 0;
    mWaveSoundInfo.biquadValue = 0;

    mWavePlayFlag = false;
    mChannel = nullptr;

    mSampleRate = 0;
    mSampleCount = 0;
}

void WaveSoundPlayer::deinit(bool stop)
{
    if (mIsRegisterPlayerCallback)
    {
        snd::internal::driver::SoundThread::instance()->unregisterPlayerCallback(this);
        mIsRegisterPlayerCallback = false;
    }

    closeChannel(stop);

    mActiveFlag = false;
}

void WaveSoundPlayer::prepare(s32 index, const PrepareArg& arg, snd::UpdateType updateType)
{
    mWaveSoundIndex = index;
    mUpdateType = updateType;

    mWsdFile = arg.wsdFile;
    mWaveFile = arg.waveFile;

    if (!mIsRegisterPlayerCallback)
    {
        snd::internal::driver::SoundThread::instance()->registerPlayerCallback(this);
        mIsRegisterPlayerCallback = true;
    }

    nw::snd::internal::WaveInfo waveInfo;
    {
        nw::snd::internal::WaveFileReader reader(mWaveFile);
        if (!reader.ReadWaveInfo(&waveInfo))
            return;
    }

    mSampleRate = waveInfo.sampleRate;
    mSampleCount = waveInfo.loopEndFrame;

    mActiveFlag = true;
}

void WaveSoundPlayer::prepare(const WaveFile& waveFile, s32 channelIdx, snd::UpdateType updateType)
{
    mUpdateType = updateType;

    SEAD_ASSERT(channelIdx < (s32)snd::cWaveChannelMax);

    if (!mIsRegisterPlayerCallback)
    {
        snd::internal::driver::SoundThread::instance()->registerPlayerCallback(this);
        mIsRegisterPlayerCallback = true;
    }

    mSampleRate = waveFile.getSampleRate();
    mSampleCount = waveFile.getSampleCount();

    nw::snd::internal::WaveInfo waveInfo;
    nw::snd::internal::GetWaveInfoFromWaveFile(&waveInfo, waveFile, channelIdx);

    startChannel(&waveInfo);

    mActiveFlag = true;
}

void WaveSoundPlayer::setBankNoteInfo(u8 key, u8 velocity, const BankFile::VelocityRegion& velocityRegion)
{
    if (!isChannelActive())
    {
        return;
    }

    SEAD_ASSERT(key < 128);
    SEAD_ASSERT(velocity < 128);

    mChannel->setKey(key, velocityRegion.getOriginalKey());
    mChannel->setVelocity(BankR::calcChannelVelocityVolume(velocity));
    mChannel->setInstrumentVolume(velocityRegion.getVolume() * BankR::INSTRUMENT_VOLUME_CRITERION_R);
    mChannel->setTune(velocityRegion.getPitch());

    mChannel->setAttack(velocityRegion.getAdshrCurve().attack);
    mChannel->setHold(velocityRegion.getAdshrCurve().hold);
    mChannel->setDecay(velocityRegion.getAdshrCurve().decay);
    mChannel->setSustain(velocityRegion.getAdshrCurve().sustain);
    mChannel->setRelease(velocityRegion.getAdshrCurve().release);

    f32 initPan = static_cast<f32>(velocityRegion.getPan() - 64) / 63.0f;
    mChannel->setInitPan(initPan);

    mChannel->setKeyGroupId(velocityRegion.getKeyGroup());
    mChannel->setIsIgnoreNoteOff(velocityRegion.isIgnoreNoteOff());
    mChannel->setInterpolationType(velocityRegion.getInterpolationType());
}

void WaveSoundPlayer::pause(bool flag)
{
    mPauseFlag = flag;

    if (isChannelActive())
    {
        if (mChannel->isPause() != flag)
            mChannel->pause(flag);
    }
}

void WaveSoundPlayer::update()
{
    if (!mActiveFlag)
        return;

    // Check for waveform terminator completion
    if (mWavePlayFlag && !mChannel)
    {
        //finishPlayer();
        deinit();
        return;
    }

    // Waveform playback
    if (!mWavePlayFlag)
    {
        if (!startChannel())
        {
            //finishPlayer();
            deinit();
            return;
        }
    }

    updateChannel();
}

bool WaveSoundPlayer::startChannel(const nw::snd::internal::WaveInfo* waveInfoPtr)
{
    nw::snd::internal::WaveInfo waveInfo;
    if (waveInfoPtr)
    {
        waveInfo = *waveInfoPtr;
    }
    else
    {
        nw::snd::internal::WaveFileReader reader(mWaveFile);
        if (!reader.ReadWaveInfo(&waveInfo))
            return false;
    }

    u32 startOffsetSamples = 0;

    snd::internal::driver::Channel* channel = snd::internal::driver::Channel::allocChannel(
        sead::Mathi::min(static_cast<s32>(waveInfo.channelCount), 2),
        snd::internal::Voice::cPriorityNoDrop,
        &channelCallbackFunc,
        this
    );

    if (!channel)
        return false;

    if (!waveInfoPtr)
    {
        {
            nw::snd::internal::WaveSoundFileReader reader(mWsdFile);
            if (!reader.ReadWaveSoundInfo(&mWaveSoundInfo, mWaveSoundIndex))
                return false;
        }

        channel->setAttack(mWaveSoundInfo.adshr.attack);
        channel->setHold(mWaveSoundInfo.adshr.hold);
        channel->setDecay(mWaveSoundInfo.adshr.decay);
        channel->setSustain(mWaveSoundInfo.adshr.sustain);
        channel->setRelease(mWaveSoundInfo.adshr.release);
    }

    channel->setReleasePriorityFix(mReleasePriorityFixFlag);
    //channel->setFrontBypass(isFrontBypass());
    channel->setUpdateType(mUpdateType);

    snd::internal::WaveInfo waveInfoS;
    nw::snd::internal::ConvertWaveInfo(&waveInfoS, waveInfo);

    channel->start(waveInfoS, -1, startOffsetSamples);

    mChannel = channel;

    mWavePlayFlag = true;

    return true;
}

void WaveSoundPlayer::closeChannel(bool stop)
{
    // Release
    if (isChannelActive())
    {
        updateChannel();
        mChannel->noteOff();

        if (stop)
            mChannel->stop();
    }

    // Free
    if (mChannel)
    {
        snd::internal::driver::Channel::detachChannel(mChannel);
        mChannel = nullptr;
    }
}

void WaveSoundPlayer::updateChannel()
{
    if (!mChannel)
        return;

    // volume
    f32 volume = 1.0f;
    volume *= getVolume();

    f32 pitchRatio = 1.0f;
    pitchRatio *= getPitch();
    pitchRatio *= mWaveSoundInfo.pitch;

    // lpf freq
    f32 lpfFreq = static_cast<f32>(mWaveSoundInfo.lpfFreq - 64) / 64.0f;
    lpfFreq += getLpfFreq();

    // biquad filter
    s32 biquadType = static_cast<s32>(mWaveSoundInfo.biquadType);
    f32 biquadValue = static_cast<f32>(mWaveSoundInfo.biquadValue) / 127.0f;
    s32 handleBiquadType = getBiquadFilterType();
    if (handleBiquadType != static_cast<s32>(snd::BiquadFilterType::Inherit))
    {
        biquadType  = handleBiquadType;
        biquadValue = getBiquadFilterValue();
    }

    // Calculate the data value (TODO: Since it appears that this can be cached, cache it)
    f32 panBase = 0.0f;
    if (mWaveSoundInfo.pan <= 1) // Pan 1 and 2 will be the same values.
        panBase = static_cast<f32>(static_cast<s32>(mWaveSoundInfo.pan) - 63) / 63.0f;
    else
        panBase = static_cast<f32>(static_cast<s32>(mWaveSoundInfo.pan) - 64) / 63.0f;

    panBase *= mPanRange;

    f32 spanBase = 0.0f;
    if (mWaveSoundInfo.surroundPan <= 63)
        spanBase = static_cast<f32>(mWaveSoundInfo.surroundPan) / 63.0f;
    else
        spanBase = static_cast<f32>(mWaveSoundInfo.surroundPan + 1) / 64.0f; // NOTE: y = (1/64) * x + 1/64 = (x/64) + (1/64) = (x+1)/64

    f32 mainSendBase = static_cast<f32>(mWaveSoundInfo.mainSend) / 127.0f - 1.0f;
    f32 fxSendBase[(s32)snd::AuxBus::Num];
    for (s32 i = 0; i < (s32)snd::AuxBus::Num; i++)
    {
        fxSendBase[i] = static_cast<f32>(mWaveSoundInfo.fxSend[i]) / 127.0f;
    }

    // Parameters for TV output (overlap with data values)
    snd::internal::OutputParam tvParam = getParam();
    {
        tvParam.pan += panBase;
        //tvParam.span += spanBase;
        tvParam.mainSend += mainSendBase;
        for (s32 i = 0; i < (s32)snd::AuxBus::Num; i++)
        {
            tvParam.fxSend[i] += fxSendBase[i];
        }
    }

    mChannel->setPanMode(getPanMode());
    mChannel->setPanCurve(getPanCurve());
    //mChannel->setRemoteFilter(getRemoteFilter());
    mChannel->setUserVolume(volume);
    mChannel->setUserPitchRatio(pitchRatio);
    //mChannel->setLfoParam(mLfoParam);
    mChannel->setUserLpfFreq(lpfFreq);
    mChannel->setBiquadFilter(biquadType, biquadValue);
    mChannel->setOutputLine(getOutputLine());

    mChannel->setParam(tvParam);
}

s32 WaveSoundPlayer::getPlaySamplePosition(bool isOriginalSamplePosition) const
{
    snd::internal::driver::SoundThreadLock lock;

    if (!mChannel)
        return 0;

    return static_cast<s32>(mChannel->getCurrentPlayingSample(isOriginalSamplePosition));
}

void WaveSoundPlayer::channelCallbackFunc(snd::internal::driver::Channel* dropChannel, snd::internal::driver::Channel::ChannelCallbackStatus, void* userData)
{
    WaveSoundPlayer* player = reinterpret_cast<WaveSoundPlayer*>(userData);

    SEAD_ASSERT(dropChannel);
    SEAD_ASSERT(player);

    SEAD_ASSERT(dropChannel == player->mChannel);

    player->mChannel = nullptr;
}
