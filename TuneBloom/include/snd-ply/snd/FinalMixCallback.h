#pragma once

#include "Global.h"

#include <container/seadOffsetList.h>

namespace snd {

namespace internal { namespace driver {

class HardwareMgr;

} } // namespace internal::driver

class FinalMixCallback
{
    SEAD_NO_COPY(FinalMixCallback);

public:
    FinalMixCallback()
    {
    }

    virtual ~FinalMixCallback()
    {
    }

    virtual void onFinalMix(const FinalMixData* data) = 0;

private:
    friend class snd::internal::driver::HardwareMgr;

    sead::ListNode mListNode;
};

} // namespace snd
