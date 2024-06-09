#pragma once

#include <basis/seadNew.h>

namespace snd { namespace internal {

class PoolImpl
{
public:
    PoolImpl()
        : mNext(nullptr)
        , mBuffer(nullptr)
        , mBufferSize(0)
    {
    }

protected:
    u32 createImpl(void* buffer, size_t size, u32 objSize);
    void destroyImpl();

    s32 countImpl() const;

    void* allocImpl();
    void freeImpl(void* ptr);

private:
    PoolImpl* mNext;
    void* mBuffer;
    size_t mBufferSize;
};

template <typename T>
class InstancePool : private PoolImpl
{
public:
    u32 create(void* buffer, u32 size)
    {
        u32 objSize = sizeof(T) > sizeof(InstancePool<T>*) ? sizeof(T) : sizeof(InstancePool<T>*);

        return this->createImpl(buffer, size, objSize);
    }

    void destroy()
    {
        this->destroyImpl();
    }

    s32 count() const
    {
        return this->countImpl();
    }

    T* alloc()
    {
        void* ptr = this->allocImpl();
        if (!ptr)
            return nullptr;

        return new(ptr) T(); // Constructor called
    }

    void free(T* obj)
    {
        if (!obj)
            return;

        obj->~T(); // Destructor called

        this->freeImpl(obj);
    }
};

} } // namespace snd::internal
