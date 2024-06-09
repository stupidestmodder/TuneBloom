#include "snd/CurveLfo.h"

#include <math/seadMathCalcCommon.h>
#include <random/seadGlobalRandom.h>
#include <prim/seadMemUtil.h>

namespace snd { namespace internal {

namespace {

f32 curveSine(f32 arg)
{
    return sead::Mathf::sinIdx(static_cast<u32>(arg * 0x100000000));
}

f32 curveSquare(f32 arg)
{
    if (arg < 0.5f)
        return 0.0f;

    return 1.0f;
}

f32 curveSaw(f32 arg)
{
    // Change the domain of definition (arg) between 0.0f and 1.0f to the domain of values 0.0f to 1.0f.
    return arg;
}

f32 curveTriangle(f32 arg)
{
    // [0] 0.00 -> 0.25  :  4 * arg;
    // [1] 0.25 -> 0.75  : -4 * arg + 2;
    // [2] 0.75 -> 1.00  :  4 * arg - 3;
    const f32 gradients[5] = { 4.0f, -4.0f, -4.0f, 4.0f, 4.0f };
    const f32 intercepts[5] = { 0.0f, 2.0f, 2.0f, -4.0f, 0.0f };

    u32 tmp = static_cast<u32>(arg * 4);

    return gradients[tmp] * arg + intercepts[tmp];
}

f32 curveRandom(f32 /*arg*/)
{
    return sead::GlobalRandom::instance()->getF32();
}

CurveLfo::CurveFunc sCurveFuncTable[(u32)CurveLfoParam::CurveType::Count] = { nullptr };

} // anonymous namespace

void CurveLfoParam::initialize()
{
    depth = 0.0f;
    speed = 6.25f;
    delay = 0;
    range = 1;
    curve = 0;
    phase = 0;
}

void CurveLfo::reset()
{
    mCounter = 0.0f;
    mRandomValue = 1.0f;
    mDelayCounter = 0;
    mIsStart = false;
    mIsNext = false;
}

void CurveLfo::update(s32 msec)
{
    if (mDelayCounter < mParam.delay)
    {
        if (mDelayCounter + msec <= mParam.delay)
        {
            mDelayCounter += msec;
            return;
        }
        else
        {
            msec -= mParam.delay - mDelayCounter;
            mDelayCounter = mParam.delay;
        }
    }

    if (mParam.speed > 0.0f)
    {
        if (mIsStart == false)
        {
            mCounter = mParam.phase / 127.0f;
            mIsStart = true;
        }

        mCounter += mParam.speed * msec / 1000.0f;

        if (mCounter >= 1.0f)
            mIsNext = true;
        else
            mIsNext = false;

        mCounter -= static_cast<s32>(mCounter);
    }
}

f32 CurveLfo::getValue() const
{
    if (mParam.depth == 0.0f)
        return 0.0f;

    if (mDelayCounter < mParam.delay)
        return 0.0f;

    SEAD_ASSERT(mParam.curve >= (u32)CurveLfoParam::CurveType::Min && mParam.curve <= (u32)CurveLfoParam::CurveType::Max);

    CurveFunc func = sCurveFuncTable[mParam.curve];

    f32 value = 0.f;
    if (func)
    {
        SEAD_ASSERT(mCounter >= 0.0f && mCounter <= 1.0f);

        // Random has been subjected to special handling so that it can get the same value continuously in one cycle.
        if (mParam.curve == (u32)CurveLfoParam::CurveType::Random)
        {
            if (mIsNext)
            {
                value = func(mCounter);
                mRandomValue = value;
            }
            else
            {
                value = mRandomValue;
            }
        }
        else
        {
            value = func(mCounter);
        }
    }
    else
    {
        value = 1.0f;
    }

    value *= mParam.depth;
    value *= mParam.range;

    return value;
}

void CurveLfo::initializeCurveTable()
{
    sead::MemUtil::fill(sCurveFuncTable, 0, sizeof(CurveFunc) * (u32)CurveLfoParam::CurveType::Count);

    sCurveFuncTable[(u32)CurveLfoParam::CurveType::Sine] = curveSine;
    sCurveFuncTable[(u32)CurveLfoParam::CurveType::Triangle] = curveTriangle;
    sCurveFuncTable[(u32)CurveLfoParam::CurveType::Saw] = curveSaw;
    sCurveFuncTable[(u32)CurveLfoParam::CurveType::Square] = curveSquare;
    sCurveFuncTable[(u32)CurveLfoParam::CurveType::Random] = curveRandom;
}

CurveLfo::CurveFunc CurveLfo::registerUserCurve(CurveFunc func, u32 index)
{
    SEAD_ASSERT(static_cast<s32>(index) >= 0 && static_cast<s32>(index) <= (s32)CurveLfoParam::CurveType::UserMax - (s32)CurveLfoParam::CurveType::UserMin);

    s32 tableIndex = index + (u32)CurveLfoParam::CurveType::UserMin;

    CurveFunc tmp = sCurveFuncTable[tableIndex];
    sCurveFuncTable[tableIndex] = func;

    return tmp;
}

CurveLfo::CurveFunc CurveLfo::unregisterUserCurve(u32 index)
{
    SEAD_ASSERT(static_cast<s32>(index) >= 0 && static_cast<s32>(index) <= (s32)CurveLfoParam::CurveType::UserMax - (s32)CurveLfoParam::CurveType::UserMin);

    s32 tableIndex = index + (u32)CurveLfoParam::CurveType::UserMin;

    CurveFunc tmp = sCurveFuncTable[tableIndex];
    sCurveFuncTable[tableIndex] = nullptr;

    return tmp;
}

} } // namespace snd::internal
