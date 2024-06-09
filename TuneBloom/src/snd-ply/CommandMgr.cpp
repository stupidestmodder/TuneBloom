#include "snd/CommandMgr.h"

#include "snd/Command.h"

#include <math/seadMathCalcCommon.h>

namespace snd { namespace internal {

CommandBuffer::CommandBuffer()
    : mBuffer(nullptr)
    , mCommandMemoryArea(nullptr)
    , mCommandMemoryAreaSize(0)
    , mCommandMemoryAreaBegin(0)
    , mCommandMemoryAreaEnd(0)
    , mCommandMemoryAreaZeroFlag(true)
{
}

CommandBuffer::~CommandBuffer()
{
    this->finalize();
}

void CommandBuffer::initialize(u32 commandBufferSize, sead::Heap* heap)
{
    mBuffer = new(heap) u8[commandBufferSize];

    mCommandMemoryAreaBegin = 0;
    mCommandMemoryAreaEnd = 0;
    mCommandMemoryAreaZeroFlag = false;

    mCommandMemoryArea = reinterpret_cast<u32*>(mBuffer);
    mCommandMemoryAreaSize = commandBufferSize / sizeof(mCommandMemoryArea[0]);
}

void CommandBuffer::finalize()
{
    if (mBuffer)
    {
        delete mBuffer;
        mBuffer = nullptr;
    }
}

void* CommandBuffer::allocMemory(u32 size)
{
    SEAD_ASSERT(size >= sizeof(Command));

    void* ptr = nullptr;

    if (mCommandMemoryAreaZeroFlag)
        return nullptr;

    const u32 count = (size + sizeof(mCommandMemoryArea[0]) - 1) / sizeof(mCommandMemoryArea[0]);
    SEAD_ASSERT(count <= mCommandMemoryAreaSize);

    // Avoid calling the `freeMemory` function while running because doing so may change the position of `End`.
    volatile u32 curEnd = mCommandMemoryAreaEnd;

    if (curEnd > mCommandMemoryAreaBegin)
    {
        // If linked regions
        if (mCommandMemoryAreaBegin + count <= curEnd)
        {
            ptr = &mCommandMemoryArea[mCommandMemoryAreaBegin];
            mCommandMemoryAreaBegin += count;
        }
    }
    else
    {
        // If unlinked regions
        if (mCommandMemoryAreaBegin + count <= mCommandMemoryAreaSize)
        {
            ptr = &mCommandMemoryArea[mCommandMemoryAreaBegin];
            mCommandMemoryAreaBegin += count;
        }
        else if (count <= curEnd)
        {
            ptr = &mCommandMemoryArea[0];
            mCommandMemoryAreaBegin = count;
        }
    }

    if (!ptr)
        return nullptr;

    if (mCommandMemoryAreaBegin == curEnd)
        mCommandMemoryAreaZeroFlag = true;

    Command* command = reinterpret_cast<Command*>(ptr);
    SEAD_ASSERT(command != nullptr);

    command->memory_next = mCommandMemoryAreaBegin;

    return command;
}

void CommandBuffer::freeMemory(Command* lastCommand)
{
    mCommandMemoryAreaEnd = lastCommand->memory_next;
    mCommandMemoryAreaZeroFlag = false;
}

u32 CommandBuffer::getAllocatableCommandSize() const
{
    if (mCommandMemoryAreaZeroFlag)
        return 0;

    u32 count;

    if (mCommandMemoryAreaEnd > mCommandMemoryAreaBegin)
    {
        // If linked regions
        count =  mCommandMemoryAreaEnd - mCommandMemoryAreaBegin;
    }
    else
    {
        // If unlinked regions
        count = sead::Mathu::max(mCommandMemoryAreaSize - mCommandMemoryAreaBegin, mCommandMemoryAreaEnd);
    }

    return count * sizeof(mCommandMemoryArea[0]);
}

CommandMgr::CommandMgr()
    : mAvailable(false)
    , mProcessCommandListFunc(nullptr)
    , mRequestProcessCommandFunc(nullptr)
{
}

CommandMgr::~CommandMgr()
{
    this->finalize();
}

void CommandMgr::initialize(u32 commandBufferSize, sead::Heap* heap, ProcessCommandListFunc func)
{
    SEAD_ASSERT(func != nullptr);

    mProcessCommandListFunc = func;
    mRequestProcessCommandFunc = nullptr;

    mCommandBuffer.initialize(commandBufferSize, heap);

    mCommandListBegin = nullptr;
    mCommandListEnd = nullptr;

    mCommandTag = 0;

    mSendCommandQueue.allocate(cSendCommandQueueSize, heap);
    mRecvCommandQueue.allocate(cRecvCommandQueueSize, heap);

    mCommandListCount.setValueNonAtomic(0);

    mAvailable = true;
}

void CommandMgr::finalize()
{
    mSendCommandQueue.free();
    mRecvCommandQueue.free();

    mCommandBuffer.finalize();

    mAvailable = false;
}

u32 CommandMgr::pushCommand(Command* command)
{
    if (!mCommandListEnd)
    {
        mCommandListBegin = command;
        mCommandListEnd = command;
    }
    else
    {
        mCommandListEnd->next = command;
        mCommandListEnd = command;
    }

    command->next = nullptr;

    return mCommandTag;
}

u32 CommandMgr::flushCommand(bool forceFlag)
{
    if (!mCommandListBegin)
    {
        if (!forceFlag)
            return 0;

        // Issues dummy commands
        Command* command = this->allocCommand<Command>();
        command->id = cInvalidCommand;

        this->pushCommand(command);
    }

    sead::MessageQueue::Element msg = reinterpret_cast<sead::MessageQueue::Element>(mCommandListBegin);
    u32 tag = mCommandTag;

    mCommandListCount.increment();

    if (!mSendCommandQueue.push(msg, sead::MessageQueue::BlockType::eNoBlock))
    {
        if (forceFlag)
        {
            if (mRequestProcessCommandFunc)
                mRequestProcessCommandFunc();

            mSendCommandQueue.push(msg, sead::MessageQueue::BlockType::eBlock);
        }
        else
        {
            return 0;
        }
    }

    mCommandListBegin->tag = tag;
    mCommandTag++;

    mCommandListBegin = nullptr;
    mCommandListEnd = nullptr;

    return tag;
}

void CommandMgr::recvCommandReply()
{
    sead::MessageQueue::Element msg = sead::MessageQueue::cNullElement;

    while (msg = mRecvCommandQueue.pop(sead::MessageQueue::BlockType::eNoBlock))
    {
        Command* commandList = reinterpret_cast<Command*>(msg);
        this->finalizeCommandList(commandList);
    }
}

void CommandMgr::waitCommandReply(u32 tag)
{
    while (true)
    {
        sead::MessageQueue::Element msg = mRecvCommandQueue.pop(sead::MessageQueue::BlockType::eNoBlock);

        if (msg == sead::MessageQueue::cNullElement)
        {
            if (mRequestProcessCommandFunc)
                mRequestProcessCommandFunc();

            msg = mRecvCommandQueue.pop(sead::MessageQueue::BlockType::eBlock);
        }

        Command* commandList = reinterpret_cast<Command*>(msg);
        this->finalizeCommandList(commandList);

        if (tag == commandList->tag)
            break;
    }
}

bool CommandMgr::isFinishCommand(u32 tag) const
{
    static const u32 limit = (1 << ((sizeof(tag) * 8) - 2));

    if (tag > mFinishCommandTag)
        return (tag - mFinishCommandTag >= limit);
    else
        return (mFinishCommandTag - tag < limit);
}

bool CommandMgr::processCommand()
{
    sead::MessageQueue::Element msg = mSendCommandQueue.pop(sead::MessageQueue::BlockType::eNoBlock);

    if (msg == sead::MessageQueue::cNullElement)
        return false;

    mCommandListCount.decrement();

    Command* commandList = reinterpret_cast<Command*>(msg);

    if (commandList->id != cInvalidCommand)
        mProcessCommandListFunc(commandList);

    mRecvCommandQueue.push(msg, sead::MessageQueue::BlockType::eBlock);

    return true;
}

void* CommandMgr::allocMemory(u32 size)
{
    void* ptr = this->tryAllocMemory(size);
    if (ptr)
        return ptr;

    this->recvCommandReply();

    ptr = this->tryAllocMemory(size);
    if (ptr)
        return ptr;

    // Forced flush
    while (!ptr)
    {
        if (mCommandListBegin)
        {
            u32 tag = this->flushCommand(true);
            this->waitCommandReply(tag);
        }
        else
        {
            if (mRequestProcessCommandFunc)
                mRequestProcessCommandFunc();

            this->recvCommandReply();
        }

        ptr = this->tryAllocMemory(size);
    }

    return ptr;
}

void* CommandMgr::tryAllocMemory(u32 size)
{
    return mCommandBuffer.allocMemory(size);
}

void CommandMgr::finalizeCommandList(Command* commandList)
{
    mFinishCommandTag = commandList->tag;

    Command* command = commandList;

    while (command)
    {
        if (!command->next)
        {
            mCommandBuffer.freeMemory(command);
            break;
        }

        command = command->next;
    }
}

} } // namespace snd::internal
