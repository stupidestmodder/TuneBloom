#pragma once

#include <snd/snd_ElementType.h>
#include <snd/snd_Util.h>

namespace nw { namespace snd { namespace internal {

struct WaveArchiveFile
{
    struct InfoBlock;
    struct FileBlock;

    static const s32 BLOCK_SIZE = 2;

    struct FileHeader : public ut::BinaryFileHeader
    {
        Util::ReferenceWithSize toBlocks[BLOCK_SIZE];

        const InfoBlock* GetInfoBlock() const
        {
            return reinterpret_cast<const InfoBlock*>(sead::PtrUtil::addOffset(this, GetInfoBlockOffset()));
        }

        const FileBlock* GetFileBlock() const
        {
            return reinterpret_cast<const FileBlock*>(sead::PtrUtil::addOffset(this, GetFileBlockOffset()));
        }

        u32 GetInfoBlockSize() const
        {
            return GetReferenceBy(ElementType_WaveArchiveFile_InfoBlock)->size;
        }
        u32 GetFileBlockSize() const
        {
            return GetReferenceBy(ElementType_WaveArchiveFile_FileBlock)->size;
        }

        u32 GetInfoBlockOffset() const
        {
            return GetReferenceBy(ElementType_WaveArchiveFile_InfoBlock)->offset;
        }
        u32 GetFileBlockOffset() const
        {
            return GetReferenceBy(ElementType_WaveArchiveFile_FileBlock)->offset;
        }

        const Util::ReferenceWithSize* GetReferenceBy(u16 typeId) const
        {
            for (s32 i = 0; i < BLOCK_SIZE; i++)
            {
                if (toBlocks[i].typeId == typeId)
                    return &toBlocks[i];
            }

            SEAD_ASSERT(false);
            return nullptr;
        }
    };

    struct InfoBlockBody
    {
        Util::Table<Util::ReferenceWithSize> table;

        inline u32 GetWaveFileCount() const { return table.count; }

        u32 GetSize(u32 index) const
        {
            SEAD_ASSERT(index < GetWaveFileCount());
            return table.item[index].size;
        }
        u32 GetOffsetFromFileBlockBody(u32 index) const
        {
            SEAD_ASSERT(index < GetWaveFileCount());
            return table.item[index].offset;
        }

        static const u32 INVALID_OFFSET = 0xFFFFFFFF;

    };

    struct InfoBlock
    {
        ut::BinaryBlockHeader header;
        InfoBlockBody body;
    };

    struct FileBlockBody
    {
    };

    struct FileBlock
    {
        ut::BinaryBlockHeader header;
        FileBlockBody body;
    };
};

} } } // namespace nw::snd::internal
