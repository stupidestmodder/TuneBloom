#pragma once

#include <basis/seadTypes.h>

namespace snd { namespace internal {

class BasicSound
{
public:
    enum class PlayerState
    {
        Init,
        Play,
        Stop
    };

    static const s32 cPriorityMin = 0;
    static const s32 cPriorityMax = 127;
    static const u32 cInvalidId = 0xFFFFFFFF;

public:
    BasicSound();
    virtual ~BasicSound();

private:
};

} } // namespace snd::internal
