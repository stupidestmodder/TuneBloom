#pragma once

#include <snd/snd_ElementType.h>
#include <snd/snd_Global.h>
#include <snd/snd_Util.h>

namespace nw { namespace snd { namespace internal {

struct WaveFile
{
    enum EncodeMethod
    {
        PCM8 = 0,
        PCM16,
        DSP_ADPCM,
        IMA_ADPCM
    };

    struct InfoBlock;
    struct ChannelInfo;
    struct DspAdpcmInfo;
    struct DataBlock;

    struct FileHeader : public Util::SoundFileHeader
    {
        const InfoBlock* GetInfoBlock() const
        {
            return reinterpret_cast<const InfoBlock*>(GetBlock(ElementType_WaveFile_InfoBlock));
        }
        const DataBlock* GetDataBlock() const
        {
            return reinterpret_cast<const DataBlock*>(GetBlock(ElementType_WaveFile_DataBlock));
        }
    };

    struct InfoBlockBody
    {
        u8 encoding;
        u8 isLoop;
        u16 padding;
        nw::ut::ResU32 sampleRate;
        nw::ut::ResU32 loopStartFrame;
        nw::ut::ResU32 loopEndFrame;
        nw::ut::ResU32 originalLoopStartFrame; // v0.1.2.0
        Util::ReferenceTable channelInfoReferenceTable;

        inline s32 GetChannelCount() const { return channelInfoReferenceTable.count; }

        const ChannelInfo& GetChannelInfo(s32 channelIndex) const
        {
            SEAD_ASSERT(channelIndex < GetChannelCount());
            return *reinterpret_cast<const ChannelInfo*>(channelInfoReferenceTable.GetReferedItem(channelIndex));
        }
    };

    struct InfoBlock
    {
        ut::BinaryBlockHeader header;
        InfoBlockBody body;
    };

    struct ChannelInfo
    {
        Util::Reference referToSamples;
        Util::Reference referToAdpcmInfo;
        nw::ut::ResU32 reserved;

        const void* GetSamplesAddress(const void* dataBlockBodyAddress) const
        {
            return reinterpret_cast<const void*>(sead::PtrUtil::addOffset(dataBlockBodyAddress, referToSamples.offset));
        }

        const DspAdpcmInfo& GetDspAdpcmInfo() const
        {
            return *reinterpret_cast<const DspAdpcmInfo*>(sead::PtrUtil::addOffset(this, referToAdpcmInfo.offset));
        }
    };

    struct DspAdpcmInfo
    {
        DspAdpcmParam adpcmParam;
        DspAdpcmLoopParam adpcmLoopParam;
    };

    struct DataBlock
    {
        ut::BinaryBlockHeader header;
        union
        {
            s8  pcm8[1];
            s16 pcm16[1];
            u8  byte[1];
        };
    };
};

} } } // namespace nw::snd::internal
