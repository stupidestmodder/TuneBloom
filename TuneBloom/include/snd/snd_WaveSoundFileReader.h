#pragma once

#include <snd/snd_WaveSoundFile.h>

namespace nw { namespace snd { namespace internal {

struct WaveSoundInfo;
struct WaveSoundNoteInfo;

class WaveSoundFileReader
{
public:
    explicit WaveSoundFileReader(const void* waveSoundFile);

    bool IsAvailable() const { return mHeader != nullptr; }

    u32 GetWaveSoundCount() const;
    u32 GetNoteInfoCount(u32 index) const;
    u32 GetTrackInfoCount(u32 index) const;

    bool ReadWaveSoundInfo(WaveSoundInfo* dst, u32 index) const;
    bool ReadNoteInfo(WaveSoundNoteInfo* dst, u32 index, u32 noteIndex) const;

    bool IsFilterSupportedVersion() const;

    //? Added
    const WaveSoundFile::WaveSoundInfo& GetWaveSoundInfo(u32 index) const
    {
        return mInfoBlockBody->GetWaveSoundData(index).GetWaveSoundInfo();
    }

    const WaveSoundFile::FileHeader* mHeader;
    const WaveSoundFile::InfoBlockBody* mInfoBlockBody;
};

struct WaveSoundInfo
{
    f32 pitch;
    AdshrCurve adshr;
    u8 pan;
    u8 surroundPan;
    u8 mainSend;
    u8 fxSend[AUX_BUS_NUM];

    u8 lpfFreq; // v0.1.1.0
    u8 biquadType; // v0.1.1.0
    u8 biquadValue; // v0.1.1.0
};

struct WaveSoundNoteInfo
{
    u32 waveArchiveId;
    s32 waveIndex;
    AdshrCurve adshr;
    u8 originalKey;
    u8 pan;
    u8 surroundPan;
    u8 volume;
    f32 pitch;
};

} } } // namespace nw::snd::internal
