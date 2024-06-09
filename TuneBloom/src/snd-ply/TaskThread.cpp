#include "snd/TaskThread.h"

#include "snd/TaskMgr.h"
#include "snd/DriverCommand.h"

#include <basis/seadWarning.h>
#include <prim/seadScopedLock.h>

namespace snd { namespace internal {

SEAD_SINGLETON_DISPOSER_IMPL(TaskThread);

TaskThread::TaskThread()
    : mThread(nullptr)
    , mIsFinished(false)
    , mIsCreated(false)
{
}

TaskThread::~TaskThread()
{
    if (mIsCreated)
        this->destroy();

    if (mThread)
    {
        delete mThread;
        mThread = nullptr;
    }
}

bool TaskThread::create(s32 priority, u32 stackSize, u32 affinityMask, sead::Heap* heap)
{
    // Multiple initialization check
    if (mIsCreated)
        this->destroy();

    mIsFinished = false;

    mThread = new(heap) TaskThreadImpl("snd::TaskThread", heap, priority, stackSize);

    bool result = mThread->start();

    if (!result)
    {
        SEAD_WARNING("snd::TaskThread::create Failed");
        return false;
    }

    mIsCreated = true;

    return true;
}

void TaskThread::destroy()
{
    if (!mIsCreated)
        return;

    // Send sound thread end message
    mIsFinished = true;
    TaskMgr::instance()->cancelWaitTask();

    // Wait for thread to end
    mThread->waitDone();

    mIsCreated = false;
}

void TaskThread::TaskThreadImpl::run_()
{
    TaskThread* thread = TaskThread::instance();

    while (!thread->mIsFinished)
    {
        TaskMgr::instance()->waitTask();

        if (thread->mIsFinished)
            break;

        {
            // NOTE: Lock on task thread.
            //
            // Stop the thread by locking it from outside of the thread with the SoundSystem::(try)LockDataLoadThread function.
            //
            // The task thread loads stream data and loads data to the player heap. This lock was added to conform to the restriction to prohibit access to files during Sleep mode.

            sead::ScopedLock<sead::CriticalSection> lock(&thread->mCriticalSection);
            TaskMgr::instance()->executeTask();
        }

        DriverCommandForTaskThread::instance()->recvCommandReply();
    }
}

void TaskThread::TaskThreadImpl::calc_(sead::MessageQueue::Element msg)
{
}

} } // namespace snd::internal
