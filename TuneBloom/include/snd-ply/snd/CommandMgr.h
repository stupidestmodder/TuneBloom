#pragma once

#include <thread/seadMessageQueue.h>
#include <thread/seadAtomic.h>

namespace snd { namespace internal {

class Command;

class CommandBuffer
{
public:
    CommandBuffer();
    ~CommandBuffer();

    void initialize(u32 commandBufferSize, sead::Heap* heap);
    void finalize();

    void* allocMemory(u32 size);
    void freeMemory(Command* lastCommand);

    u32 getAllocatableCommandSize() const;

private:
    u8* mBuffer;

    u32* mCommandMemoryArea;
    u32 mCommandMemoryAreaSize;

    u32 mCommandMemoryAreaBegin;
    u32 mCommandMemoryAreaEnd;

    // Flag to determine whether the area size is zero or the maximum when m_CommandMemoryAreaBegin == m_CommandMemoryAreaBegin.
    bool mCommandMemoryAreaZeroFlag;
};

class CommandMgr
{
public:
    // Implementation function for command processing
    typedef void (*ProcessCommandListFunc)(Command* commandList);

    // Function that requests command processing
    typedef void (*RequestProcessCommandFunc)();

public:
    CommandMgr();
    virtual ~CommandMgr();

    bool isAvailable() const
    {
        return mAvailable;
    }

    void initialize(u32 commandBufferSize, sead::Heap* heap, ProcessCommandListFunc func);
    void finalize();

    void setRequestProcessCommandFunc(RequestProcessCommandFunc func)
    {
        mRequestProcessCommandFunc = func;
    }

    template <typename CommandType>
    CommandType* allocCommand()
    {
        return reinterpret_cast<CommandType*>(this->allocMemory(sizeof(CommandType)));
    }

    u32 pushCommand(Command* command);

    u32 flushCommand(bool forceFlag);
    void recvCommandReply();

    void waitCommandReply(u32 tag);
    bool isFinishCommand(u32 tag) const;

    u32 getAllocatableCommandSize() const
    {
        return mCommandBuffer.getAllocatableCommandSize();
    }

    u32 getCommandListCount() const
    {
        return static_cast<const u32>(mCommandListCount.getValue());
    }

    bool processCommand();

private:
    static const int cSendCommandQueueSize = 32;
    static const int cRecvCommandQueueSize = cSendCommandQueueSize + 1;

    static const u32 cInvalidCommand = 0xFFFFFFFF;

    void* allocMemory(u32 size);
    void* tryAllocMemory(u32 size);
    void finalizeCommandList(Command* command);

private:
    bool mAvailable;
    ProcessCommandListFunc mProcessCommandListFunc;
    RequestProcessCommandFunc mRequestProcessCommandFunc;

    sead::MessageQueue mSendCommandQueue;
    sead::MessageQueue mRecvCommandQueue;

    Command* mCommandListBegin;
    Command* mCommandListEnd;

    sead::AtomicU32 mCommandListCount;

    u32 mCommandTag;
    u32 mFinishCommandTag;

    CommandBuffer mCommandBuffer;
};

} } // namespace snd::internal
