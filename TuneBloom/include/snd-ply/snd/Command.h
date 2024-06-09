#pragma once

#include <basis/seadTypes.h>

namespace snd { namespace internal {

struct Command
{
    Command* next;
    u32 id;
    u32 tag;
    u32 memory_next;
};

} } // namespace snd::internal
