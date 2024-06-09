#pragma once

#include <basis/seadTypes.h>

/*
template <typename MostDerived, typename C, typename M>
constexpr u32 calcOffset(M C::* member)
{
    u8 buf[sizeof(MostDerived)];
    MostDerived* d = (MostDerived*)buf;
    return reinterpret_cast<u8*>(&(d->*member)) - reinterpret_cast<u8*>(d);
}
*/
