#pragma once

#include <basis/seadTypes.h>

namespace snd { namespace internal {

struct CurveLfoParam
{
    enum class CurveType
    {
        Sine,
        Min = Sine,
        Triangle,
        Saw,
        Square,
        Random,
        UserMin = 64,
        UserMax = 127,
        Max = UserMax,
        Count
    };

    CurveLfoParam()
    {
        initialize();
    }

    void initialize();

    f32 depth;
    f32 speed; // [Hz]
    u32 delay;
    u8 range;
    u8 curve; // enum Curve
    u8 phase;
    u8 padding;
};

class CurveLfo
{
public:
    CurveLfo()
        : mDelayCounter(0)
        , mCounter(0.0f)
        , mRandomValue(1.0f)
        , mIsStart(false), mIsNext(false)
    {
    }

    void reset();

    void update(s32 msec);
    f32 getValue() const;

    void setParam(const CurveLfoParam& lfoParam)
    {
        mParam = lfoParam;
    }

    CurveLfoParam& getParam()
    {
        return mParam;
    }

    const CurveLfoParam& getParam() const
    {
        return mParam;
    }

    typedef f32 (*CurveFunc)(f32 arg);

    static void initializeCurveTable();
    static CurveFunc registerUserCurve(CurveFunc userCurve, u32 index); // The indexes are from 0 to 63.
    static CurveFunc unregisterUserCurve(u32 index); // The indexes are from 0 to 63.

private:
    CurveLfoParam mParam;
    u32 mDelayCounter;
    f32 mCounter;
    mutable f32 mRandomValue;
    bool mIsStart;
    bool mIsNext;
    u8 padding[2];
};

} } // namespace snd::internal
