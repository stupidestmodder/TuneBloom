#include "snd/SoundThread.h"

#include "snd/HardwareMgr.h"
#include "snd/MultiVoiceMgr.h"
#include "snd/DriverCommand.h"
#include "snd/ChannelMgr.h"
#include "snd/VoiceImpl.h"
#include "snd/Macro.h"

#include <thread/seadDelegateThread.h>
#include <prim/seadDelegate.h>
#include <prim/seadScopedLock.h>
#include <time/seadTickTime.h>

namespace snd { namespace internal { namespace driver {

SEAD_SINGLETON_DISPOSER_IMPL(SoundThread);

SoundThread::SoundThread()
    : mCreateFlag(false)
    , mPauseFlag(false)
    , mIsEnableGetTick(false)
    , mVoiceRenderer(nullptr)
    , mAudioFrameCounter(0)
    , mOboeCallbackCounter(0)
    , mThread(nullptr)
    , mUserCallback(nullptr)
    , mUserCallbackArg(nullptr)
{
    mSoundFrameCallbackList.initOffset(offsetof(SoundFrameCallback, mListNode));
    mPlayerCallbackList.initOffset(offsetof(PlayerCallback, mListNode));
}

SoundThread::~SoundThread()
{
    mSoundFrameCallbackList.clear();
    mPlayerCallbackList.clear();

    if (mThread)
    {
        if (!mThread->isDone())
            mThread->quitAndDestroySingleThread(true);

        delete mThread;
        mThread = nullptr;
    }
}

bool SoundThread::prepareForCreate_(bool enableGetTick)
{
    SEAD_ASSERT_MSG(HardwareMgr::instance()->isInitialized(), "note initialized snd::internal::HardwareMgr.");

    if (mCreateFlag)
        return true;

    mCreateFlag = true;
    mIsEnableGetTick = enableGetTick;

    return true;
}

void SoundThread::initialize()
{
}

void SoundThread::finalize()
{
    this->destroy();

    for (auto it = mPlayerCallbackList.robustBegin(); it != mPlayerCallbackList.robustEnd(); ++it)
    {
        PlayerCallback& player = *it;
        player.onShutdownSoundThread();
    }
}

bool SoundThread::createSoundThread(s32 priority, u32 stackSize, u32 affinityMask, bool enableGetTick, sead::Heap* heap)
{
    bool prepareResult = this->prepareForCreate_(enableGetTick);

    if (!prepareResult)
        return false;

    mSoundThreadAffinityMask = affinityMask;

    mOboeCallbackCounter = 0;
    mAudioFrameCounter = 0;

    mThread = new(heap) SoundThreadImpl("snd::SoundThread", heap, priority, stackSize);

    bool result = mThread->start();

    return result;
}

void SoundThread::destroy()
{
    if (!mCreateFlag)
        return;

    SEAD_ASSERT(mThread);

    // Send sound thread end message
    mThread->quit(true);

    // Wait for thread to end
    mThread->waitDone();

    mCreateFlag = false;
}

void SoundThread::frameProcess(UpdateType updateType)
{
    sead::ScopedLock<sead::CriticalSection> lock(&mCriticalSection);

    if (!HardwareMgr::instance())
        return;

/*
    // idk where to put this...
    {
        internal::DriverCommand* cmdMgr = internal::DriverCommand::instance();

        u32 tag = cmdMgr->flushCommand(true);
        cmdMgr->waitCommandReply(tag);
    }
*/

    sead::TickTime beginTick;

    // Sound frame start callback
    {
        for (auto it = mSoundFrameCallbackList.robustBegin(); it != mSoundFrameCallbackList.robustEnd(); ++it)
        {
            SoundFrameCallback& cb = *it;
            cb.onBeginSoundFrame();
        }
    }

    u32 voiceCount = 0;

    // Main process for sound thread
    {
        // Update voice status with SDK result.
        //   MultiVoice::updateVoiceStatus
        //    Check if sound has stopped.
        //    Voice drop check.
        if (updateType == UpdateType::AudioFrame)
            MultiVoiceMgr::instance()->updateAudioFrameVoiceStatus();
        else
            MultiVoiceMgr::instance()->updateAllVoiceStatus();

        // Command process
        {
            while (DriverCommandForTaskThread::instance()->processCommand())
            {
            }

            while (DriverCommand::instance()->processCommand())
            {
            }
        }

        // Update player
        {
            for (auto it = mPlayerCallbackList.robustBegin(); it != mPlayerCallbackList.robustEnd(); ++it)
            {
                PlayerCallback& cb = *it;
                if (updateType == UpdateType::AudioFrame)
                    cb.onUpdateFrameSoundThreadWithAudioFrameFrequency();
                else
                    cb.onUpdateFrameSoundThread();
            }
        }

        // Calculate channel parameters.
        //   Channel::update
        //     Parameter Calculation
        //     Envelope/LFO.
        //     Update parameters in MultiVoice.
        if (updateType == UpdateType::AudioFrame)
            ChannelMgr::instance()->updateAudioFrameChannel();
        else
            ChannelMgr::instance()->updateAllChannel();

        // Calculate voice parameters.
        //   MultiVoice::calc
        //     Parameter Calculation
        //     Update parameters in Voice.
        //   MultiVoice::update
        //     Calculate some parameters.
        //     Update voice status.
        //     Update parameters in Voice.
        if (updateType == UpdateType::AudioFrame)
            MultiVoiceMgr::instance()->updateAudioFrameVoices();
        else
            MultiVoiceMgr::instance()->updateAllVoices();

        if (mVoiceRenderer)
        {
            // Calculate Voice parameters.
            //   VoiceImpl::updateState
            //     Update voice status.
            mVoiceRenderer->updateAllVoices();

            // Synthesize Voice waveforms.
            voiceCount = mVoiceRenderer->synthesize();
        }

        // Update HardwareMgr
        HardwareMgr::instance()->update();
    }

    // Callback for end of sound frame
    {
        for (auto it = mSoundFrameCallbackList.robustBegin(); it != mSoundFrameCallbackList.robustEnd(); ++it)
        {
            SoundFrameCallback& cb = *it;
            cb.onEndSoundFrame();
        }
    }

    // Call the user callback
    if (mUserCallback)
        mUserCallback((void*)mUserCallbackArg);

    //sead::TickTime endTick;

    //SEAD_PRINT("SoundThread took %.4f ms\n", beginTick.diffToNow().toNanoSeconds() / 1000.0f / 1000.0f);
}

void SoundThread::registerSoundFrameCallback(SoundFrameCallback* callback)
{
    SEAD_ASSERT(callback);

    sead::ScopedLock<sead::CriticalSection> lock(&mCriticalSection);
    mSoundFrameCallbackList.pushBack(callback);
}

void SoundThread::unregisterSoundFrameCallback(SoundFrameCallback* callback)
{
    SEAD_ASSERT(callback);

    sead::ScopedLock<sead::CriticalSection> lock(&mCriticalSection);
    mSoundFrameCallbackList.erase(callback);
}

void SoundThread::registerPlayerCallback(PlayerCallback* callback)
{
    SEAD_ASSERT(callback);

    mPlayerCallbackList.pushBack(callback);
}

void SoundThread::unregisterPlayerCallback(PlayerCallback* callback)
{
    SEAD_ASSERT(callback);

    mPlayerCallbackList.erase(callback);
}

void SoundThread::forceWakeup()
{
    SEAD_ASSERT(mThread);

    mThread->sendMessage((sead::MessageQueue::Element)Message::ForceWakeup, sead::MessageQueue::BlockType::eNoBlock);
}

void SoundThread::HwCallbackFunc()
{
    SoundThread::instance()->HwCallbackProc();
}

void SoundThread::HwCallbackProc()
{
    mOboeCallbackCounter++;
    mOboeCallbackCounter &= 0x0fffffff;

    // Send sound thread start message
    if (!mPauseFlag)
    {
        if (!mThread->sendMessage(static_cast<sead::MessageQueue::Element>(Message::HwCallback | mOboeCallbackCounter), sead::MessageQueue::BlockType::eNoBlock))
        {
            //info("Failed to wake up SoundThread");
        }
    }
}

void SoundThread::SoundThreadImpl::run_()
{
    HardwareMgr::registerHwCallback(&SoundThread::HwCallbackFunc);

    while (true)
    {
#ifdef DEBUG
        this->checkStackOverFlow(nullptr, 0);
#endif

        const sead::MessageQueue::Element msg = mMessageQueue.pop(mBlockType);
        //const sead::MessageQueue::Element msg = mMessageQueue.pop(sead::MessageQueue::BlockType::eNoBlock);

        if (msg == mQuitMsg)
            break;

        this->calc_(msg);
    }

    HardwareMgr::deregisterHwCallback(&SoundThread::HwCallbackFunc);
}

void SoundThread::SoundThreadImpl::calc_(sead::MessageQueue::Element msg)
{
    SoundThread* thread = SoundThread::instance();

    Message message = static_cast<Message>(static_cast<u32>(msg) & 0xF0000000);
    u32 messageData = static_cast<u32>(msg) & 0x0FFFFFFF;

    if (message == Message::HwCallback)
    {
        thread->mAudioFrameCounter = messageData;
        thread->frameProcess();
    }
    else if (message == Message::ForceWakeup)
    {
        // Only process commands.
        while (DriverCommand::instance()->processCommand())
        {
        }

        while (DriverCommandForTaskThread::instance()->processCommand())
        {
        }
    }
}

} } } // namespace snd::internal::driver
