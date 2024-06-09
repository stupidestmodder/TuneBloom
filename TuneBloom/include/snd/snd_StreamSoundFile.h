#pragma once

#include <snd/snd_ElementType.h>
#include <snd/snd_Util.h>

namespace nw { namespace snd { namespace internal {

struct StreamSoundFile
{
    struct InfoBlock;
    struct InfoBlockBody;

    struct DataBlock;

    struct FileHeader : public ut::BinaryFileHeader
    {
    public:
        static const s32 BLOCK_SIZE = 4;

        Util::ReferenceWithSize toBlocks[BLOCK_SIZE];

        bool HasSeekBlock() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_SeekBlock);

            return p != nullptr;
        }
        bool HasRegionBlock() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_RegionBlock);

            return p != nullptr;
        }

        u32 GetInfoBlockSize() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_InfoBlock);

            SEAD_ASSERT(p);
            return p->size;
        }
        u32 GetSeekBlockSize() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_SeekBlock);

            SEAD_ASSERT(p);
            return p->size;
        }
        u32 GetDataBlockSize() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_DataBlock);

            SEAD_ASSERT(p);
            return p->size;
        }
        u32 GetRegionBlockSize() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_RegionBlock);

            SEAD_ASSERT(p);
            return p->size;
        }

        u32 GetInfoBlockOffset() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_InfoBlock);

            SEAD_ASSERT(p);
            return p->offset;
        }
        u32 GetSeekBlockOffset() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_SeekBlock);

            SEAD_ASSERT(p);
            return p->offset;
        }
        u32 GetDataBlockOffset() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_DataBlock);

            SEAD_ASSERT(p);
            return p->offset;
        }
        u32 GetRegionBlockOffset() const
        {
            const Util::ReferenceWithSize* p = GetReferenceBy(ElementType_StreamSoundFile_RegionBlock);

            SEAD_ASSERT(p);
            return p->offset;
        }

        const InfoBlock* GetInfoBlock() const
        {
            return static_cast<const InfoBlock*>(sead::PtrUtil::addOffset(this, GetInfoBlockOffset()));
        }

        // Custom
        const DataBlock* GetDataBlock() const
        {
            return static_cast<const DataBlock*>(sead::PtrUtil::addOffset(this, GetDataBlockOffset()));
        }

        const Util::ReferenceWithSize* GetReferenceBy(u16 typeId) const
        {
            for (s32 i = 0; i < dataBlocks; i++)
            {
                if (toBlocks[i].typeId == typeId)
                    return &toBlocks[i];
            }

            return nullptr;
        }
    };

    struct StreamSoundInfo;
    struct TrackInfoTable;
    struct ChannelInfoTable;

    struct InfoBlockBody
    {
        Util::Reference toStreamSoundInfo;
        Util::Reference toTrackInfoTable;
        Util::Reference toChannelInfoTable;

        const StreamSoundInfo* GetStreamSoundInfo() const
        {
            if (toStreamSoundInfo.typeId != ElementType_StreamSoundFile_StreamSoundInfo)
                return nullptr;

            return static_cast<const StreamSoundInfo*>(sead::PtrUtil::addOffset(this, toStreamSoundInfo.offset));
        }

        const TrackInfoTable* GetTrackInfoTable() const
        {
            if (toTrackInfoTable.typeId != ElementType_Table_ReferenceTable)
                return nullptr;

            return static_cast<const TrackInfoTable*>(sead::PtrUtil::addOffset(this, toTrackInfoTable.offset));
        }

        const ChannelInfoTable* GetChannelInfoTable() const
        {
            if (toChannelInfoTable.typeId != ElementType_Table_ReferenceTable)
                return nullptr;

            return static_cast<const ChannelInfoTable*>(sead::PtrUtil::addOffset(this, toChannelInfoTable.offset));
        }
    };

    struct InfoBlock
    {
        ut::BinaryBlockHeader header;
        InfoBlockBody body;
    };

    struct StreamSoundInfo
    {
        u8 encodeMethod;
        bool isLoop;
        u8 channelCount;
        u8 regionCount; // How many RegionInfo's are in the REGN block?
        ut::ResU32 sampleRate;
        ut::ResU32 loopStart;
        ut::ResU32 frameCount;
        ut::ResU32 blockCount;

        ut::ResU32 oneBlockBytes;
        ut::ResU32 oneBlockSamples;

        ut::ResU32 lastBlockBytes;
        ut::ResU32 lastBlockSamples;
        ut::ResU32 lastBlockPaddedBytes;

        ut::ResU32 sizeofSeekInfoAtom; // The size of the seek information (1 channel's worth)
        ut::ResU32 seekInfoIntervalSamples;

        Util::Reference sampleDataOffset; // The offset to the sample data from the start of the DATA block body

        ut::ResU16 regionInfoBytes; // Size for each RegionInfo (in bytes).
        u16 padding;
        Util::Reference regionDataOffset; // The offset from the start of the REGN block body to RegionInfo[].

        ut::ResU32 originalLoopStart; // v0.4.0.0
        ut::ResU32 originalLoopEnd;   // v0.4.0.0
    };

    struct TrackInfo;

    struct TrackInfoTable
    {
        Util::ReferenceTable table;

        const TrackInfo* GetTrackInfo(u32 index) const
        {
            return static_cast<const TrackInfo*>(table.GetReferedItem(index, ElementType_StreamSoundFile_TrackInfo));
        }

        u32 GetTrackCount() const { return table.count; }
    };

    struct GlobalChannelIndexTable;

    struct TrackInfo
    {
        u8 volume;
        u8 pan;
        u8 span;
        u8 flags;
        Util::Reference toGlobalChannelIndexTable;

        u32 GetTrackChannelCount() const
        {
            return GetGlobalChannelIndexTable().GetCount();
        }

        u8 GetGlobalChannelIndex(u32 index) const
        {
            return GetGlobalChannelIndexTable().GetGlobalIndex(index);
        }

        const GlobalChannelIndexTable& GetGlobalChannelIndexTable() const
        {
            return *reinterpret_cast<const GlobalChannelIndexTable*>(sead::PtrUtil::addOffset(this, toGlobalChannelIndexTable.offset));
        }
    };

    struct GlobalChannelIndexTable
    {
        Util::Table<u8> table;

        u32 GetCount() const { return table.count; }

        u8 GetGlobalIndex(u32 index) const
        {
            SEAD_ASSERT(0 <= index <= table.count);
            return table.item[index];
        }
    };
    // Can't check size

    struct ChannelInfo;

    struct ChannelInfoTable
    {
        Util::ReferenceTable table;

        u32 GetChannelCount() const { return table.count; }

        const ChannelInfo* GetChannelInfo(u32 index) const
        {
            SEAD_ASSERT(index < table.count);

            return static_cast<const ChannelInfo*>(
                table.GetReferedItem(
                    index,
                    ElementType_StreamSoundFile_ChannelInfo
                )
            );
        }
    };

    struct DspAdpcmChannelInfo; // Depending on the encoding, there might be others.

    struct ChannelInfo
    {
        Util::Reference toDetailChannelInfo;

        const DspAdpcmChannelInfo* GetDspAdpcmChannelInfo() const
        {
            if (!toDetailChannelInfo.IsValidTypeId(ElementType_Codec_DspAdpcmInfo))
                return nullptr;

            return reinterpret_cast<const DspAdpcmChannelInfo*>(sead::PtrUtil::addOffset(this, toDetailChannelInfo.offset));
        }
    };

    struct DspAdpcmChannelInfo
    {
        DspAdpcmParam param;
        DspAdpcmLoopParam loopParam;
    };

    // --------------------------
    // SEEK block (Information for midstream playback)
    struct SeekBlock
    {
        ut::BinaryBlockHeader header;

        // The entire block is not expanded into memory.
        // Because Seek and Read occur via the file stream, the accessor is left up to StreamSoundFileLoader.
    };

    // Custom
    struct DataBlockBody
    {
    };

    // --------------------------
    // DATA block (sample data)
    struct DataBlock
    {
        ut::BinaryBlockHeader header;

        // The entire block is not expanded into memory.
        // Because Seek and Read occur via the file stream, the accessor is left up to StreamSoundFileLoader.

        // Custom
        DataBlockBody body;
    };

    // --------------------------
    // REGN block (region data).
    struct RegionBlock
    {
        ut::BinaryBlockHeader header;
        // After exactly the rest of this block + regionDataOffset, there are regionCount sets of RegionInfo data.
    };

    struct RegionInfo
    {
        ut::ResU32 start;
        ut::ResU32 end;
        DspAdpcmLoopParam adpcmContext[16]; // 16 channels (= 2ch/trk * 8 trk).
        u8 pading[152];                     // Adjust so that each RegionInfo is 256 bytes long.
    };
};

} } } // namespace nw::snd::internal
