#pragma once

#include <basis/seadTypes.h>

namespace snd {

struct AdpcmParam
{
    u16 coef[8][2];
};

struct AdpcmContext
{
    u16 pred_scale;
    s16 yn1;
    s16 yn2;
};

} // namespace snd
