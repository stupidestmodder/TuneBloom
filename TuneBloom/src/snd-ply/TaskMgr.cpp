#include "snd/TaskMgr.h"

#include "snd/Task.h"
#include "snd/Macro.h"

#include <prim/seadScopedLock.h>

namespace snd { namespace internal {

SEAD_SINGLETON_DISPOSER_IMPL(TaskMgr);

TaskMgr::TaskMgr()
    : mCurrentTask(nullptr)
    , mIsWaitTaskCancel(false)
{
    for (u32 i = 0; i < cPriorityNum; i++)
    {
        mTaskList[i].initOffset(offsetof(Task, mListNode));
    }
}

TaskMgr::~TaskMgr()
{
    for (u32 i = 0; i < cPriorityNum; i++)
    {
        mTaskList[i].clear();
    }
}

void TaskMgr::initialize(sead::Heap* heap)
{
    mBlockingQueue.allocate(cThreadMessageBufSize, heap);
}

void TaskMgr::finalize()
{
    mBlockingQueue.free();
}

void TaskMgr::executeTask()
{
    while (true)
    {
        Task* task = this->popTask();

        if (!task)
            break;

        mCurrentTask = task;

        task->execute();
        task->mStatus = Task::Status::Done;
        task->mEvent.setSignal();

        mCurrentTask = nullptr;
    }
}

void TaskMgr::appendTask(Task* task, TaskPriority priority)
{
    SEAD_ASSERT((u32)priority >= 0 && (u32)priority < cPriorityNum);

    task->mEvent.resetSignal();
    task->mStatus = Task::Status::Append;

    {
        sead::ScopedLock<sead::CriticalSection> scopeLock(&mCriticalSection);
        mTaskList[(u32)priority].pushBack(task);
    }

    mBlockingQueue.push(static_cast<sead::MessageQueue::Element>(Message::Append), sead::MessageQueue::BlockType::eNoBlock);
}

void TaskMgr::cancelTask(Task* task)
{
    if (this->tryRemoveTask(task))
        return;

    task->mEvent.wait();
}

void TaskMgr::cancelTaskById(u32 id)
{
    this->removeTaskById(id);
}

void TaskMgr::cancelAllTask()
{
    // TODO
}

void TaskMgr::waitTask()
{
    mIsWaitTaskCancel = false;

    while (this->getNextTask() == nullptr && !mIsWaitTaskCancel)
    {
        sead::MessageQueue::Element msg = mBlockingQueue.pop(sead::MessageQueue::BlockType::eBlock);

        if (msg != sead::MessageQueue::cNullElement)
        {
            if (msg == static_cast<sead::MessageQueue::Element>(Message::Append))
                break;
        }
    }
}

void TaskMgr::cancelWaitTask()
{
    mIsWaitTaskCancel = true;

    mBlockingQueue.push(static_cast<sead::MessageQueue::Element>(Message::Append), sead::MessageQueue::BlockType::eBlock);
}

bool TaskMgr::tryRemoveTask(Task* task)
{
    sead::ScopedLock<sead::CriticalSection> scopeLock(&mCriticalSection);

    for (u32 i = 0; i < cPriorityNum; i++)
    {
        for (auto it = mTaskList[i].robustBegin(); it != mTaskList[i].robustEnd(); ++it)
        {
            Task& curTask = *it;
            if (&curTask == task)
            {
                mTaskList[i].erase(&curTask);
                curTask.mStatus = Task::Status::Cancel;
                curTask.mEvent.setSignal();

                return true;
            }
        }
    }

    return false;
}

Task* TaskMgr::popTask()
{
    sead::ScopedLock<sead::CriticalSection> scopeLock(&mCriticalSection);

    Task* task = this->getNextTask(TaskPriority::High, true);
    if (task)
        return task;

    task = this->getNextTask(TaskPriority::Middle, true);
    if (task)
        return task;

    task = this->getNextTask(TaskPriority::Low, true);
    if (task)
        return task;

    return nullptr;
}

void TaskMgr::removeTaskById(u32 id)
{
    sead::ScopedLock<sead::CriticalSection> scopeLock(&mCriticalSection);

    for (u32 i = 0; i < cPriorityNum; i++)
    {
        for (auto it = mTaskList[i].robustBegin(); it != mTaskList[i].robustEnd(); ++it)
        {
            Task& curTask = *it;
            if (curTask.mId == id)
            {
                mTaskList[i].erase(&curTask);
                curTask.mStatus = Task::Status::Cancel;
                curTask.mEvent.setSignal();
            }
        }
    }
}

Task* TaskMgr::getNextTask()
{
    Task* task = this->getNextTask(TaskPriority::High, false);
    if (task)
        return task;

    task = this->getNextTask(TaskPriority::Middle, false);
    if (task)
        return task;

    task = this->getNextTask(TaskPriority::Low, false);
    if (task)
        return task;

    return nullptr;
}

Task* TaskMgr::getNextTask(TaskPriority priority, bool doRemove)
{
    SEAD_ASSERT((u32)priority >= 0 && (u32)priority < cPriorityNum);

    sead::ScopedLock<sead::CriticalSection> scopeLock(&mCriticalSection);

    if (mTaskList[(u32)priority].isEmpty())
        return nullptr;

    Task* task = mTaskList[(u32)priority].front();

    if (doRemove)
        mTaskList[(u32)priority].popFront();

    return task;
}

} } // namespace snd::internal
