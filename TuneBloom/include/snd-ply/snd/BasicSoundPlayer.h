#pragma once

#include "Global.h"

#include <basis/seadRawPrint.h>

struct PlayerParamSet
{
    f32 volume;
    f32 pitch;
    f32 lpfFreq;
    f32 biquadValue;
    s8 biquadType;
    snd::PanMode panMode;
    snd::PanCurve panCurve;
    u32 outputLineFlag;

    snd::internal::OutputParam param;

    PlayerParamSet()
    {
        initialize();
    }

    void initialize()
    {
        volume = 1.0f;
        pitch = 1.0f;
        lpfFreq = 0.0f;

        biquadType = (s8)snd::BiquadFilterType::Inherit;
        biquadValue = 0.0f;

        panMode = snd::PanMode::Dual;
        panCurve = snd::PanCurve::Sqrt;

        outputLineFlag = (u32)snd::OutputLine::Main;
        param.initialize();
    }
};

class BasicSoundPlayer
{
public:
    BasicSoundPlayer()
        : mActiveFlag(false)
        , mPauseFlag(false)
    {
    }

    void init()
    {
        mPauseFlag = false;
        mActiveFlag = false;

        mInitVolume = 1.0f;

        mPlayerParamSet.initialize();
    }

    virtual void pause(bool flag) = 0;

    bool isActive() const
    {
        return mActiveFlag;
    }

    bool isPause() const
    {
        return mPauseFlag;
    }

    void setInitialVolume(f32 volume)
    {
        SEAD_ASSERT(volume >= 0.0f);
        if (volume < 0.0f)
            volume = 0.0f;

        mInitVolume = volume;
        //SEAD_PRINT("%f\n", volume);
    }

    f32 getInitialVolume() const
    {
        return mInitVolume;
    }

    void setVolume(f32 volume) { mPlayerParamSet.volume = volume; }
    void setPitch(f32 pitch) { mPlayerParamSet.pitch = pitch; }
    void setLpfFreq(f32 lpfFreq) { mPlayerParamSet.lpfFreq = lpfFreq; }

    void setBiquadFilter(s32 type, f32 value)
    {
        SEAD_ASSERT((s32)snd::BiquadFilterType::Min <= type && type <= (s32)snd::BiquadFilterType::Max);
        SEAD_ASSERT(0.0f <= value && value <= 1.0f);

        mPlayerParamSet.biquadType = static_cast<s8>(type);
        mPlayerParamSet.biquadValue = value;
    }

    void setPanMode(snd::PanMode mode) { mPlayerParamSet.panMode = mode; }
    void setPanCurve(snd::PanCurve curve) { mPlayerParamSet.panCurve = curve; }

    f32 getParamVolume() const { return mPlayerParamSet.volume; }

    f32 getVolume() const { return mPlayerParamSet.volume * getInitialVolume(); }
    f32 getPitch() const { return mPlayerParamSet.pitch; }
    f32 getLpfFreq() const { return mPlayerParamSet.lpfFreq; }
    s32 getBiquadFilterType() const { return mPlayerParamSet.biquadType; }
    f32 getBiquadFilterValue() const { return mPlayerParamSet.biquadValue; }
    snd::PanMode getPanMode() const { return mPlayerParamSet.panMode; }
    snd::PanCurve getPanCurve() const { return mPlayerParamSet.panCurve; }

    u32 getOutputLine() const { return mPlayerParamSet.outputLineFlag; }

    const snd::internal::OutputParam& getParam() const
    {
        return mPlayerParamSet.param;
    }

protected:
    bool mActiveFlag;
    bool mPauseFlag;

    f32 mInitVolume;

    PlayerParamSet mPlayerParamSet;
};
