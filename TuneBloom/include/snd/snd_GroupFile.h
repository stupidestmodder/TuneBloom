#pragma once

#include <snd/ut/ut_BinaryFileFormat.h>
#include <snd/snd_ElementType.h>
#include <snd/snd_Util.h>

namespace nw { namespace snd { namespace internal {

struct GroupFile
{
    struct InfoBlock;
    struct FileBlock;
    struct InfoExBlock;

    struct FileHeader : public Util::SoundFileHeader
    {
        const InfoBlock* GetInfoBlock() const
        {
            return static_cast<const InfoBlock*>(GetBlock(ElementType_GroupFile_InfoBlock));
        }

        const FileBlock* GetFileBlock() const
        {
            return static_cast<const FileBlock*>(GetBlock(ElementType_GroupFile_FileBlock));
        }

        const InfoExBlock* GetInfoExBlock() const
        {
            return static_cast<const InfoExBlock*>(GetBlock(ElementType_GroupFile_InfoExBlock));
        }
    };
    // Can't check size

    struct GroupItemInfo;

    struct InfoBlockBody
    {
        Util::ReferenceTable referenceTableOfGroupItemInfo;

        u32 GetGroupItemInfoCount() const
        {
            return referenceTableOfGroupItemInfo.count;
        }

        const GroupItemInfo* GetGroupItemInfo(u32 index) const
        {
            if (index >= GetGroupItemInfoCount())
                return nullptr;

            return static_cast<const GroupItemInfo*>(sead::PtrUtil::addOffset(this, referenceTableOfGroupItemInfo.item[index].offset));
        }
    };
    // Can't check size

    struct InfoBlock
    {
        ut::BinaryBlockHeader header;
        InfoBlockBody body;
    };

    struct FileBlockBody;

    struct GroupItemInfo
    {
        ut::ResU32 fileId;
        Util::ReferenceWithSize embeddedItemInfo;

        static const u32 OFFSET_FOR_LINK = 0xFFFFFFFF;
        static const u32 SIZE_FOR_LINK = 0xFFFFFFFF;

        const void* GetFileLocation(const FileBlockBody* fileBlockBody) const
        {
            if (embeddedItemInfo.offset == OFFSET_FOR_LINK)
                return nullptr;

            return sead::PtrUtil::addOffset(fileBlockBody, embeddedItemInfo.offset);
        }
    };
    static_assert(sizeof(GroupItemInfo) == 0x10);

    struct FileBlockBody
    {
    };

    struct FileBlock
    {
        ut::BinaryBlockHeader header;
        FileBlockBody body;
    };

    struct GroupItemInfoEx;

    struct InfoExBlockBody
    {
        Util::ReferenceTable referenceTableOfGroupItemInfoEx;

        u32 GetGroupItemInfoExCount() const
        {
            return referenceTableOfGroupItemInfoEx.count;
        }

        const GroupItemInfoEx* GetGroupItemInfoEx(u32 index) const
        {
            if (index >= GetGroupItemInfoExCount())
                return nullptr;

            return static_cast<const GroupItemInfoEx*>(sead::PtrUtil::addOffset(this, referenceTableOfGroupItemInfoEx.item[index].offset));
        }
    };
    // Can't check size

    struct InfoExBlock
    {
        ut::BinaryBlockHeader header;
        InfoExBlockBody body;
    };

    struct GroupItemInfoEx
    {
        ut::ResU32 itemId;
        ut::ResU32 loadFlag;
    };
    static_assert(sizeof(GroupItemInfoEx) == 0x8);
};

} } } // namespace nw::snd::internal
