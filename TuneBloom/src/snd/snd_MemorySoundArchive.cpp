#include <snd/snd_MemorySoundArchive.h>

#include <snd/snd_GroupFileReader.h>

#include <ui/UI.h>

namespace nw { namespace snd {

MemorySoundArchive::MemorySoundArchive()
    : SoundArchive()
    , mHeader()
    , mStringBlockBody(nullptr)
    , mInfoBlockBody(nullptr)
    , mFileBlockBody(nullptr)
    , mData(nullptr)
{
}

MemorySoundArchive::~MemorySoundArchive()
{
    for (u32 i = 0; i < mExternalGroups.size(); i++)
    {
        if (mExternalGroups[i])
        {
            delete mExternalGroups[i];
        }
    }
}

bool MemorySoundArchive::Initialize(const void* soundArchiveData)
{
    SEAD_ASSERT(soundArchiveData);

    const internal::SoundArchiveFile::FileHeader& header = *reinterpret_cast<const internal::SoundArchiveFile::FileHeader*>(soundArchiveData);

    mHeader = header; // Copy

    const void* infoBlock = sead::PtrUtil::addOffset(soundArchiveData, mHeader.GetInfoBlockOffset());
    if (!CheckBlockCorrupt("BFSAR", "INFO", infoBlock))
    {
        return false;
    }

    mInfoBlockBody = &reinterpret_cast<const internal::SoundArchiveFile::InfoBlock*>(infoBlock)->body;

    if (mHeader.GetStringBlockOffset() != 0xFFFFFFFF && mHeader.GetStringBlockSize() != 0xFFFFFFFF)
    {
        const void* stringBlock = sead::PtrUtil::addOffset(soundArchiveData, mHeader.GetStringBlockOffset());
        if (!CheckBlockCorrupt("BFSAR", "STRG", stringBlock))
        {
            return false;
        }

        mStringBlockBody = &reinterpret_cast<const internal::SoundArchiveFile::StringBlock*>(stringBlock)->body;
    }

    mData = soundArchiveData;

    return true;
}

const internal::Util::Table<ut::ResU32>* MemorySoundArchive::detail_GetWaveArchiveIdTable(ItemId id) const
{
    SEAD_ASSERT(mInfoBlockBody);

    switch (internal::Util::GetItemType(id))
    {
        case internal::ItemType_Sound:
        {
            const internal::SoundArchiveFile::SoundInfo* pInfo = mInfoBlockBody->GetSoundInfo(id);
            SEAD_ASSERT(pInfo);

            if (pInfo->GetSoundType() == SOUND_TYPE_WAVE)
            {
                for (u32 i = 0; i < mInfoBlockBody->GetSoundGroupCount(); i++)
                {
                    const internal::SoundArchiveFile::SoundGroupInfo* pGroupInfo = mInfoBlockBody->GetSoundGroupInfo(
                        internal::Util::GetMaskedItemId(i, internal::ItemType_SoundGroup)
                    );

                    SEAD_ASSERT(pGroupInfo);

                    if (id < pGroupInfo->startId || pGroupInfo->endId < id)
                        continue;

                    const internal::SoundArchiveFile::WaveSoundGroupInfo* pWaveSoundGroupInfo = pGroupInfo->GetWaveSoundGroupInfo();
                    SEAD_ASSERT(pWaveSoundGroupInfo);

                    return pWaveSoundGroupInfo->GetWaveArchiveItemIdTable();
                }
            }

            return nullptr;
        }

        case internal::ItemType_SoundGroup:
        {
            const internal::SoundArchiveFile::SoundGroupInfo* pInfo = mInfoBlockBody->GetSoundGroupInfo(id);
            SEAD_ASSERT(pInfo);

            const internal::SoundArchiveFile::WaveSoundGroupInfo* pWaveSoundGroupInfo = pInfo->GetWaveSoundGroupInfo();
            SEAD_ASSERT(pWaveSoundGroupInfo);

            return pWaveSoundGroupInfo->GetWaveArchiveItemIdTable();
        }

        case internal::ItemType_Bank:
        {
            const internal::SoundArchiveFile::BankInfo* pInfo = mInfoBlockBody->GetBankInfo(id);
            SEAD_ASSERT(pInfo);

            return pInfo->GetWaveArchiveItemIdTable();
        }

        default:
            return nullptr;
    }
}

const void* MemorySoundArchive::detail_GetFileAddress(FileId fileId, u32* outFileSize, u32 variationId, u32* outGroupFileId, u32* outGroupId) const
{
    u32 counter = 0;

    const internal::SoundArchiveFile::FileInfo* fileInfo = detail_GetFileInfo(fileId);

    if (fileInfo)
    {
        const internal::SoundArchiveFile::InternalFileInfo* internalFileInfo = fileInfo->GetInternalFileInfo();

        if (internalFileInfo)
        {
            u32 offsetFromFileBlockHead = internalFileInfo->GetOffsetFromFileBlockHead();

            if (offsetFromFileBlockHead != 0xFFFFFFFF)
            {
                if (counter == variationId)
                {
                    if (outFileSize)
                    {
                        u32 fileSize = internalFileInfo->GetFileSize();
                        SEAD_ASSERT(fileSize != 0xFFFFFFFF);

                        *outFileSize = fileSize;
                    }

                    if (outGroupFileId)
                    {
                        *outGroupFileId = INVALID_ID;
                    }

                    if (outGroupId)
                    {
                        *outGroupId = INVALID_ID;
                    }

                    return sead::PtrUtil::addOffset(mData, offsetFromFileBlockHead + mHeader.GetFileBlockOffset());
                }

                counter++;
            }
        }
    }

    u32 groupCount = GetGroupCount();
    for (u32 i = 0; i < groupCount; i++)
    {
        u32 groupId = GetGroupIdFromIndex(i);
        const internal::SoundArchiveFile::GroupInfo* info = GetGroupInfo(groupId);
        if (info)
        {
            const void* groupFile = nullptr;

            if (info->fileId >= detail_GetFileCount())
            {
                u32 externalFileId = info->fileId - detail_GetFileCount();
                if (externalFileId < mExternalGroups.size())
                {
                    groupFile = mExternalGroups[externalFileId];
                    SEAD_ASSERT(groupFile);

                    if (fileId == info->fileId)
                    {
                        return groupFile;
                    }
                }
            }
            else
            {
                const internal::SoundArchiveFile::FileInfo* groupFileInfo = detail_GetFileInfo(info->fileId);
                if (groupFileInfo && groupFileInfo->GetFileLocationType() != nw::snd::internal::SoundArchiveFile::FILE_LOCATION_TYPE_NONE)
                {
                    const internal::SoundArchiveFile::InternalFileInfo* groupInternalFileInfo = groupFileInfo->GetInternalFileInfo();
                    SEAD_ASSERT(groupInternalFileInfo);

                    u32 offsetFromFileBlockHead = groupInternalFileInfo->GetOffsetFromFileBlockHead();
                    SEAD_ASSERT(offsetFromFileBlockHead != 0xFFFFFFFF);

                    groupFile = sead::PtrUtil::addOffset(mData, offsetFromFileBlockHead + mHeader.GetFileBlockOffset());
                }
            }

            if (groupFile)
            {
                internal::GroupFileReader reader(groupFile);
                u32 groupItemCount = reader.GetGroupItemCount();
                for (u32 j = 0; j < groupItemCount; j++)
                {
                    internal::GroupItemLocationInfo locationInfo;
                    if (reader.ReadGroupItemLocationInfo(&locationInfo, j))
                    {
                        if (locationInfo.fileId == fileId && locationInfo.address)
                        {
                            if (counter == variationId)
                            {
                                if (outFileSize)
                                {
                                    u32 fileSize = locationInfo.fileSize;
                                    SEAD_ASSERT(fileSize != 0xFFFFFFFF);

                                    *outFileSize = fileSize;
                                }

                                if (outGroupFileId)
                                {
                                    *outGroupFileId = info->fileId;
                                }

                                if (outGroupId)
                                {
                                    *outGroupId = groupId;
                                }

                                return locationInfo.address;
                            }

                            counter++;
                        }
                    }
                }
            }
        }
    }

    if (outFileSize)
    {
        *outFileSize = INVALID_ID;
    }

    if (outGroupFileId)
    {
        *outGroupFileId = INVALID_ID;
    }

    if (outGroupId)
    {
        *outGroupId = INVALID_ID;
    }

    return nullptr;
}

const void* MemorySoundArchive::detail_GetFileAddressGroup(FileId fileId, u32 groupId) const
{
    const internal::SoundArchiveFile::GroupInfo* info = GetGroupInfo(groupId);
    if (info)
    {
        const void* groupFile = nullptr;

        if (info->fileId >= detail_GetFileCount())
        {
            u32 externalFileId = info->fileId - detail_GetFileCount();
            if (externalFileId < mExternalGroups.size())
            {
                groupFile = mExternalGroups[externalFileId];
                SEAD_ASSERT(groupFile);

                if (fileId == info->fileId)
                {
                    return groupFile;
                }
            }
        }
        else
        {
            const internal::SoundArchiveFile::FileInfo* groupFileInfo = detail_GetFileInfo(info->fileId);
            if (groupFileInfo && groupFileInfo->GetFileLocationType() != nw::snd::internal::SoundArchiveFile::FILE_LOCATION_TYPE_NONE)
            {
                const internal::SoundArchiveFile::InternalFileInfo* groupInternalFileInfo = groupFileInfo->GetInternalFileInfo();
                SEAD_ASSERT(groupInternalFileInfo);

                u32 offsetFromFileBlockHead = groupInternalFileInfo->GetOffsetFromFileBlockHead();
                SEAD_ASSERT(offsetFromFileBlockHead != 0xFFFFFFFF);

                groupFile = sead::PtrUtil::addOffset(mData, offsetFromFileBlockHead + mHeader.GetFileBlockOffset());
            }
        }

        if (groupFile)
        {
            internal::GroupFileReader reader(groupFile);
            u32 groupItemCount = reader.GetGroupItemCount();
            for (u32 j = 0; j < groupItemCount; j++)
            {
                internal::GroupItemLocationInfo locationInfo;
                if (reader.ReadGroupItemLocationInfo(&locationInfo, j))
                {
                    if (locationInfo.fileId == fileId)
                    {
                        return locationInfo.address;
                    }
                }
            }
        }
    }

    return nullptr;
}

const void* MemorySoundArchive::detail_GetFileAddressSimple(FileId fileId) const
{
    const internal::SoundArchiveFile::FileInfo* fileInfo = detail_GetFileInfo(fileId);

    if (fileInfo)
    {
        const internal::SoundArchiveFile::InternalFileInfo* internalFileInfo = fileInfo->GetInternalFileInfo();
        if (internalFileInfo)
        {
            u32 offsetFromFileBlockHead = internalFileInfo->GetOffsetFromFileBlockHead();
            if (offsetFromFileBlockHead != 0xFFFFFFFF)
            {
                return sead::PtrUtil::addOffset(mData, offsetFromFileBlockHead + mHeader.GetFileBlockOffset());
            }
        }
    }

    return nullptr;
}

} } // namespace nw::snd
