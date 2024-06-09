#pragma once

#include <container/seadSafeArray.h>
#include <thread/seadAtomic.h>

// Lock-Free RingBuffer
template <typename T, u32 Size>
class LFRingBuffer
{
    SEAD_NO_COPY(LFRingBuffer);

public:
    LFRingBuffer()
        : mRingBuffer()
        , mReadPosition(0)
        , mWritePosition(0)
    {
    }

    bool push(const T& element)
    {
        const u32 oldWritePosition = mWritePosition.getValue();
        const u32 newWritePosition = getPositionAfter(oldWritePosition);
        const u32 readPosition = mReadPosition.getValue();

        if (newWritePosition == readPosition)
        {
            // The queue is full
            return false;
        }

        mRingBuffer[oldWritePosition] = element;
        mWritePosition.setValue(newWritePosition);

        return true;
    }

    bool pop(T* element)
    {
        if (isEmpty())
        {
            // The queue is empty
            return false;
        }

        SEAD_ASSERT(element);

        const u32 readPosition = mReadPosition.getValue();
        *element = std::move(mRingBuffer[readPosition]);
        mReadPosition.setValue(getPositionAfter(readPosition));

        return true;
    }

    void clear()
    {
        const u32 readPosition = mReadPosition.getValue();
        const u32 writePosition = mWritePosition.getValue();

        if (readPosition != writePosition)
            mReadPosition.setValue(writePosition);
    }

    u32 size() const
    {
        const u32 readPosition = mReadPosition.getValue();
        const u32 writePosition = mWritePosition.getValue();

        if (readPosition == writePosition)
            return 0;

        if (writePosition < readPosition)
            return cRingBufferSize - readPosition + writePosition;
        else
            return writePosition - readPosition;
    }

    bool isEmpty() const
    {
        const u32 readPosition = mReadPosition.getValue();
        const u32 writePosition = mWritePosition.getValue();

        if (readPosition == writePosition)
            return true;

        return false;
    }

    constexpr u32 maxSize() const
    {
        return cRingBufferSize - 1;
    }

    static constexpr u32 getPositionAfter(u32 pos)
    {
        return pos + 1 == cRingBufferSize ? 0 : pos + 1;
    }

private:
    static const u32 cRingBufferSize = Size + 1;

    sead::SafeArray<T, cRingBufferSize> mRingBuffer;
    sead::AtomicU32 mReadPosition;
    sead::AtomicU32 mWritePosition;
};
