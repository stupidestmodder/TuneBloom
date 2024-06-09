#pragma once

#include "Global.h"

namespace snd { namespace internal {

class Util
{
public:
    enum class PanCurve
    {
        Sqrt,
        SinCos,
        Linear
    };

    struct PanInfo
    {
        PanInfo()
            : curve(PanCurve::Sqrt)
            , centerZeroFlag(false)
            , zeroClampFlag(false)
            //, isEnableFrontBypass(false)
        {
        }

        PanCurve curve;
        bool centerZeroFlag; // whether the center should be set to 0 dB
        bool zeroClampFlag; // whether the volume should be clamped when it exceeds 0 dB
        //bool isEnableFrontBypass;
    };

public:
    static const s32 cVolumeDbMin = -904; // -90.4.4 dB = -inf
    static const s32 cVolumeDbMax = 60; // + 6.0dB

    static const s32 cPitchDivisionBit = 8; // Half-tone resolution. (Number of bits.)
    static const s32 cPitchDivisionRange = 1 << cPitchDivisionBit; // Half-tone resolution

    static u16 calcLpfFreq(f32 scale);
    static f32 calcPanRatio(f32 pan, const PanInfo& info, OutputMode mode);
    static f32 calcSurroundPanRatio(f32 surroundPan, const PanInfo& info);

    static f32 calcPitchRatio(s32 pitch);
    static f32 calcVolumeRatio(f32 dB);

    static u32 getSampleByByte(u32 byte, SampleFormat format);
    static u32 getByteBySample(u32 sample, SampleFormat format);

private:
    // CalcLpfFreq Table
    static const s32 cCalcLpfFreqTableSize = 24;
    static constexpr f32 cCalcLpfFreqIntercept = 0.135614381f;
    static const u16 cCalcLpfFreqTable[cCalcLpfFreqTableSize];

private:
    Util();
};

} } // namespace snd::internal
