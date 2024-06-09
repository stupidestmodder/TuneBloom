#pragma once

#include <heap/seadDisposer.h>
#include <container/seadOffsetList.h>

#include "InstancePool.h"

namespace snd { namespace internal { namespace driver {

class Channel;

class ChannelMgr
{
    SEAD_SINGLETON_DISPOSER(ChannelMgr);

private:
    ChannelMgr();
    ~ChannelMgr();

public:
    void initialize(u32 channelCount, sead::Heap* heap);
    void finalize();

    Channel* alloc();
    void free(Channel* channel);

    void updateAllChannel();
    void updateAudioFrameChannel();

    s32 getChannelCount()
    {
        return mChannelList.size();
    }

private:
    typedef InstancePool<Channel> ChannelPool;
    typedef sead::OffsetList<Channel> ChannelList;

    ChannelPool mPool;
    ChannelList mChannelList;
    bool mIsInitialized;
    u32 mChannelCount;
};

} } } // namespace snd::internal::driver
