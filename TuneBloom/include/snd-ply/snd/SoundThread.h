#pragma once

#include <container/seadOffsetList.h>
#include <heap/seadDisposer.h>
#include <thread/seadThread.h>

#include "Global.h"

namespace snd { namespace internal {

class VoiceRenderer;

namespace driver {

class SoundThread
{
    SEAD_SINGLETON_DISPOSER(SoundThread);

public:
    class SoundFrameCallback
    {
    public:
        virtual ~SoundFrameCallback()
        {
        }

        virtual void onBeginSoundFrame()
        {
        }
        virtual void onEndSoundFrame()
        {
        }

    private:
        friend class SoundThread;

        sead::ListNode mListNode;
    };

    class PlayerCallback
    {
    public:
        virtual ~PlayerCallback()
        {
        }

        virtual void onUpdateFrameSoundThread()
        {
        }
        virtual void onUpdateFrameSoundThreadWithAudioFrameFrequency()
        {
        }
        virtual void onShutdownSoundThread()
        {
        }

    private:
        friend class SoundThread;

        sead::ListNode mListNode;
    };

private:
    class SoundThreadImpl : public sead::Thread
    {
    public:
        SoundThreadImpl(const sead::SafeString& name, sead::Heap* heap, s32 priority, s32 stackSize)
            : Thread(name, heap, priority,
                sead::MessageQueue::BlockType::eBlock,
                (sead::MessageQueue::Element)Message::Shutdown,
                 stackSize, cThreadMgsBufferSize)
        {
        }

    private:
        void run_() override;
        void calc_(sead::MessageQueue::Element msg);
    };

    enum Message
    {
        HwCallback = 0x10000000,
        Shutdown = 0x20000000,
        ForceWakeup = 0x30000000
    };

    static const u32 cThreadMgsBufferSize = 64;

private:
    SoundThread();
    ~SoundThread();

public:
    void initialize();
    void finalize();

    bool createSoundThread(s32 priority, u32 stackSize, u32 affinityMask, bool enableGetTick, sead::Heap* heap);
    void destroy();

    bool isCreated() const
    {
        return mCreateFlag;
    }

    void detail_setVoiceRenderer(VoiceRenderer* voiceRenderer)
    {
        mVoiceRenderer = voiceRenderer;
    }

    VoiceRenderer* detail_getVoiceRenderer() const
    {
        return mVoiceRenderer;
    }

    void pause(bool pauseFlag)
    {
        mPauseFlag = pauseFlag;
    }

    u32 getFrameCounter() const
    {
        return mAudioFrameCounter;
    }

    void frameProcess(UpdateType updateType = UpdateType::AudioFrame);

    void registerSoundFrameUserCallback(SoundFrameUserCallback callback, void* arg)
    {
        mUserCallbackArg = arg;
        mUserCallback = callback;
    }

    void clearSoundFrameUserCallback()
    {
        mUserCallbackArg = nullptr;
        mUserCallback = nullptr;
    }

    void registerSoundFrameCallback(SoundFrameCallback* callback);
    void unregisterSoundFrameCallback(SoundFrameCallback* callback);

    void registerPlayerCallback(PlayerCallback* callback);
    void unregisterPlayerCallback(PlayerCallback* callback);

    void lock()
    {
        mCriticalSection.lock();
    }

    void unlock()
    {
        mCriticalSection.unlock();
    }

    void forceWakeup();

private:
    bool prepareForCreate_(bool enableGetTick);

    static void HwCallbackFunc();
    void HwCallbackProc();

private:
    friend class SoundThreadImpl;

    SoundThreadImpl* mThread;

    u32 mAudioFrameCounter;
    u32 mOboeCallbackCounter;

    mutable sead::CriticalSection mCriticalSection;

    typedef sead::OffsetList<SoundFrameCallback> SoundFrameCallbackList;
    typedef sead::OffsetList<PlayerCallback> PlayerCallbackList;

    SoundFrameCallbackList mSoundFrameCallbackList;
    PlayerCallbackList mPlayerCallbackList;

    volatile SoundFrameUserCallback mUserCallback;
    volatile void* mUserCallbackArg;

    s32 mSoundThreadAffinityMask;
    bool mCreateFlag;
    bool mPauseFlag;
    bool mIsEnableGetTick;

    VoiceRenderer* mVoiceRenderer;
};

class SoundThreadLock
{
    SEAD_NO_COPY(SoundThreadLock);

public:
    SoundThreadLock()
    {
        SEAD_ASSERT(SoundThread::instance());

        SoundThread::instance()->lock();
    }

    ~SoundThreadLock()
    {
        SEAD_ASSERT(SoundThread::instance());

        SoundThread::instance()->unlock();
    }
};

} } } // namespace snd::internal::driver
