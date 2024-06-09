#pragma once

#include <container/seadOffsetList.h>
#include <heap/seadDisposer.h>

namespace snd { namespace internal { namespace driver {

class DisposeCallback;

class DisposeCallbackMgr
{
    SEAD_SINGLETON_DISPOSER(DisposeCallbackMgr);

private:
    DisposeCallbackMgr();
    ~DisposeCallbackMgr();

public:
    void dispose(const void* mem, u32 size);

    void registerDisposeCallback(DisposeCallback* callback);
    void unregisterDisposeCallback(DisposeCallback* callback);

    u32 getCallbackCount() const
    {
        return mCallbackList.size();
    }

private:
    typedef sead::OffsetList<DisposeCallback> CallbackList;

    CallbackList mCallbackList;
};

} } } // namespace snd::internal::driver
