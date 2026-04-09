#pragma once

#include <snd/ut/res/ut_ResTypes.h>

#include <basis/seadRawPrint.h>
#include <prim/seadEndian.h>

#include <snd/Global.h>

class WaveFile;

#define NW_CONSOLE_ENABLE

namespace nw { namespace snd {

enum AuxBus
{
    AUX_BUS_A,
    AUX_BUS_B,
    AUX_BUS_C,
    AUX_BUS_NUM
};

enum SampleFormat
{
    SAMPLE_FORMAT_PCM_S8,
    SAMPLE_FORMAT_PCM_S16,
    SAMPLE_FORMAT_DSP_ADPCM,
    SAMPLE_FORMAT_PCM_S32
};

static const u32 WAVE_CHANNEL_MAX = 2;

static const u32 SEQ_BANK_MAX = 4;

enum PanMode
{
    PAN_MODE_DUAL,
    PAN_MODE_BALANCE,
    PAN_MODE_INVALID
};

enum PanCurve
{
    PAN_CURVE_SQRT,
    PAN_CURVE_SQRT_0DB,
    PAN_CURVE_SQRT_0DB_CLAMP,
    PAN_CURVE_SINCOS,
    PAN_CURVE_SINCOS_0DB,
    PAN_CURVE_SINCOS_0DB_CLAMP,
    PAN_CURVE_LINEAR,
    PAN_CURVE_LINEAR_0DB,
    PAN_CURVE_LINEAR_0DB_CLAMP,
    PAN_CURVE_INVALID
};

enum WaveType
{
    WAVE_TYPE_NWWAV,
    WAVE_TYPE_DSPADPCM,
    WAVE_TYPE_INVALID = -1
};

struct DspAdpcmParam
{
    ut::ResU16 coef[8][2];
    ut::ResU16 predScale;
    ut::ResU16 yn1;
    ut::ResU16 yn2;
};

struct AdshrCurve
{
    u8 attack;
    u8 decay;
    u8 sustain;
    u8 hold;
    u8 release;

    AdshrCurve(u8 a = 0, u8 d = 0, u8 s = 0, u8 h = 0, u8 r = 0)
        : attack(a)
        , decay(d)
        , sustain(s)
        , hold(h)
        , release(r)
    {
    }
};

namespace internal {

struct DspAdpcmLoopParam
{
    ut::ResU16 loopPredScale;
    ut::ResU16 loopYn1;
    ut::ResU16 loopYn2;
};

struct WaveInfo
{
    SampleFormat sampleFormat;
    bool loopFlag;
    s32 channelCount;
    s32 sampleRate;
    u32 loopStartFrame;
    u32 loopEndFrame;
    u32 originalLoopStartFrame; // v0.1.2.0

    struct ChannelParam
    {
        const void* dataAddress;
        DspAdpcmParam adpcmParam;
        DspAdpcmLoopParam adpcmLoopParam;
    } channelParam[WAVE_CHANNEL_MAX];

    sead::Endian::Types endian = sead::Endian::eBig; // Custom

    void Dump()
    {
    #ifdef NW_CONSOLE_ENABLE
        SEAD_PRINT("[WaveInfo] fmt(%d) loop(%d) ch(%d) rate(%d) LS(%d) LE(%d)\n",
            sampleFormat, loopFlag, channelCount, sampleRate,
            loopStartFrame, loopEndFrame);

        for (s32 i = 0; i < channelCount; i++)
        {
            const ChannelParam& p = channelParam[i];
            const DspAdpcmParam& adpcm = p.adpcmParam;
            const DspAdpcmLoopParam& loop = p.adpcmLoopParam;

            SEAD_PRINT("  %d/%d addr(%p)\n       ps(%5d)  yn1(%5d)  yn2(%5d)\n      lps(%5d) lyn1(%5d) lyn2(%5d)\n",
                    i, channelCount-1, p.dataAddress, (u16)adpcm.predScale, (u16)adpcm.yn1, (u16)adpcm.yn2,
                    (u16)loop.loopPredScale, (u16)loop.loopYn1, (u16)loop.loopYn2);

            SEAD_PRINT("      coef %5d %5d %5d %5d %5d %5d %5d %5d\n",
                    (u16)adpcm.coef[0][0], (u16)adpcm.coef[0][1], (u16)adpcm.coef[1][0], (u16)adpcm.coef[1][1],
                    (u16)adpcm.coef[2][0], (u16)adpcm.coef[2][1], (u16)adpcm.coef[3][0], (u16)adpcm.coef[3][1]);

            SEAD_PRINT("           %5d %5d %5d %5d %5d %5d %5d %5d\n",
                    (u16)adpcm.coef[4][0], (u16)adpcm.coef[4][1], (u16)adpcm.coef[5][0], (u16)adpcm.coef[5][1],
                    (u16)adpcm.coef[6][0], (u16)adpcm.coef[6][1], (u16)adpcm.coef[7][0], (u16)adpcm.coef[7][1]);
        }
    #endif // NW_CONSOLE_ENABLE
    }
};

void GetWaveInfoFromWaveFile(WaveInfo* out, const WaveFile& waveFile, s32 channelIdx = -1);
void ConvertWaveInfo(::snd::internal::WaveInfo* out, const WaveInfo& waveInfo);

} // namespace nw::snd::internal

} } // namespace nw::snd
