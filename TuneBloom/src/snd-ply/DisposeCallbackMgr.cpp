#include "snd/DisposeCallbackMgr.h"

#include "snd/DisposeCallback.h"
#include "snd/Macro.h"

namespace snd { namespace internal { namespace driver {

SEAD_SINGLETON_DISPOSER_IMPL(DisposeCallbackMgr);

DisposeCallbackMgr::DisposeCallbackMgr()
{
    mCallbackList.initOffset(offsetof(DisposeCallback, mListNode));
}

DisposeCallbackMgr::~DisposeCallbackMgr()
{
    mCallbackList.clear();
}

void DisposeCallbackMgr::dispose(const void* mem, u32 size)
{
    const void* start = mem;
    const void* end = static_cast<const u8*>(mem) + size;

    for (auto it = mCallbackList.robustBegin(); it != mCallbackList.robustEnd(); ++it)
    {
        it->invalidateData(start, end);
    }
}

void DisposeCallbackMgr::registerDisposeCallback(DisposeCallback* callback)
{
    mCallbackList.pushBack(callback);
}

void DisposeCallbackMgr::unregisterDisposeCallback(DisposeCallback* callback)
{
    mCallbackList.erase(callback);
}

} } } // namespace snd::internal::driver
