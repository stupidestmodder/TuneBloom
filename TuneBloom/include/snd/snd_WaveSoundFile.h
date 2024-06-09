#pragma once

#include <snd/snd_ElementType.h>
#include <snd/snd_Global.h>
#include <snd/snd_Util.h>

namespace nw { namespace snd { namespace internal {

const u8 WSD_DEFAULT_PAN = 64;
const s8 WSD_DEFAULT_SURROUND_PAN = 0;
const f32 WSD_DEFAULT_PITCH = 1.0f;
const u8 WSD_DEFAULT_MAIN_SEND = 127;
const u8 WSD_DEFAULT_FX_SEND = 0;
const AdshrCurve WSD_DEFAULT_ADSHR_CURVE(
    127, // u8 attack
    127, // u8 decay
    127, // u8 sustain
    127, // u8 hold
    127  // u8 release
);
const u8 WSD_DEFAULT_LPF_FREQ = 64; // No filter is applied.
const u8 WSD_DEFAULT_BIQUAD_TYPE = 0; // No filter is yet used.
const u8 WSD_DEFAULT_BIQUAD_VALUE = 0; // No filter is applied.
const u8 WSD_DEFAULT_KEY = 64;
const u8 WSD_DEFAULT_VOLUME = 96;

enum WaveSoundInfoBitFlagWsd
{
    WAVE_SOUND_INFO_PAN = 0x00,
    WAVE_SOUND_INFO_PITCH,
    WAVE_SOUND_INFO_FILTER,
    WAVE_SOUND_INFO_SEND = 0x08,
    WAVE_SOUND_INFO_ENVELOPE,
    //WAVE_SOUND_INFO_RANDOMIZER  // Not implemented.
};

enum NoteInfoBitFlag
{
    NOTE_INFO_KEY = 0x00,
    NOTE_INFO_VOLUME,
    NOTE_INFO_PAN,
    NOTE_INFO_PITCH,
    NOTE_INFO_SEND = 0x08,
    NOTE_INFO_ENVELOPE,
    //NOTE_INFO_RANDOMIZER,   // Not implemented.
    //NOTE_INFO_LFO           // Not implemented.
};

struct SendValueWsd
{
    u8 mainSend;
    Util::Table<u8, u8> fxSend;
};

struct WaveSoundFile
{
    struct InfoBlock;
    struct FileHeader : public Util::SoundFileHeader
    {
        const InfoBlock* GetInfoBlock() const
        {
            return reinterpret_cast<const InfoBlock*>(GetBlock(ElementType_WaveSoundFile_InfoBlock));
        }
    };

    struct WaveSoundData;

    struct InfoBlockBody
    {
        Util::Reference toWaveIdTable;
        Util::Reference toWaveSoundDataReferenceTable;

        const Util::WaveIdTable& GetWaveIdTable() const
        {
            return *reinterpret_cast<const Util::WaveIdTable*>(sead::PtrUtil::addOffset(this, toWaveIdTable.offset));
        }
        const Util::ReferenceTable& GetWaveSoundDataReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toWaveSoundDataReferenceTable.offset));
        }

        u32 GetWaveIdCount() const
        {
            return GetWaveIdTable().GetCount();
        }
        u32 GetWaveSoundCount() const
        {
            return GetWaveSoundDataReferenceTable().count;
        }

        const Util::WaveId* GetWaveId(u32 index) const
        {
            return GetWaveIdTable().GetWaveId(index);
        }
        const WaveSoundData& GetWaveSoundData(u32 index) const
        {
            SEAD_ASSERT(index < GetWaveSoundCount());

            const void* pWaveSoundData = GetWaveSoundDataReferenceTable().GetReferedItem(index, ElementType_WaveSoundFile_WaveSoundMetaData);
            SEAD_ASSERT(pWaveSoundData);

            return *reinterpret_cast<const WaveSoundData*>(pWaveSoundData);
        }
    };

    struct InfoBlock
    {
        ut::BinaryBlockHeader header;
        InfoBlockBody body;
    };

    struct WaveSoundInfo;
    struct TrackInfo;
    struct NoteInfo;

    struct WaveSoundData
    {
        Util::Reference toWaveSoundInfo;
        Util::Reference toTrackInfoReferenceTable;
        Util::Reference toNoteInfoReferenceTable;

        const WaveSoundInfo& GetWaveSoundInfo() const
        {
            SEAD_ASSERT(toWaveSoundInfo.IsValidTypeId(ElementType_WaveSoundFile_WaveSoundInfo));

            return *reinterpret_cast<const WaveSoundInfo*>(sead::PtrUtil::addOffset(this, toWaveSoundInfo.offset));
        }

        const Util::ReferenceTable& GetTrackInfoReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toTrackInfoReferenceTable.offset));
        }
        const Util::ReferenceTable& GetNoteInfoReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toNoteInfoReferenceTable.offset));
        }

        u32 GetTrackCount() const
        {
            return GetTrackInfoReferenceTable().count;
        }
        u32 GetNoteCount() const
        {
            return GetNoteInfoReferenceTable().count;
        }

        const TrackInfo& GetTrackInfo(u32 index) const
        {
            SEAD_ASSERT(index < GetTrackCount());

            const void* pTrackInfo = GetTrackInfoReferenceTable().GetReferedItem(index, ElementType_WaveSoundFile_TrackInfo);
            SEAD_ASSERT(pTrackInfo);

            return *reinterpret_cast<const TrackInfo*>(pTrackInfo);
        }
        const NoteInfo& GetNoteInfo(u32 index) const
        {
            SEAD_ASSERT(index < GetNoteCount());

            const void* pNoteInfo = GetNoteInfoReferenceTable().GetReferedItem(index, ElementType_WaveSoundFile_NoteInfo);
            SEAD_ASSERT(pNoteInfo);

            return *reinterpret_cast<const NoteInfo*>(pNoteInfo);
        }
    };

    struct WaveSoundInfo
    {
        Util::BitFlag optionParameter;

        u8 GetPan() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_SOUND_INFO_PAN);
            if (!result)
                return WSD_DEFAULT_PAN;

            return Util::DevideBy8bit(value, 0);
        }

        s8 GetSurroundPan() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_SOUND_INFO_PAN);
            if (!result)
                return WSD_DEFAULT_SURROUND_PAN;

            return static_cast<s8>(Util::DevideBy8bit(value, 1));
        }

        f32 GetPitch() const
        {
            f32 value;
            bool result = optionParameter.GetValueF32(&value, WAVE_SOUND_INFO_PITCH);
            if (!result)
                return WSD_DEFAULT_PITCH;

            return value;
        }

        void GetSendValue(u8* mainSend, u8* fxSend, u8 fxSendCount) const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_SOUND_INFO_SEND);
            if (!result)
            {
                *mainSend = WSD_DEFAULT_MAIN_SEND;
                for (s32 i = 0; i < fxSendCount; i++)
                {
                    fxSend[i] = WSD_DEFAULT_FX_SEND;
                }

                return;
            }

            const SendValueWsd& sendValue = *reinterpret_cast<const SendValueWsd*>(sead::PtrUtil::addOffset(this, value));

            *mainSend = sendValue.mainSend;
            s32 countSize = sendValue.fxSend.count > AUX_BUS_NUM ? AUX_BUS_NUM : sendValue.fxSend.count;

            for (s32 i = 0; i < countSize; i++)
            {
                fxSend[i] = sendValue.fxSend.item[i];
            }
        }

        const AdshrCurve& GetAdshrCurve() const
        {
            u32 offsetToReference;
            bool result = optionParameter.GetValue(&offsetToReference, WAVE_SOUND_INFO_ENVELOPE);
            if (!result)
                return WSD_DEFAULT_ADSHR_CURVE;

            const Util::Reference& ref = *reinterpret_cast<const Util::Reference*>(sead::PtrUtil::addOffset(this, offsetToReference));
            return *reinterpret_cast<const AdshrCurve*>(sead::PtrUtil::addOffset(&ref, ref.offset));
        }

        // v0.1.1.0
        u8 GetLpfFreq() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_SOUND_INFO_FILTER);
            if (!result)
                return WSD_DEFAULT_LPF_FREQ;

            return Util::DevideBy8bit(value, 0);
        }

        // v0.1.1.0
        u8 GetBiquadType() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_SOUND_INFO_FILTER);
            if (!result)
                return WSD_DEFAULT_BIQUAD_TYPE;

            return Util::DevideBy8bit(value, 1);
        }

        // v0.1.1.0
        u8 GetBiquadValue() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, WAVE_SOUND_INFO_FILTER);
            if (!result)
                return WSD_DEFAULT_BIQUAD_VALUE;

            return Util::DevideBy8bit(value, 2);
        }
    };

    struct NoteEvent
    {
        ut::ResF32 position;
        ut::ResF32 length;
        ut::ResU32 noteIndex;
        ut::ResU32 reserved;
    };

    struct TrackInfo
    {
        Util::Reference toNoteEventReferenceTable;

        const Util::ReferenceTable& GetNoteEventReferenceTable() const
        {
            return *reinterpret_cast<const Util::ReferenceTable*>(sead::PtrUtil::addOffset(this, toNoteEventReferenceTable.offset));
        }

        u32 GetNoteEventCount() const
        {
            return GetNoteEventReferenceTable().count;
        }

        const NoteEvent& GetNoteEvent(u32 index) const
        {
            SEAD_ASSERT(index < GetNoteEventCount());

            const void* pNoteEvent = GetNoteEventReferenceTable().GetReferedItem(index, ElementType_WaveSoundFile_NoteEvent);
            SEAD_ASSERT(pNoteEvent);

            return *reinterpret_cast<const NoteEvent*>(pNoteEvent);
        }
    };

    struct NoteInfo
    {
        ut::ResU32 waveIdTableIndex;
        Util::BitFlag optionParameter;

        u8 GetOriginalKey() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, NOTE_INFO_KEY);
            if (!result)
                return WSD_DEFAULT_KEY;

            return Util::DevideBy8bit(value, 0);
        }

        u8 GetVolume() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, NOTE_INFO_VOLUME);
            if (!result)
                return WSD_DEFAULT_VOLUME;

            return Util::DevideBy8bit(value, 0);
        }

        u8 GetPan() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, NOTE_INFO_PAN);
            if (!result)
                return WSD_DEFAULT_PAN;

            return Util::DevideBy8bit(value, 0);
        }

        u8 GetSurroundPan() const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, NOTE_INFO_PAN);
            if (!result)
                return WSD_DEFAULT_SURROUND_PAN;

            return static_cast<s8>(Util::DevideBy8bit(value, 1));
        }

        f32 GetPitch() const
        {
            f32 value;
            bool result = optionParameter.GetValueF32(&value, NOTE_INFO_PITCH);
            if (!result)
                return WSD_DEFAULT_PITCH;

            return value;
        }

        void GetSendValue(u8* mainSend, u8* fxSend[], u8 fxSendCount) const
        {
            u32 value;
            bool result = optionParameter.GetValue(&value, NOTE_INFO_SEND);
            if (!result)
            {
                *mainSend = WSD_DEFAULT_MAIN_SEND;
                for (s32 i = 0; i < fxSendCount; i++)
                {
                    *fxSend[i] = WSD_DEFAULT_FX_SEND;
                }

                return;
            }

            const SendValueWsd& sendValue = *reinterpret_cast<const SendValueWsd*>(sead::PtrUtil::addOffset(this, value));

            SEAD_ASSERT(fxSendCount <= sendValue.fxSend.count);
            *mainSend = sendValue.mainSend;
            s32 countSize = sendValue.fxSend.count > AUX_BUS_NUM ? AUX_BUS_NUM : sendValue.fxSend.count;

            for (s32 i = 0; i < countSize; i++)
            {
                *fxSend[i] = sendValue.fxSend.item[i];
            }
        }

        const AdshrCurve& GetAdshrCurve() const
        {
            u32 offsetToReference;
            bool result = optionParameter.GetValue(&offsetToReference, NOTE_INFO_ENVELOPE);
            if (!result)
                return WSD_DEFAULT_ADSHR_CURVE;

            const Util::Reference& ref = *reinterpret_cast<const Util::Reference*>(sead::PtrUtil::addOffset(this, offsetToReference));
            return *reinterpret_cast<const AdshrCurve*>(sead::PtrUtil::addOffset(&ref, ref.offset));
        }
    };
};

} } } // namespace nw::snd::internal
