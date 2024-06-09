#pragma once

#include <basis/seadTypes.h>

namespace nw { namespace snd {

struct AdpcmContext
{
    u16 pred_scale;
    s16 yn1;
    s16 yn2;
};

} } // namespace nw::snd
