#include "snd/ChannelMgr.h"

#include "snd/Channel.h"
#include "snd/Macro.h"

namespace snd { namespace internal { namespace driver {

SEAD_SINGLETON_DISPOSER_IMPL(ChannelMgr);

ChannelMgr::ChannelMgr()
    : mIsInitialized(false)
    , mChannelCount(0)
{
    mChannelList.initOffset(offsetof(Channel, mListNode));
}

ChannelMgr::~ChannelMgr()
{
    mChannelList.clear();
}

void ChannelMgr::initialize(u32 channelCount, sead::Heap* heap)
{
    if (mIsInitialized)
        return;

    u32 memSize = sizeof(Channel) * (channelCount * 2);
    void* mem = new(heap) u8[memSize];

    mChannelCount = mPool.create(mem, memSize);

    mIsInitialized = true;
}

void ChannelMgr::finalize()
{
    if (!mIsInitialized)
        return;

    // Stop all voices in the list
    for (auto it = mChannelList.robustBegin(); it != mChannelList.robustEnd(); ++it)
    {
        Channel& channel = (*it);

        channel.stop();
        channel.callChannelCallback(Channel::ChannelCallbackStatus::Cancel);

        this->free(&channel);
    }

    SEAD_ASSERT(mChannelList.isEmpty());

    mPool.destroy();

    mIsInitialized = false;
}

Channel* ChannelMgr::alloc()
{
    Channel* channel = mPool.alloc();
    if (!channel)
        return nullptr;

    mChannelList.pushBack(channel);

    return channel;
}

void ChannelMgr::free(Channel* channel)
{
    mChannelList.erase(channel);
    mPool.free(channel);
}

void ChannelMgr::updateAllChannel()
{
    for (auto it = mChannelList.robustBegin(); it != mChannelList.robustEnd(); ++it)
    {
        Channel& channel = (*it);

        channel.update(true);

        if (!channel.isActive())
        {
            channel.callChannelCallback(Channel::ChannelCallbackStatus::Stopped);
            this->free(&channel);
        }
    }
}

void ChannelMgr::updateAudioFrameChannel()
{
    for (auto it = mChannelList.robustBegin(); it != mChannelList.robustEnd(); ++it)
    {
        Channel& channel = (*it);

        if (channel.isActive() && channel.getUpdateType() == UpdateType::AudioFrame)
            channel.update(true);

        if (!channel.isActive())
        {
            channel.callChannelCallback(Channel::ChannelCallbackStatus::Stopped);
            this->free(&channel);
        }
    }
}

} } } // namespace snd::internal::driver
