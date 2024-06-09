#pragma once

#include <container/seadOffsetList.h>
#include <heap/seadDisposer.h>

#include "MultiVoice.h"

namespace snd { namespace internal { namespace driver {

class MultiVoiceMgr
{
    SEAD_SINGLETON_DISPOSER(MultiVoiceMgr);

public:
    typedef sead::OffsetList<MultiVoice> VoiceList;

private:
    MultiVoiceMgr();
    ~MultiVoiceMgr();

public:
    void initialize(u32 voiceCount, sead::Heap* heap);
    void finalize();

    MultiVoice* allocVoice(s32 voiceChannelCount, u32 priority, MultiVoice::VoiceCallback callback, void* callbackData);
    void freeVoice(MultiVoice* voice);

    void stopAllVoices();

    void updateAllVoices();
    void updateAudioFrameVoices();

    void updateAllVoiceStatus();
    void updateAudioFrameVoiceStatus();

    void updateAllVoicesSync(u32 syncFlag);

    s32 getVoiceCount() const;
    u32 getActiveCount() const;
    u32 getFreeCount() const;

    const VoiceList& getVoiceList() const
    {
        return mPrioVoiceList;
    }

private:
    friend class MultiVoice;

    void changeVoicePriority(MultiVoice* voice);
    s32 dropLowestPriorityVoice(u32 priority);
    //void updateEachVoicePriority(const VoiceList::iterator& beginItr, const VoiceList::iterator& endItr);

    void appendVoiceList(MultiVoice* voice);
    void removeVoiceList(MultiVoice* voice);

private:
    bool mInitialized;
    VoiceList mPrioVoiceList; // lower priority order
    VoiceList mFreeVoiceList;
};

} } } // namespace snd::internal::driver
