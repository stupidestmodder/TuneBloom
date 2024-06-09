#pragma once

#include <basis/seadTypes.h>

namespace snd { namespace internal {

class CurveAdshr
{
public:
    enum class Status
    {
        Attack = 0,
        Hold,
        Decay,
        Sustain,
        Release
    };

private:
    static constexpr f32 cVolumeInit = -90.4f;
    static const s32 cAttackInit = 127;
    static const s32 cHoldInit = 0;
    static const s32 cDecayInit = 127;
    static const s32 cSustainInit = 127;
    static const s32 cReleaseInit = 127;

    static const s32 cDecibelSquareTableSize = 128;
    static const s32 cCalcDecibelScaleMax = 127;
    static const s16 cDecibelSquareTable[cDecibelSquareTableSize];

public:
    CurveAdshr()
    {
        initialize();
    }

    void initialize(f32 initDecibel = cVolumeInit);
    void reset(f32 initDecibel = cVolumeInit);

    void update(s32 msec);
    f32 getValue() const;

    Status getStatus() const
    {
        return mStatus;
    }

    void setStatus(Status status)
    {
        mStatus = status;
    }

    void setAttack(s32 attack);
    void setHold(s32 hold);
    void setDecay(s32 decay);
    void setSustain(s32 sustain);
    void setRelease(s32 release);

private:
    static f32 calcRelease(s32 release);
    static s16 calcDecibelSquare(s32 scale);

    Status mStatus;
    f32 mValue;
    f32 mDecay;
    f32 mRelease;
    f32 mAttack;
    u16 mHold;
    u16 mHoldCounter;
    u8 mSustain;
    u8 padding[3];
};

} } // namespace snd::internal
