#include "snd/InstancePool.h"

#include <math/seadMathCalcCommon.h>
#include <prim/seadPtrUtil.h>

namespace snd { namespace internal {

u32 PoolImpl::createImpl(void* buffer, size_t size, u32 objSize)
{
    SEAD_ASSERT(buffer != nullptr);

    char* ptr = (char*)sead::PtrUtil::roundUpPow2(buffer, 4);
    objSize = sead::Mathu::roundUpPow2(objSize, 4);

    u32 numObjects = (size - (ptr - static_cast<char*>(buffer))) / objSize;

    for (u32 i = 0; i < numObjects; i++)
    {
        PoolImpl* head = reinterpret_cast<PoolImpl*>(ptr);
        head->mNext = mNext;

        mNext = head;

        ptr += objSize;
    }

    mBuffer = buffer;
    mBufferSize = size;

    return numObjects;
}

void PoolImpl::destroyImpl()
{
    SEAD_ASSERT(mBuffer != nullptr);

    void* begin = mBuffer;
    void* end = static_cast<void*>(static_cast<char*>(mBuffer ) + mBufferSize);

    PoolImpl* ptr = mNext;
    PoolImpl* prev = this;

    while (ptr)
    {
        if ((begin <= ptr) && (ptr < end))
            prev->mNext = ptr->mNext;
        else
            prev = ptr;

        ptr = ptr->mNext;
    }
}

s32 PoolImpl::countImpl() const
{
    s32 count = 0;

    for (PoolImpl* ptr = mNext; ptr != nullptr; ptr = ptr->mNext )
    {
        count++;
    }

    return count;
}

void* PoolImpl::allocImpl()
{
    if (!mNext)
        return nullptr;

    PoolImpl* head = mNext;
    mNext = head->mNext;

    return head;
}

void PoolImpl::freeImpl(void* ptr)
{
    PoolImpl* head = reinterpret_cast<PoolImpl*>(ptr);
    head->mNext = mNext;

    mNext = head;
}

} } // namespace snd::internal
