#include "snd/MultiVoiceMgr.h"

#include "snd/SoundThread.h"
#include "snd/Macro.h"

namespace snd { namespace internal { namespace driver {

SEAD_SINGLETON_DISPOSER_IMPL(MultiVoiceMgr);

MultiVoiceMgr::MultiVoiceMgr()
    : mInitialized(false)
{
    mPrioVoiceList.initOffset(offsetof(MultiVoice, mListNode));
    mFreeVoiceList.initOffset(offsetof(MultiVoice, mListNode));
}

MultiVoiceMgr::~MultiVoiceMgr()
{
    mPrioVoiceList.clear();
    mFreeVoiceList.clear();
}

void MultiVoiceMgr::initialize(u32 voiceCount, sead::Heap* heap)
{
    if (mInitialized)
        return;

    for (u32 i = 0; i < voiceCount; i++)
    {
        MultiVoice* voice = new(heap) MultiVoice();
        //MultiVoice* voice = (MultiVoice*)malloc(sizeof(MultiVoice));
        //new(voice) MultiVoice();

        mFreeVoiceList.pushBack(voice);
    }

    mInitialized = true;
}

void MultiVoiceMgr::finalize()
{
    if (!mInitialized)
        return;

    this->stopAllVoices();

    // Delete voice from the list
    while (!mFreeVoiceList.isEmpty())
    {
        MultiVoice* voice = mFreeVoiceList.front();
        mFreeVoiceList.popFront();

        delete voice;
    }

    mInitialized = false;
}

MultiVoice* MultiVoiceMgr::allocVoice(s32 voiceChannelCount, u32 priority, MultiVoice::VoiceCallback callback, void* callbackData)
{
    // Stops low priority voice when there's shortage of voice.
    if (mFreeVoiceList.isEmpty())
    {
        if (this->dropLowestPriorityVoice(priority) == 0)
            return nullptr;
    }

    // Gets an available voice from the free list
    MultiVoice* voice = mFreeVoiceList.front();

    if (!voice->alloc(voiceChannelCount, priority, callback, callbackData))
        return nullptr;

    // Registers the priority and adds to the voice list
    voice->mPriority = priority;
    mFreeVoiceList.erase(voice);

    this->appendVoiceList(voice);

    return voice;
}

void MultiVoiceMgr::freeVoice(MultiVoice* voice)
{
    SEAD_ASSERT(voice != nullptr);

    this->removeVoiceList(voice);
}

void MultiVoiceMgr::stopAllVoices()
{
    // Free all the playing voices
    while (!mPrioVoiceList.isEmpty())
    {
        MultiVoice* voice = mPrioVoiceList.front();
        voice->stop();

        if (voice->mCallback)
            voice->mCallback(voice, MultiVoice::VoiceCallbackStatus::Cancel, voice->mCallbackData);

        voice->free();
    }
}

void MultiVoiceMgr::updateAllVoices()
{
    // Calculate parameter for all voices.
    for (auto it = mPrioVoiceList.robustBegin(); it != mPrioVoiceList.robustEnd(); ++it)
    {
        MultiVoice& voice = *it;
        voice.calc();
    }

    // Update all voices.
    for (auto it = mPrioVoiceList.robustBegin(); it != mPrioVoiceList.robustEnd(); ++it)
    {
        MultiVoice& voice = *it;
        voice.update();
    }
}

void MultiVoiceMgr::updateAudioFrameVoices()
{
    // Update frequency updates audio frame voice.
    for (auto it = mPrioVoiceList.robustBegin(); it != mPrioVoiceList.robustEnd(); ++it)
    {
        MultiVoice& voice = *it;
        if (voice.getUpdateType() == UpdateType::AudioFrame)
            voice.calc();
    }

    // Update frequency updates audio frame voice.
    for (auto it = mPrioVoiceList.robustBegin(); it != mPrioVoiceList.robustEnd(); ++it)
    {
        MultiVoice& voice = *it;
        if (voice.getUpdateType() == UpdateType::AudioFrame)
            voice.update();
    }
}

void MultiVoiceMgr::updateAllVoiceStatus()
{
    // Apply voice-drop effect for end of waveform or to prevent DSP slowdowns.
    for (auto it = mPrioVoiceList.robustBegin(); it != mPrioVoiceList.robustEnd(); ++it)
    {
        MultiVoice& voice = *it;
        voice.updateVoiceStatus();
    }
}

void MultiVoiceMgr::updateAudioFrameVoiceStatus()
{
    // Apply voice-drop effect for end of waveform or to prevent DSP slowdowns.
    for (auto it = mPrioVoiceList.robustBegin(); it != mPrioVoiceList.robustEnd(); ++it)
    {
        MultiVoice& voice = *it;
        if (voice.mUpdateType == UpdateType::AudioFrame)
            voice.updateVoiceStatus();
    }
}

void MultiVoiceMgr::updateAllVoicesSync(u32 syncFlag)
{
    for (auto it = mPrioVoiceList.robustBegin(); it != mPrioVoiceList.robustEnd(); ++it)
    {
        MultiVoice& voice = *it;
        if (voice.mIsActive)
            voice.mSyncFlag |= syncFlag;
    }
}

s32 MultiVoiceMgr::getVoiceCount() const
{
    SoundThreadLock lock;

    s32 voiceCount = 0;

    for (auto& voice : mPrioVoiceList)
    {
        voiceCount += voice.getSdkVoiceCount();
    }

    return voiceCount;
}

u32 MultiVoiceMgr::getActiveCount() const
{
    SoundThreadLock lock;

    return mPrioVoiceList.size();
}

u32 MultiVoiceMgr::getFreeCount() const
{
    SoundThreadLock lock;

    return mFreeVoiceList.size();
}

void MultiVoiceMgr::changeVoicePriority(MultiVoice* voice)
{
    mPrioVoiceList.erase(voice);
    this->appendVoiceList(voice);
}

s32 MultiVoiceMgr::dropLowestPriorityVoice(u32 priority)
{
    if (mPrioVoiceList.isEmpty())
        return 0;

    MultiVoice* dropVoice = mPrioVoiceList.front();
    if (dropVoice->getPriority() > priority)
        return 0;

    // Drops the voice with the lowest priority
    s32 dropCount = dropVoice->getSdkVoiceCount();

    MultiVoice::VoiceCallback callbackFunc = dropVoice->mCallback;
    void* callbackData = dropVoice->mCallbackData;

    dropVoice->stop();
    dropVoice->free();

    if (callbackFunc)
        callbackFunc(dropVoice, MultiVoice::VoiceCallbackStatus::DropVoice, callbackData);

    return dropCount;
}

void MultiVoiceMgr::appendVoiceList(MultiVoice* voice)
{
    // Insert into the priority list. If the voices have the same priority levels, tack the new one after the older one.
    // Searching in reverse is faster if the priorities are the same.
    VoiceList::reverseIterator itr = mPrioVoiceList.reverseBegin();

    while (itr != mPrioVoiceList.reverseEnd())
    {
        if (itr->mPriority <= voice->mPriority)
            break;

        ++itr;
    }

    mPrioVoiceList.insertBefore(&(*itr), voice);
}

void MultiVoiceMgr::removeVoiceList(MultiVoice* voice)
{
    mPrioVoiceList.erase(voice);
    mFreeVoiceList.pushBack(voice);
}

} } } // namespace snd::internal::driver
