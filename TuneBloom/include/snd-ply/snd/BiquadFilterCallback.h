#pragma once

#include "Global.h"

namespace snd {

class BiquadFilterCallback
{
public:
    typedef BiquadFilterCoefficients Coefficients;

public:
    virtual ~BiquadFilterCallback()
    {
    }

    virtual void getCoefficients(s32 type, f32 value, Coefficients* coef) const = 0;
};

} // namespace snd
