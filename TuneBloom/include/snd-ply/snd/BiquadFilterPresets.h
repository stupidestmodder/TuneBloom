#pragma once

#include "BiquadFilterCallback.h"

namespace snd { namespace internal {

/* ------------------------------------------------------------------------
        BiquadFilterLpf - Low-pass filter
        (1/16 octave interval, Chebyshev approximation)
   ------------------------------------------------------------------------ */
class BiquadFilterLpf : public BiquadFilterCallback
{
public:
    BiquadFilterLpf()
    {
    }

    void getCoefficients(s32 type, f32 value, Coefficients* coef) const override;

private:
    static const s32 cCoefficientsTableSize = 112;
    static const Coefficients cCoefficientsTable[cCoefficientsTableSize];
};

/* ------------------------------------------------------------------------
        BiquadFilterHpf - High-pass filter
        (1/16 octave interval, Chebyshev approximation)
   ------------------------------------------------------------------------ */
class BiquadFilterHpf : public BiquadFilterCallback
{
public:
    BiquadFilterHpf()
    {
    }

    void getCoefficients(s32 type, f32 value, Coefficients* coef) const override;

private:
    static const s32 cCoefficientsTableSize = 97;
    static const Coefficients cCoefficientsTable[cCoefficientsTableSize];
};

/* ------------------------------------------------------------------------
        BiquadFilterBpf512 - Band-pass filter
        (center frequency of 512 Hz, Chebyshev approximation)
   ------------------------------------------------------------------------ */
class BiquadFilterBpf512 : public BiquadFilterCallback
{
public:
    BiquadFilterBpf512()
    {
    }

    void getCoefficients(s32 type, f32 value, Coefficients* coef) const override;

private:
    static const s32 cCoefficientsTableSize = 122;
    static const Coefficients cCoefficientsTable[cCoefficientsTableSize];
};

/* ------------------------------------------------------------------------
        BiquadFilterBpf1024 - Band-pass filter
        (center frequency of 1024 Hz, Chebyshev approximation)
   ------------------------------------------------------------------------ */
class BiquadFilterBpf1024 : public BiquadFilterCallback
{
public:
    BiquadFilterBpf1024()
    {
    }

    void getCoefficients(s32 type, f32 value, Coefficients* coef) const override;

private:
    static const s32 cCoefficientsTableSize = 93;
    static const Coefficients cCoefficientsTable[cCoefficientsTableSize];
};

/* ------------------------------------------------------------------------
        BiquadFilterBpf2048 - Band-pass filter
        (center frequency of 2048 Hz, Chebyshev approximation)
   ------------------------------------------------------------------------ */
class BiquadFilterBpf2048 : public BiquadFilterCallback
{
public:
    BiquadFilterBpf2048()
    {
    }

    void getCoefficients(s32 type, f32 value, Coefficients* coef) const override;

private:
    static const s32 cCoefficientsTableSize = 93;
    static const Coefficients cCoefficientsTable[cCoefficientsTableSize];
};

} } // namespace snd::internal
