#include <bfsar/SoundDataMgr.h>

#include <snd/snd_GroupFileReader.h>

#include <basis/seadWarning.h>

SoundDataMgr::SoundDataMgr()
    : mSoundArchive(nullptr)
{
}

void SoundDataMgr::init(const nw::snd::MemorySoundArchive* arc, sead::Heap* heap)
{
    mSoundArchive = arc;

    u32 fileCount = arc->detail_GetFileCount();
    if (fileCount == 0)
        return;

    mFileTable.allocBuffer(fileCount, heap);
    mFileTable.fill(nullptr);
}

void SoundDataMgr::deinit()
{
    mFileTable.freeBuffer();
    mSoundArchive = nullptr;
}

const void* SoundDataMgr::detail_GetFileAddress(nw::snd::SoundArchive::FileId fileId) const
{
    // Query the sound archive
    {
        const void* addr = GetFileAddressFromSoundArchive(fileId);
        if (addr)
            return addr;
    }

    // Reference the file address table
    {
        const void* fileData = GetFileAddressFromTable(fileId);
        if (fileData)
            return fileData;
    }

    return nullptr;
}

const void* SoundDataMgr::GetFileAddressFromSoundArchive(nw::snd::SoundArchive::FileId fileId) const
{
    if (!mSoundArchive)
        return nullptr;

    return mSoundArchive->detail_GetFileAddress(fileId);
}

const void* SoundDataMgr::GetFileAddressFromTable(nw::snd::SoundArchive::FileId fileId) const
{
    if (!mFileTable.isBufferReady())
    {
        SEAD_WARNING("Failed to SoundDataMgr::GetFileAddress because file table is not allocated.\n");
        return nullptr;
    }

    if (fileId >= mFileTable.size())
        return nullptr;

    return mFileTable[fileId];
}

bool SoundDataMgr::SetFileAddressInGroupFile(const void* address)
{
    if (!address)
    {
        SEAD_ASSERT(false);
        return false;
    }

    nw::snd::internal::GroupFileReader reader(address);
    u32 groupItemCount = reader.GetGroupItemCount();
    for (u32 i = 0; i < groupItemCount; i++)
    {
        nw::snd::internal::GroupItemLocationInfo info;
        if (!reader.ReadGroupItemLocationInfo(&info, i))
            return false;

        if (!info.address)
            return false;

        SetFileAddressToTable(info.fileId, info.address);
    }

    return true;
}

const void* SoundDataMgr::SetFileAddressToTable(nw::snd::SoundArchive::FileId fileId, const void* address)
{
    if (!mFileTable.isBufferReady())
    {
        SEAD_WARNING("Failed to SoundDataMgr::GetFileAddress because file table is not allocated.\n");
        return nullptr;
    }

    SEAD_ASSERT(0 <= fileId && fileId <= mFileTable.size() - 1);

    const void* preAddress = mFileTable[fileId];
    mFileTable[fileId] = address;
    return preAddress;
}
