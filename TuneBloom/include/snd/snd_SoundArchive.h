#pragma once

#include <snd/snd_Config.h>
#include <snd/snd_Global.h>
#include <snd/snd_Util.h>

#include <basis/seadTypes.h>
#include <prim/seadMemUtil.h>

namespace nw { namespace snd {

class SoundArchive
{
public:
    typedef u32 ItemId;

    static const ItemId INVALID_ID = 0xFFFFFFFF;

    static const s32 USER_PARAM_INDEX_MAX = 3;

    enum SoundType
    {
        SOUND_TYPE_INVALID = 0,

        SOUND_TYPE_SEQ,
        SOUND_TYPE_STRM,
        SOUND_TYPE_WAVE
    };

    enum StreamFileType
    {
        STREAM_FILE_TYPE_INVALID = 0,

        STREAM_FILE_TYPE_NW_STREAM_BINARY,

        STREAM_FILE_TYPE_ADTS
    };

    typedef ItemId FileId;

    typedef ItemId StringId;

    static const u32 SEQ_BANK_MAX = nw::snd::SEQ_BANK_MAX;

    static const u32 STRM_TRACK_NUM = internal::STRM_TRACK_NUM;

    struct StreamTrackInfo
    {
        u8 volume;
        u8 pan;
        u8 span;
        u8 flags;
        u8 mainSend;
        u8 fxSend[AUX_BUS_NUM];
        u8 lpfFreq;
        u8 biquadType;
        u8 biquadValue;
        u8 channelCount;
        s8 globalChannelIndex[WAVE_CHANNEL_MAX];

        StreamTrackInfo()
            : volume(0)
            , pan(0)
            , span(0)
            , flags(0)
            , mainSend(127)
            , lpfFreq(64)
            , biquadType(0)
            , biquadValue(0)
            , channelCount(0)
        {
            sead::MemUtil::fill(fxSend, 0, sizeof(u8) * AUX_BUS_NUM);
            sead::MemUtil::fill(globalChannelIndex, -1, sizeof(s8) * WAVE_CHANNEL_MAX);
        }
    };

public:
    SoundArchive()
    {
    }

    static ItemId GetSoundIdFromIndex(u32 index)
    {
        return internal::Util::GetMaskedItemId(index, internal::ItemType_Sound);
    }

    static ItemId GetSoundGroupIdFromIndex(u32 index)
    {
        return internal::Util::GetMaskedItemId(index, internal::ItemType_SoundGroup);
    }

    static ItemId GetBankIdFromIndex(u32 index)
    {
        return internal::Util::GetMaskedItemId(index, internal::ItemType_Bank);
    }

    static ItemId GetPlayerIdFromIndex(u32 index)
    {
        return internal::Util::GetMaskedItemId(index, internal::ItemType_Player);
    }

    static ItemId GetWaveArchiveIdFromIndex(u32 index)
    {
        return internal::Util::GetMaskedItemId(index, internal::ItemType_WaveArchive);
    }

    static ItemId GetGroupIdFromIndex(u32 index)
    {
        return internal::Util::GetMaskedItemId(index, internal::ItemType_Group);
    }
};

} } // namespace nw::snd