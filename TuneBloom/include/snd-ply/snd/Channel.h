#pragma once

#include "HardwareMgr.h"
#include "MultiVoice.h"
#include "WaveFileReader.h"
#include "DisposeCallback.h"
#include "CurveAdshr.h"
#include "CurveLfo.h"
#include "MoveValue.h"

#include <math/seadMathCalcCommon.h>

namespace snd { namespace internal { namespace driver {

class Channel
{
public:
    enum class LfoTarget
    {
        Pitch,
        Volume,
        Pan
    };

    enum class ChannelCallbackStatus
    {
        Stopped,
        Drop,
        Finish,
        Cancel
    };

    typedef void (*ChannelCallback)(Channel* channel, ChannelCallbackStatus status, void* userData);

    static const s32 cChannelNum = HardwareMgr::cMaxVoiceCount + 1; // Number of channels.
    static const s32 cChannelMin = 0;
    static const s32 cChannelMax = cChannelMin + cChannelNum - 1;

    static const u32 cPriorityRelease = 1;

    static const s32 cWaveBufferMax = 2;

    static const s32 cModCount = 4;

private:
    static const s32 cKeyInit = 60;
    static const s32 cOriginalKeyInit = 60;
    static const u8 cSilenceVolumeMax = 255;
    static const u8 cSilenceVolumeMin = 0;

    // Inverse of cSilenceVolumeMax. Trying to speed up process making division into multiplication.
    static constexpr f32 cSilenceVolumeMaxR = 1.0f / static_cast<f32>(cSilenceVolumeMax);

public:
    static Channel* allocChannel(s32 voiceChannelCount, u32 priority, ChannelCallback callback, void* callbackData);
    static void freeChannel(Channel* channel);
    static void detachChannel(Channel* channel);

private:
    static void VoiceCallbackFunc(MultiVoice* voice, MultiVoice::VoiceCallbackStatus status, void* arg);

public:
    Channel();
    ~Channel();

    void update(bool doPeriodicProc);
    void callChannelCallback(ChannelCallbackStatus status);

    void setOffsetContext(const WaveFileReader::OffsetContextInfo& contextInfo)
    {
        mContextInfo = contextInfo;
        mUseContextInfo = true;
    }

    void start(const WaveInfo& waveParam, s32 length, u32 startOffsetSamples);
    void stop();
    void pause(bool flag);

    void noteOff();
    void release();

    bool isActive() const
    {
        return mActiveFlag != 0;
    }

    bool isPause() const
    {
        return mPauseFlag != 0;
    }

    void setKey(u8 key)
    {
        mKey = key;
    }

    void setKey(u8 key, u8 originalKey)
    {
        mKey = key;
        mOriginalKey = originalKey;
    }

    void setInitPan(f32 pan)
    {
        mInitPan = pan;
    }

    //void setInitSurroundPan(f32 surroundPan)
    //{
    //    mInitSurroundPan = surroundPan;
    //}

    void setTune(f32 tune)
    {
        mTune = tune;
    }

    void setAttack(s32 attack)
    {
        mCurveAdshr.setAttack(attack);
    }

    void setHold(s32 hold)
    {
        mCurveAdshr.setHold(hold);
    }

    void setDecay(s32 decay)
    {
        mCurveAdshr.setDecay(decay);
    }

    void setSustain(s32 sustain)
    {
        mCurveAdshr.setSustain(sustain);
    }

    void setRelease(s32 release)
    {
        mCurveAdshr.setRelease(release);
    }

    void setSilence(bool silenceFlag, s32 fadeTimes)
    {
        SEAD_ASSERT(fadeTimes >= 0 && fadeTimes <= sead::MathCalcCommon<u16>::maxNumber());

        mSilenceVolume.setTarget(silenceFlag ? cSilenceVolumeMin : cSilenceVolumeMax, static_cast<u16>(fadeTimes));
    }

    s32 getLength() const
    {
        return mLength;
    }

    void setLength(s32 length)
    {
        mLength = length;
    }

    bool isRelease() const
    {
        return mCurveAdshr.getStatus() == CurveAdshr::Status::Release;
    }

    void setUserVolume(f32 volume)
    {
        mUserVolume = volume;
    }

    void setUserPitch(f32 pitch)
    {
        mUserPitch = pitch;
    }

    void setUserPitchRatio(f32 pitchRatio)
    {
        mUserPitchRatio = pitchRatio;
    }

    void setUserLpfFreq(f32 lpfFreq)
    {
        mUserLpfFreq = lpfFreq;
    }

    void setBiquadFilter(s32 type, f32 value)
    {
        // To reach here, the INHERIT process should be done, so BiquadFilterType::DataMin should be minimized.
        SEAD_ASSERT(type >= (s32)BiquadFilterType::DataMin && type <= (s32)BiquadFilterType::Max);

        mBiquadType = static_cast<u8>(type);
        mBiquadValue = value;
    }

    void setLfoParam(const CurveLfoParam& param, u32 i = 0)
    {
        mLfo[i].setParam(param);
    }

    void setLfoTarget(LfoTarget type, u32 i = 0)
    {
        mLfoTarget[i] = static_cast<u8>(type);
    }

    void setPriority(u32 priority);

    void setReleasePriorityFix(bool fix)
    {
        mReleasePriorityFixFlag = fix;
    }

    void setIsIgnoreNoteOff(bool flag)
    {
        mIsIgnoreNoteOff = flag;
    }

    //void setFrontBypass(bool flag)
    //{
    //    mVoice->setFrontBypass(flag);
    //}

    void setSweepParam(f32 sweepPitch, s32 sweepTime, bool autoUpdate)
    {
        mSweepPitch = sweepPitch;
        mSweepLength = sweepTime;
        mAutoSweep = autoUpdate;
        mSweepCounter = 0;
    }

    bool isAutoUpdateSweep() const
    {
        return mAutoSweep != 0;
    }

    void updateSweep(s32 count);

    void setPanMode(PanMode panMode)
    {
        mPanMode = panMode;
     }

    void setPanCurve(PanCurve panCurve)
    {
        mPanCurve = panCurve;
    }

    void setOutputLine(u32 lineFlag)
    {
        mOutputLineFlag = lineFlag;
    }

    u32 getOutputLine() const
    {
        return mOutputLineFlag;
    }

    void setParam(const OutputParam& param)
    {
        mParam = param;
    }

    const OutputParam& getParam() const
    {
        return mParam;
    }

    Channel* getNextTrackChannel() const
    {
        return mNextLink;
    }

    void setNextTrackChannel(Channel* channel)
    {
        mNextLink = channel;
    }

    u32 getCurrentPlayingSample(bool isOriginalSamplePosition) const;

    void setKeyGroupId(u8 id)
    {
        mKeyGroupId = id;
    }

    u8 getKeyGroupId() const
    {
        return mKeyGroupId;
    }

    void setInterpolationType(u8 type)
    {
        mInterpolationType = type;
    }

    u8 getInterpolationType() const
    {
        return mInterpolationType;
    }

    void setInstrumentVolume(f32 instrumentVolume)
    {
        mInstrumentVolume = instrumentVolume;
    }

    void setVelocity(f32 velocity)
    {
        mVelocity = velocity;
    }

    void setUpdateType(UpdateType updateType);
    UpdateType getUpdateType() const;

private:
    f32 getSweepValue() const;
    void initParam(ChannelCallback callback, void* callbackData);
    void appendWaveBuffer(const WaveInfo& waveInfo, u32 startOffsetSamples);

private:
    class Disposer : public DisposeCallback
    {
    public:
        Disposer()
            : mChannel(nullptr)
        {
        }

        ~Disposer() override
        {
        }

        void initialize(Channel* channel)
        {
            mChannel = channel;
        }

        void invalidateData(const void* start, const void* end) override;

    private:
        Channel* mChannel;
    };

    friend class ChannelMgr;

    Disposer mDisposer;
    CurveAdshr mCurveAdshr;

    CurveLfo mLfo[cModCount];
    u8 mLfoTarget[cModCount]; // enum LfoTarget

    u8 mPauseFlag;
    u8 mActiveFlag;
    u8 mAllocFlag;
    u8 mAutoSweep;
    u8 mReleasePriorityFixFlag;
    u8 mIsIgnoreNoteOff;
    u8 mBiquadType;
    //u8 padding;

    f32 mUserVolume;
    f32 mUserPitchRatio;
    f32 mUserLpfFreq;
    f32 mBiquadValue;
    u32 mOutputLineFlag;

    OutputParam mParam;

    f32 mUserPitch;
    f32 mSweepPitch;
    s32 mSweepCounter;
    s32 mSweepLength;

    f32 mInitPan;
    //f32 mInitSurroundPan;
    f32 mTune;
    MoveValue<u8, u16> mSilenceVolume;

    f32 mCent;
    f32 mCentPitch;

    s32 mLength;

    PanMode mPanMode;
    PanCurve mPanCurve;

    u8 mKey;
    u8 mOriginalKey;
    u8 mKeyGroupId;
    u8 mInterpolationType;

    f32 mInstrumentVolume;
    f32 mVelocity;

    ChannelCallback mCallback;
    void* mCallbackData;

    MultiVoice* mVoice;

    Channel* mNextLink;

    WaveBuffer mWaveBuffer[cWaveChannelMax][cWaveBufferMax];
    AdpcmContext mAdpcmContext[cWaveChannelMax];
    AdpcmContext mAdpcmLoopContext[cWaveChannelMax];
    u32 mStartOffsetSamples;
    u32 mLoopStartFrame;
    u32 mOriginalLoopStartFrame;
    WaveFileReader::OffsetContextInfo mContextInfo;
    bool mUseContextInfo;
    bool mLoopFlag;

    //u8 padding2;

    sead::ListNode mListNode;
};

} } } // namespace snd::internal::driver
