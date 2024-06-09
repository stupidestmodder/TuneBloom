#pragma once

#include <container/seadOffsetList.h>
#include <thread/seadCriticalSection.h>
#include <thread/seadMessageQueue.h>

namespace snd { namespace internal {

class Task;

class TaskMgr
{
    SEAD_SINGLETON_DISPOSER(TaskMgr);

public:
    enum class TaskPriority
    {
        Low,
        Middle,
        High
    };

private:
    TaskMgr();
    ~TaskMgr();

public:
    void initialize(sead::Heap* heap);
    void finalize();

    void executeTask();
    void appendTask(Task* task, TaskPriority priority);
    void cancelTask(Task* task);
    void cancelTaskById(u32 id);
    void cancelAllTask();

    void waitTask();
    void cancelWaitTask();
    bool tryRemoveTask(Task* task);

private:
    static const u32 cPriorityNum = (u32)TaskPriority::High - (u32)TaskPriority::Low + 1;
    static const u32 cThreadMessageBufSize = 32;

    enum class Message
    {
        Append = 1
    };

    Task* popTask();
    void removeTaskById(u32 id);

    Task* getNextTask();
    Task* getNextTask(TaskPriority priority, bool doRemove);

private:
    typedef sead::OffsetList<Task> TaskList;

    TaskList mTaskList[cPriorityNum];

    Task* volatile mCurrentTask;

    volatile bool mIsWaitTaskCancel;

    sead::CriticalSection mCriticalSection;
    sead::MessageQueue mBlockingQueue;
};

} } // namespace snd::internal
