#pragma once

#include "Global.h"

namespace snd { namespace internal {

class VoiceImpl;
class VoiceMgr;

class Voice
{
    SEAD_NO_COPY(Voice);

public:
    static const u32 cPriorityMin = 0;
    static const u32 cPriorityMax = 255;
    static const u32 cPriorityNoDrop = cPriorityMax;

public:
    Voice();
    ~Voice();

    bool allocVoice(u32 priority);
    void free();

    bool isAvailable() const;

    void setState(VoiceState state);

    void appendWaveBuffer(WaveBuffer* waveBuffer);

    void setSampleFormat(SampleFormat format)
    {
        mSampleFormat = format;
    }

    void setSampleRate(u32 sampleRate)
    {
        mSampleRate = sampleRate;
    }

    void setAdpcmParam(const AdpcmParam& param)
    {
        mAdpcmParam = param;
    }

    void setPriority(u32 priority);

    void setVolume(f32 volume)
    {
        mVoiceParam.volume = volume;
    }

    void setPitch(f32 pitch)
    {
        mVoiceParam.pitch = pitch;
    }

    void setMix(const OutputMix& mix)
    {
        mVoiceParam.mix = mix;
    }

    void setInterpolationType(u8 type)
    {
        mVoiceParam.interpolationType = type;
    }

    void setMonoFilter(bool enable, u16 cutoff = 0);
    void setBiquadFilter(bool enable, const BiquadFilterCoefficients* coef = nullptr);

    void updateParam();
    //void updateVoiceStatus();

    VoiceState getState() const
    {
        return mState;
    }

    u32 getPlayPosition() const;

    s32 getPriority() const
    {
        return mPriority;
    }

    f32 getVolume() const
    {
        return mVoiceParam.volume;
    }

    f32 getPitch() const
    {
        return mVoiceParam.pitch;
    }

    const OutputMix& getMix() const
    {
        return mVoiceParam.mix;
    }

    static void detail_setVoiceMgr(VoiceMgr* voiceMgr);

private:
    void initialize_(u32 priority);
    void freeAllWaveBuffer_();

    static void VoiceImplDisposeCallback(VoiceImpl* voiceImpl, void* arg);

private:
    u32 mPriority;

    VoiceState mState;
    VoiceParam mVoiceParam;

    SampleFormat mSampleFormat;
    u32 mSampleRate;

    VoiceImpl* mVoiceImpl;
    AdpcmParam mAdpcmParam;

    static VoiceMgr* sVoiceMgr;
};

} } // namespace snd::internal
