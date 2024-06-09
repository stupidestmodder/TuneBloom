#include "snd/DecodeAdpcm.h"

namespace snd { namespace internal {

void DecodeDspAdpcm(
        u32 playPosition,                   // Decode start position counted from adpcmData.
        AdpcmContext& context,              // ADPCM context for decoding
        const AdpcmParam& param,            // ADPCM parameters for decoding
        const void* adpcmData,              // ADPCM encoded data
        u32 decodeSamples,                  // Decode sample count
        s16* dest)                          // Decode data output destination
{
    u32 frame = playPosition / 14;
    u32 frameFrac = playPosition - frame * 14;
    const u8* frameBegin = reinterpret_cast<const u8*>(reinterpret_cast<u32>(adpcmData) + (frame * 8));

    s32 pred  = static_cast<s32>(context.pred_scale >> 4);
    s32 scale = static_cast<s32>(context.pred_scale & 0xF);

    for (u32 i = 0; i < decodeSamples; i++)
    {
        if (frameFrac == 0)
        {
            const u8 pred_scale = *frameBegin;
            context.pred_scale = pred_scale;
            pred = (pred_scale >> 4);
            scale = (pred_scale & 0xF);
        }

        u8 code = frameBegin[frameFrac / 2 + 1];
        if (frameFrac & 0x1)
            code &= 0xF;
        else
            code >>= 4;

        s16 nibble = code;
        nibble <<= 12;
        nibble >>= 1;

        s16 a1   = static_cast<s16>(param.coef[pred][0]);
        s16 a2   = static_cast<s16>(param.coef[pred][1]);
        s16 gain = static_cast<s16>(1 << scale);

        s32 val = a1 * static_cast<s16>(context.yn1);
        val  += a2 * static_cast<s16>(context.yn2);
        val  += gain * nibble;
        val >>= 10;
        val  += 1;
        val >>= 1;

        if (val > 32767)
            val = 32767;
        else if (val < -32768)
            val = -32768;

        s16 smp = static_cast<s16>(val);

        context.yn2 = context.yn1;
        context.yn1 = smp;

        *dest = smp;
        ++dest;

        frameFrac++;
        if (frameFrac == 14)
        {
            frameFrac = 0;
            frameBegin += 8;
        }
    }
}

} } // namespace snd::internal
