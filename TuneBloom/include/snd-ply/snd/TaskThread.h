#pragma once

#include <heap/seadDisposer.h>
#include <thread/seadThread.h>

namespace snd { namespace internal {

class TaskThread
{
    SEAD_SINGLETON_DISPOSER(TaskThread);

private:
    class TaskThreadImpl : public sead::Thread
    {
    public:
        TaskThreadImpl(const sead::SafeString& name, sead::Heap* heap, s32 priority, s32 stackSize)
            : Thread(name, heap, priority, sead::MessageQueue::BlockType::eBlock, 0x7FFFFFFF, stackSize, 1)
        {
        }

    private:
        void run_() override;
        void calc_(sead::MessageQueue::Element msg);
    };

private:
    TaskThread();
    ~TaskThread();

public:
    bool create(s32 priority, u32 stackSize, u32 affinityMask, sead::Heap* heap);
    void destroy();

    bool isCreated() const
    {
        return mIsCreated;
    }

    void lock()
    {
        mCriticalSection.lock();
    }

    void unlock()
    {
        mCriticalSection.unlock();
    }

private:
    friend class TaskThreadImpl;

    TaskThreadImpl* mThread;
    sead::CriticalSection mCriticalSection;
    volatile bool mIsFinished;
    bool mIsCreated;
};

} } // namespace snd::internal
