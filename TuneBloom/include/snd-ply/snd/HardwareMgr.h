#pragma once

#include "FinalMixCallback.h"
#include "Global.h"
#include "MoveValue.h"
#include "BiquadFilterPresets.h"

#include <heap/seadDisposer.h>
#include <thread/seadCriticalSection.h>

namespace snd { namespace internal { namespace driver {

class HardwareMgr
{
    SEAD_SINGLETON_DISPOSER(HardwareMgr);

public:
    typedef void (*HardwareCallback)();

    static const u32 cSoundFrameIntervalMSEC = 3;
    static const u32 cSoundFrameIntervalUSEC = cSoundFrameIntervalMSEC * 1000;

    static const u32 cSampleRate = 32000;
    static const u32 cSamplePerFrame = cSampleRate * cSoundFrameIntervalMSEC / 1000;

    static const u32 cChannelCount = 2;

    // How many voices?
    static const u32 cMaxVoiceCount = 96; // AX_MAX_VOICES

    static f32 sLeftDataBuffer[cSamplePerFrame];
    static f32 sRightDataBuffer[cSamplePerFrame];

private:
    HardwareMgr();
    ~HardwareMgr();

public:
    void initialize();
    void finalize();

    bool isInitialized() const
    {
        return mIsInitialized;
    }

    void update();

    void setOutputMode(OutputMode mode);

    OutputMode getOutputMode() const
    {
        return mOutputMode;
    }

    OutputMode getEndUserOutputMode() const
    {
        return mEndUserOutputMode;
    }

    void updateEndUserOutputMode();

    void setOutputDeviceFlag(u32 outputLineIndex, u8 outputDeviceFlag);

    u8 getOutputDeviceFlag(u32 outputLineIndex) const
    {
        if (outputLineIndex < (s32)OutputLineIdx::Max)
        {
            return mOutputDeviceFlag[outputLineIndex];
        }

        return 0;
    }

    // Effect...

    f32 getOutputVolume() const;

    void setMasterVolume(f32 volume, s32 fadeTimes);

    f32 getMasterVolume() const
    {
        return mMasterVolume.getValue();
    }

    void setSrcType(SrcType type);

    SrcType getSrcType() const
    {
        return mSrcType;
    }

    void setBiquadFilterCallback(BiquadFilterType type, const BiquadFilterCallback* cb);

    const BiquadFilterCallback* getBiquadFilterCallback(s32 type)
    {
        SEAD_ASSERT(type >= (s32)BiquadFilterType::DataMin && type <= (s32)BiquadFilterType::Max);
        return mBiquadFilterCallbackTable[type];
    }

    void prepareReset();
    bool isResetReady() const;

    void appendFinalMixCallback(FinalMixCallback* callback);
    void prependFinalMixCallback(FinalMixCallback* callback);
    void eraseFinalMixCallback(FinalMixCallback* callback);

    void onFinalMixCallback(f32** data, u32 sampleCount, u32 channelCount);

    static void registerHwCallback(HardwareCallback callback);
    static void deregisterHwCallback(HardwareCallback callback);
    static void callHwCallback();

    static void resetFinalMixCallbackData();
    static void processFinalMixCallback(f32** data, u32 sampleCount, u32 channelCount);

private:
    static const BiquadFilterLpf cBiquadFilterLpf;
    static const BiquadFilterHpf cBiquadFilterHpf;
    static const BiquadFilterBpf512 cBiquadFilterBpf512;
    static const BiquadFilterBpf1024 cBiquadFilterBpf1024;
    static const BiquadFilterBpf2048 cBiquadFilterBpf2048;

private:
    bool mIsInitialized;

    OutputMode mOutputMode;
    OutputMode mEndUserOutputMode;
    SrcType mSrcType;

    MoveValue<f32, s32> mMasterVolume;
    MoveValue<f32, s32> mVolumeForReset;

    typedef sead::OffsetList<FinalMixCallback> FinalMixCallbackList;

    FinalMixCallbackList mFinalMixCallbackList;
    sead::CriticalSection mCriticalSection;

    const BiquadFilterCallback* mBiquadFilterCallbackTable[(s32)BiquadFilterType::Max + 1];

    u8 mOutputDeviceFlag[(s32)OutputLineIdx::Max];
};

} } } // namespace snd::internal::driver
