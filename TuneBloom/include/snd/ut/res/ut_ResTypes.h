#pragma once

#include <prim/seadEndian.h>

#define NW_UT_RES_SWAP_ENDIAN

inline sead::Endian::Types sFileEndian = sead::Endian::eBig;

namespace nw { namespace ut {

namespace EndianSwap {

inline u16 BSwap(u16 val)
{
    if (sFileEndian != sead::Endian::getHostEndian())
        return sead::Endian::swapU16(val);

    return val;
}

inline s16 BSwap(s16 val)
{
    if (sFileEndian != sead::Endian::getHostEndian())
        return sead::Endian::swapS16(val);

    return val;
}

inline u32 BSwap(u32 val)
{
    if (sFileEndian != sead::Endian::getHostEndian())
        return sead::Endian::swapU32(val);

    return val;
}

inline s32 BSwap(s32 val)
{
    if (sFileEndian != sead::Endian::getHostEndian())
        return sead::Endian::swapS32(val);

    return val;
}

inline u64 BSwap(u64 val)
{
    if (sFileEndian != sead::Endian::getHostEndian())
        return sead::Endian::swapU64(val);

    return val;
}

inline s64 BSwap(s64 val)
{
    if (sFileEndian != sead::Endian::getHostEndian())
        return sead::Endian::swapS64(val);

    return val;
}

inline f32 BSwap(f32 val)
{
    if (sFileEndian != sead::Endian::getHostEndian())
        return sead::Endian::swapF32(val);

    return val;
}

//! Ehh
//inline f64 BSwap(f64 val)
//{
//    if (sFileEndian != sead::Endian::getHostEndian())
//        return sead::Endian::swapF64(val);
//
//    return val;
//}

} // namespace nw::ut::EndianSwap

#if !defined(NW_UT_RES_SWAP_ENDIAN)
    typedef u8  ResU8;
    typedef s8  ResS8;
    typedef u16 ResU16;
    typedef s16 ResS16;
    typedef u32 ResU32;
    typedef s32 ResS32;
    typedef f32 ResF32;
    typedef u32 ResU64; //! Should i fix this ?
    typedef s32 ResS64; //! ^
    typedef f32 ResF64; //! ^
#else
    template <typename T>
    class ResNum
    {
    public:
        ResNum()
        {
        }

        ResNum(const ResNum& other)
            : mBits(other.mBits)
        {
        }

        explicit ResNum(const T val)
            : mBits(EndianSwap::BSwap(val))
        {
        }

        void operator=(const ResNum& other)
        {
            mBits = other.mBits;
        }

        void operator=(T val)
        {
            mBits = EndianSwap::BSwap(val);
        }

        operator T() const
        {
            return EndianSwap::BSwap(mBits);
        }

        ResNum operator+=(T num)
        {
            mBits = EndianSwap::BSwap(static_cast<T>(*this) + num);
            return ResNum(*this);
        }

        ResNum operator-=(T num)
        {
            mBits = EndianSwap::BSwap(static_cast<T>(*this) - num);
            return ResNum(*this);
        }

    private:
        T mBits;
    };

    //? Don't need to add float specialization ?

    typedef u8          ResU8;
    typedef s8          ResS8;
    typedef ResNum<u16> ResU16;
    typedef ResNum<s16> ResS16;
    typedef ResNum<u32> ResU32;
    typedef ResNum<s32> ResS32;
    typedef ResNum<f32> ResF32;
    typedef ResNum<u64> ResU64;
    typedef ResNum<s64> ResS64;
    typedef ResNum<f64> ResF64;

    typedef ResU32 Size;
    typedef ResU32 Length;

    struct ResBool
    {
        ResS8 value;

        operator bool() const
        {
            return value != 0;
        }

        bool operator=(bool rhs)
        {
            value = rhs;
            return bool(*this);
        }
    };
#endif // NW_UT_RES_SWAP_ENDIAN

} } // namespace nw::ut
