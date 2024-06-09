#pragma once

#include <container/seadOffsetList.h>

#include "Voice.h"

namespace snd { namespace internal { namespace driver {

class MultiVoice
{
    SEAD_NO_COPY(MultiVoice);

public:
    // Update flags
    static const s32 cUpdateStart = 0x1 << 0;
    static const s32 cUpdatePause = 0x1 << 1;
    static const s32 cUpdateSrc = 0x1 << 2;
    static const s32 cUpdateMix = 0x1 << 3;
    static const s32 cUpdateLpf = 0x1 << 4;
    static const s32 cUpdateBiquad = 0x1 << 5;
    static const s32 cUpdateVe = 0x1 << 6;

    // Callback Function
    enum class VoiceCallbackStatus
    {
        FinishWave,
        Cancel,
        DropVoice,
        DropDsp
    };

    typedef void (*VoiceCallback)(MultiVoice* voice, VoiceCallbackStatus status, void* callbackData);

    // Parameter boundaries
    static constexpr f32 cVolumeMin = 0.0f;
    static constexpr f32 cVolumeDefault = 1.0f;
    static constexpr f32 cVolumeMax = 2.0f;

    static constexpr f32 cPanLeft = -1.0f;
    static constexpr f32 cPanCenter = 0.0f;
    static constexpr f32 cPanRight = 1.0f;

    static constexpr f32 cSpanFront = 0.0f;
    static constexpr f32 cSpanCenter = 1.0f;
    static constexpr f32 cSpanRear = 2.0f;

    static constexpr f32 cCutOffFreqMin = 0.0f;
    static constexpr f32 cCutOffFreqMax = 1.0f;

    static constexpr f32 cBiquadValueMin = 0.0f;
    static constexpr f32 cBiquadValueMax = 1.0f;

    static constexpr f32 cSendMin = 0.0f;
    static constexpr f32 cSendMax = 1.0f;

    static const u32 cPriorityNoDrop = Voice::cPriorityNoDrop;

public:
    MultiVoice();
    ~MultiVoice();

    bool alloc(s32 channelCount, u32 priority, VoiceCallback callback, void* callbackData);
    void free();

    void start();
    void stop();
    void updateVoiceStatus();
    void pause(bool flag);

    void calc();
    void update();

    bool isActive() const
    {
        return mChannelCount > 0;
    }

    bool isRun() const;

    bool isPause() const
    {
        return mIsPause;
    }

    bool isPlayFinished() const;

    void setSampleFormat(SampleFormat format);
    void setSampleRate(u32 sampleRate);

    f32 getVolume() const
    {
        return mVolume;
    }

    void setVolume(f32 volume);

    f32 getPitch() const
    {
        return mPitch;
    }

    void setPitch(f32 pitch);

    void setPanMode(PanMode panMode);
    void setPanCurve(PanCurve panCurve);

    f32 getLpfFreq() const
    {
        return mLpfFreq;
    }

    void setLpfFreq(f32 lpfFreq);

    s32 getBiquadType() const
    {
        return mBiquadType;
    }

    f32 getBiquadValue() const
    {
        return mBiquadValue;
    }

    void setBiquadFilter(s32 type, f32 value);

    u32 getPriority() const
    {
        return mPriority;
    }

    void setPriority(u32 priority);

    //void setFrontBypass(bool isFrontBypass);

    void setInterpolationType(u8 interpolationType);

    s32 getSdkVoiceCount() const
    {
        return mChannelCount;
    }

    const Voice& detail_getSdkVoice(s32 index) const;

    void setOutputLine(u32 lineFlag);

    u32 getOutputLine() const
    {
        return mOutputLineFlag;
    }

    void setParam(const OutputParam& param);

    const OutputParam& getParam() const
    {
        return mParam;
    }

    u32 getCurrentPlayingSample() const;

    SampleFormat getFormat() const;

    static u32 frameToByte(u32 sample, SampleFormat format);
    static void calcOffsetAdpcmParam(AdpcmContext* context, const AdpcmParam& param, u32 offsetSamples, const void* dataAddress);

    void appendWaveBuffer(s32 channel, WaveBuffer* waveBuffer, bool lastFlag);

    void setAdpcmParam(s32 channelIndex, const AdpcmParam& param);

    void setUpdateType(UpdateType updateType)
    {
        mUpdateType = updateType;
    }

    UpdateType getUpdateType() const
    {
        return mUpdateType;
    }

private:
    struct PreMixVolume
    {
        f32 volume[(u32)ChannelIdx::Num];
        f32 lrMixedVolume;
    };

    void initParam(VoiceCallback callback, void* callbackData);

    // Update function
    void calcSrc(bool initialUpdate);
    void calcVe();
    void calcMix();
    void calcLpf();
    void calcBiquadFilter();

    void calcPreMixVolume(PreMixVolume* mix, const OutputParam& param, s32 channelIndex);
    void calcMix(OutputMix* mix, const PreMixVolume& preMix);
    void calcMixImpl(OutputMix* mix, const OutputParam& param, const PreMixVolume& pre);

    void runAllSdkVoice();
    void stopAllSdkVoice();
    void pauseAllSdkVoice();

    void setOutputParamImpl(const OutputParam& in, OutputParam& out);

private:
    friend class MultiVoiceMgr;

    Voice mVoice[cWaveChannelMax];

    s32 mChannelCount;

    VoiceCallback mCallback;
    void* mCallbackData;

    bool mIsActive;
    bool mIsStart;
    bool mIsStarted;
    bool mIsPause;
    bool mIsPausing;
    bool mIsInitialized;

    WaveBuffer* mLastWaveBuffer;

    u16 mSyncFlag;

    u8 mBiquadType;

    //bool mIsEnableFrontBypass;

    f32 mVolume;
    f32 mPitch;
    PanMode mPanMode;
    PanCurve mPanCurve;
    f32 mLpfFreq;
    f32 mBiquadValue;
    u32 mPriority;

    u32 mOutputLineFlag;

    // L/R: -1.0f(left) - 0.0f(center) - 1.0f(right)
    // F/R: 0.0f(front) - 2.0f(rear)
    OutputParam mParam;

    SampleFormat mFormat;

    void* mVoiceUser; // Channel or StreamTrack

    UpdateType mUpdateType;

    sead::ListNode mListNode;
};

} } } // namespace snd::internal::driver
