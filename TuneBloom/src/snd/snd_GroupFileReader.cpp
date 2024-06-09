#include <snd/snd_GroupFileReader.h>

#include <prim/seadMemUtil.h>

namespace nw { namespace snd { namespace internal {

GroupFileReader::GroupFileReader(const void* groupFile)
    : mInfoBlockBody(nullptr)
    , mFileBlockBody(nullptr)
    , mInfoExBlockBody(nullptr)
{
    SEAD_ASSERT(groupFile);

    {
        const ut::BinaryFileHeader* header = reinterpret_cast<const ut::BinaryFileHeader*>(groupFile);

        //if (sead::MemUtil::compare(header->signature, "CGRP", 4) != 0)
        if (sead::MemUtil::compare(header->signature, "FGRP", 4) != 0)
        {
            SEAD_ASSERT_MSG(false, "not a GROUP file");
            return;
        }

        //if (false)
        if (header->version != 0x00010000)
        {
            SEAD_ASSERT_MSG(false, "GROUP version not supported (0x%08X)", (u32)header->version);
            return;
        }
    }

    const GroupFile::FileHeader* header = static_cast<const GroupFile::FileHeader*>(groupFile);

    const GroupFile::InfoBlock* infoBlock = header->GetInfoBlock();
    const GroupFile::FileBlock* fileBlock = header->GetFileBlock();
    const GroupFile::InfoExBlock* infoExBlock = header->GetInfoExBlock();

    if (!infoBlock)
    {
        SEAD_ASSERT_MSG(false, "GROUP: INFO block is missing");
        return;
    }

    if (!fileBlock)
    {
        SEAD_ASSERT_MSG(false, "GROUP: FILE block is missing");
        return;
    }

    if (sead::MemUtil::compare(infoBlock->header.kind, "INFO", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "GROUP: INFO block is invalid");
        return;
    }

    if (sead::MemUtil::compare(fileBlock->header.kind, "FILE", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "GROUP: FILE block is invalid");
        return;
    }

    if (infoExBlock)
    {
        if (sead::MemUtil::compare(infoExBlock->header.kind, "INFX", 4) != 0)
        {
            SEAD_ASSERT_MSG(false, "GROUP: INFX block is invalid");
            return;
        }

        mInfoExBlockBody = &(infoExBlock->body);
    }

    mInfoBlockBody = &(infoBlock->body);
    mFileBlockBody = &(fileBlock->body);
}

bool GroupFileReader::ReadGroupItemLocationInfo(GroupItemLocationInfo* out, u32 index) const
{
    SEAD_ASSERT(out);

    if (!mInfoBlockBody)
        return false;

    const GroupFile::GroupItemInfo* groupItemInfo = mInfoBlockBody->GetGroupItemInfo(index);
    if (!groupItemInfo)
        return false;

    out->fileId = groupItemInfo->fileId;
    out->address = groupItemInfo->GetFileLocation(mFileBlockBody);

    out->fileSize = groupItemInfo->embeddedItemInfo.size; // Custom

    return true;
}

bool GroupFileReader::ReadGroupItemInfoEx(GroupFile::GroupItemInfoEx* out, u32 index) const
{
    SEAD_ASSERT(out);

    if (!mInfoExBlockBody)
        return false;

    const GroupFile::GroupItemInfoEx* groupItemInfoEx = mInfoExBlockBody->GetGroupItemInfoEx(index);
    if (!groupItemInfoEx)
        return false;

    *out = *groupItemInfoEx; // Copy
    return true;
}

} } } // namespace nw::snd::internal
