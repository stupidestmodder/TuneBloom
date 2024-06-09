#pragma once

#include <container/seadOffsetList.h>

namespace snd { namespace internal { namespace driver {

class DisposeCallback
{
public:
    virtual ~DisposeCallback()
    {
    }

    virtual void invalidateData(const void* start, const void* end) = 0;

private:
    friend class DisposeCallbackMgr;

    sead::ListNode mListNode;
};

} } } // namespace snd::internal::driver
