#include "snd/WavePlayer.h"

#include "snd/SoundThread.h"
#include "snd/MultiVoiceMgr.h"
#include "snd/DriverCommand.h"

namespace snd {

WavePlayer::WavePlayer()
    : mMultiVoice(nullptr)
    , mCallback(nullptr)
    , mCallbackArg(nullptr)
    , mChannel(nullptr)
{
}

WavePlayer::~WavePlayer()
{
    if (mMultiVoice)
        this->finalize();
}

bool WavePlayer::initialize(s32 channelCount, u32 priority, VoiceCallback callback, void* callbackArg)
{
    SEAD_ASSERT(mMultiVoice == nullptr);
    SEAD_ASSERT(channelCount >= 1 && channelCount <= cWaveChannelMax);
    SEAD_ASSERT(priority >= internal::Voice::cPriorityMin && priority <= internal::Voice::cPriorityMax);

    internal::driver::MultiVoice* voice = nullptr;

    {
        internal::driver::SoundThreadLock lock;

        voice = internal::driver::MultiVoiceMgr::instance()->allocVoice(channelCount, priority, &WavePlayer::MultiVoiceCallbackFunc, this);

        mChannel = internal::driver::Channel::allocChannel(channelCount, priority, nullptr, nullptr);
        SEAD_ASSERT(mChannel);
    }

    if (!voice)
        return false;

    mMultiVoice = voice;
    mCallback = callback;
    mCallbackArg = callbackArg;

    mParam.initialize();

    return true;
}

void WavePlayer::finalize()
{
    this->stop();
}

// change
void WavePlayer::start(snd::internal::WaveInfo* waveInfo)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

/*
    internal::DriverCommand* cmdMgr = internal::DriverCommand::instance();
    internal::DriverCommandVoicePlay* cmd = cmdMgr->allocCommand<internal::DriverCommandVoicePlay>();

    cmd->id = internal::DriverCommandId::VoicePlay;
    cmd->voice = mMultiVoice;
    cmd->state = internal::DriverCommandVoicePlay::State::Start;

    cmdMgr->pushCommand(cmd);
*/

    internal::driver::SoundThreadLock lock;

    if (!waveInfo)
    {
        mMultiVoice->start();
        return;
    }

    SEAD_ASSERT(mChannel);
    mChannel->start(*waveInfo, -1, 0);
}

// change
void WavePlayer::stop()
{
    if (!mMultiVoice)
        return;

    SEAD_ASSERT(mMultiVoice != nullptr);

/*
    internal::DriverCommand* cmdMgr = internal::DriverCommand::instance();
    internal::DriverCommandVoicePlay* cmd = cmdMgr->allocCommand<internal::DriverCommandVoicePlay>();

    cmd->id = internal::DriverCommandId::VoicePlay;
    cmd->voice = mMultiVoice;
    cmd->state = internal::DriverCommandVoicePlay::State::Stop;

    cmdMgr->pushCommand(cmd);
*/

    internal::driver::SoundThreadLock lock;
    mMultiVoice->stop();
    mMultiVoice->free();
    mMultiVoice = nullptr;

    SEAD_ASSERT(mChannel);
    mChannel->stop();
    //internal::driver::Channel::freeChannel(mChannel);
    mChannel = nullptr;
}

// change
void WavePlayer::pause(bool isPauseOn)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

/*
    internal::DriverCommand* cmdMgr = internal::DriverCommand::instance();
    internal::DriverCommandVoicePlay* cmd = cmdMgr->allocCommand<internal::DriverCommandVoicePlay>();

    cmd->id = internal::DriverCommandId::VoicePlay;
    cmd->voice = mMultiVoice;
    cmd->state = isPauseOn ? internal::DriverCommandVoicePlay::State::PauseOn : internal::DriverCommandVoicePlay::State::PauseOff;

    cmdMgr->pushCommand(cmd);
*/

    internal::driver::SoundThreadLock lock;
    mMultiVoice->pause(isPauseOn);
}

bool WavePlayer::isActive() const
{
    if (mMultiVoice)
        return mMultiVoice->isActive();

    return false;
}

bool WavePlayer::isRun() const
{
    if (mMultiVoice)
        return mMultiVoice->isRun();

    return false;
}

bool WavePlayer::isPause() const
{
    if (mMultiVoice)
        return mMultiVoice->isPause();

    return false;
}

u32 WavePlayer::getCurrentPlayingSample() const
{
    if (mMultiVoice)
        return mMultiVoice->getCurrentPlayingSample();

    return 0;
}

// change
void WavePlayer::setWaveInfo(SampleFormat format, u32 sampleRate, s32 interpolationType)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

/*
    internal::DriverCommand* cmdMgr = internal::DriverCommand::instance();
    internal::DriverCommandVoiceWaveInfo* cmd = cmdMgr->allocCommand<internal::DriverCommandVoiceWaveInfo>();

    cmd->id = internal::DriverCommandId::VoiceWaveInfo;
    cmd->voice = mMultiVoice;
    cmd->format = format;
    cmd->sampleRate = sampleRate;
    cmd->interpolationType = interpolationType;

    cmdMgr->pushCommand(cmd);
*/

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setSampleFormat(format);
    mMultiVoice->setSampleRate(sampleRate);
    mMultiVoice->setInterpolationType(interpolationType);
}

void WavePlayer::appendWaveBuffer(s32 channel, WaveBuffer* buffer, bool lastFlag)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    internal::driver::SoundThreadLock lock;
    mMultiVoice->appendWaveBuffer(channel, buffer, lastFlag);
}

// change
void WavePlayer::setPriority(u32 priority)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setPriority(priority);
}

// change
void WavePlayer::setVolume(f32 volume)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setVolume(volume);
}

// change
void WavePlayer::setPitch(f32 pitch)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setPitch(pitch);
}

// change
void WavePlayer::setPanMode(PanMode mode)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setPanMode(mode);
}

// change
void WavePlayer::setPanCurve(PanCurve curve)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setPanCurve(curve);
}

// change
void WavePlayer::setPan(f32 pan)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    mParam.pan = pan;

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setParam(mParam);
}

// change
void WavePlayer::setLpfFreq(f32 lpfFreq)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setLpfFreq(lpfFreq);
}

// change
void WavePlayer::setBiquadFilter(s32 type, f32 value)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setBiquadFilter(type, value);
}

// change
void WavePlayer::setOutputLine(u32 lineFlag)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setOutputLine(lineFlag);
}

// change
void WavePlayer::setOutVolume(f32 volume)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    mParam.volume = volume;

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setParam(mParam);
}

// change
void WavePlayer::setMainSend(f32 send)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    mParam.mainSend = send;

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setParam(mParam);
}

// change
void WavePlayer::setFxSend(AuxBus bus, f32 send)
{
    SEAD_ASSERT(mMultiVoice != nullptr);

    mParam.fxSend[(u32)bus] = send;

    internal::driver::SoundThreadLock lock;
    mMultiVoice->setParam(mParam);
}

void WavePlayer::MultiVoiceCallbackFunc(internal::driver::MultiVoice* driverVoice, internal::driver::MultiVoice::VoiceCallbackStatus status, void* arg)
{
    SEAD_ASSERT(arg != nullptr);

    switch (status)
    {
        case internal::driver::MultiVoice::VoiceCallbackStatus::FinishWave:
        case internal::driver::MultiVoice::VoiceCallbackStatus::Cancel:
            driverVoice->free(); // End the association with WaveBuffer.
            break;

        default:
            break;
    }

    WavePlayer* wavePlayer = static_cast<WavePlayer*>(arg);

    if (wavePlayer->mCallback)
        wavePlayer->mCallback(wavePlayer->mCallbackArg);

    wavePlayer->mMultiVoice = nullptr;
    wavePlayer->mCallback = nullptr;
    wavePlayer->mCallbackArg = nullptr;
}

} // namespace snd
