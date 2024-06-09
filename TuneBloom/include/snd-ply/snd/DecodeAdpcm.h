#pragma once

#include "Adpcm.h"

namespace snd { namespace internal {

void DecodeDspAdpcm(
        u32 playPosition,                   // Decode start position counted from adpcmData.
        AdpcmContext& context,              // ADPCM context for decoding
        const AdpcmParam& param,            // ADPCM parameters for decoding
        const void* adpcmData,              // ADPCM encoded data
        u32 decodeSamples,                  // Decode sample count
        s16* dest                           // Decode data output destination
);

} } // namespace snd::internal
