#pragma once

#include <container/seadOffsetList.h>
#include <thread/seadEvent.h>

namespace snd { namespace internal {

class Task
{
    SEAD_NO_COPY(Task);

public:
    enum class Status
    {
        Free,
        Append,
        Execute,
        Done,
        Cancel
    };

public:
    Task()
        : mEvent(true)
        , mStatus(Status::Free)
        , mId(0)
    {
        mEvent.setSignal();
    }

    virtual ~Task()
    {
        SEAD_ASSERT(mStatus != Status::Append && mStatus != Status::Execute);
    }

protected:
    // Implement the task process.
    // NOTE: Do not release the Task instance within the execute function.
    virtual void execute() = 0;

    void initializeStatus()
    {
        mStatus = Status::Free;
    }

public:
    void setId(u32 id)
    {
        mId = id;
    }

    Status getStatus() const
    {
        return mStatus;
    }

    void wait()
    {
        mEvent.wait();
    }

    bool tryWait()
    {
        return mEvent.wait(sead::TickSpan::makeFromSeconds(0));
    }

private:
    friend class TaskMgr;

    sead::ListNode mListNode;
    sead::Event mEvent;
    Status mStatus;
    u32 mId;
};

} } // namespace snd::internal
