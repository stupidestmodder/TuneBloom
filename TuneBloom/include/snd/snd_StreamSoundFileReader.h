#pragma once

#include <snd/snd_Global.h>
#include <snd/snd_StreamSoundFile.h>

#include <prim/seadMemUtil.h>

namespace nw { namespace snd { namespace internal {

class StreamSoundFileReader
{
public:
    struct TrackInfo
    {
        u8 volume;
        u8 pan;
        u8 span;
        u8 flags;
        u8 channelCount;
        u8 globalChannelIndex[WAVE_CHANNEL_MAX];

        TrackInfo()
            : volume(0)
            , pan(0)
            , channelCount(0)
        {
            sead::MemUtil::fill(globalChannelIndex, 0, sizeof(u8) * WAVE_CHANNEL_MAX);
        }
    };

    StreamSoundFileReader();

    void Initialize(const void* streamSoundFile);
    void Finalize();

    bool IsAvailable() const { return mHeader != nullptr; }
    bool IsTrackInfoAvailable() const;
    bool IsOriginalLoopAvailable() const;

    bool IsValidFileHeader(const void* streamSoundFile) const;

    bool ReadStreamSoundInfo(StreamSoundFile::StreamSoundInfo* strmInfo) const;
    bool ReadStreamTrackInfo(TrackInfo* trackInfo, s32 trackIndex) const;
    bool ReadDspAdpcmChannelInfo(DspAdpcmParam* param, DspAdpcmLoopParam* loopParam, s32 channelIndex) const;

    u32 GetChannelCount() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetChannelInfoTable()->GetChannelCount();
    }

    u32 GetTrackCount() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetTrackInfoTable()->GetTrackCount();
    }

    u32 GetSeekBlockOffset() const
    {
        if (IsAvailable() && mHeader->HasSeekBlock())
            return mHeader->GetSeekBlockOffset();

        return 0;
    }

    u32 GetSampleDataOffset() const
    {
        if (IsAvailable())
        {
            u32 result = mHeader->GetDataBlockOffset() +
                sizeof(ut::BinaryBlockHeader) +
                mInfoBlockBody->GetStreamSoundInfo()->sampleDataOffset.offset;

            return result;
        }

        return 0;
    }

    // Return offset from start of file to the position where the data within the REGN block starts.
    // (Also takes block header and starting data position from end of block header into account.)
    u32 GetRegionDataOffset() const
    {
        SEAD_ASSERT(mInfoBlockBody);

        if (IsAvailable() && mHeader->HasRegionBlock())
        {
            u32 result = mHeader->GetRegionBlockOffset() +
                sizeof(ut::BinaryBlockHeader) +
                mInfoBlockBody->GetStreamSoundInfo()->regionDataOffset.offset;

            return result;
        }

        return 0;
    }

    // Return number of bytes per set of region info.
    u32 GetRegionInfoBytes() const
    {
        SEAD_ASSERT(mInfoBlockBody);

        u16 result = mInfoBlockBody->GetStreamSoundInfo()->regionInfoBytes;
        return result;
    }

    const StreamSoundFile::FileHeader* mHeader;
    const StreamSoundFile::InfoBlockBody* mInfoBlockBody;
};

} } } // namespace nw::snd::internal
