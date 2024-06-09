#include <snd/snd_MemorySoundArchive.h>

#include <snd/snd_GroupFileReader.h>

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

bool MemorySoundArchive::Initialize(const void* soundArchiveData)
{
    SEAD_ASSERT(soundArchiveData);

    const internal::SoundArchiveFile::FileHeader& header = *reinterpret_cast<const internal::SoundArchiveFile::FileHeader*>(soundArchiveData);

    //if (sead::MemUtil::compare(header.signature, "CSAR", 4) != 0)
    if (sead::MemUtil::compare(header.signature, "FSAR", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "not a BFSAR file");
        return false;
    }

    {
        const void* byteOrder = sead::PtrUtil::addOffset(&header, offsetof(ut::BinaryFileHeader, byteOrder));
        sFileEndian = sead::Endian::markToEndian(*(u16*)byteOrder);
    }

    if (false)
    //if (!(0x00010000 <= header.version && header.version <= 0x00020000))
    {
        SEAD_ASSERT_MSG(false, "BFSAR version not supported (0x%08X)", (u32)header.version);
        return false;
    }

    mHeader = header; // Copy

    const void* infoBlock = sead::PtrUtil::addOffset(soundArchiveData, mHeader.GetInfoBlockOffset());
    mInfoBlockBody = &reinterpret_cast<const internal::SoundArchiveFile::InfoBlock*>(infoBlock)->body;

    if (mHeader.GetStringBlockOffset() != 0xFFFFFFFF && mHeader.GetStringBlockSize() != 0xFFFFFFFF)
    {
        const void* stringBlock = sead::PtrUtil::addOffset(soundArchiveData, mHeader.GetStringBlockOffset());
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

const void* MemorySoundArchive::detail_GetFileAddress(FileId fileId, u32* outFileSize, u32 variationId, u32* outGroupFileId) const
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

                    return sead::PtrUtil::addOffset(mData, offsetFromFileBlockHead + mHeader.GetFileBlockOffset());
                }

                counter++;
            }
        }
    }

    u32 groupCount = GetGroupCount();
    for (u32 i = 0; i < groupCount; i++)
    {
        const internal::SoundArchiveFile::GroupInfo* info = GetGroupInfo(GetGroupIdFromIndex(i));
        if (info)
        {
            const internal::SoundArchiveFile::FileInfo* groupFileInfo = detail_GetFileInfo(info->fileId);
            if (groupFileInfo && groupFileInfo->GetFileLocationType() != nw::snd::internal::SoundArchiveFile::FILE_LOCATION_TYPE_NONE)
            {
                const internal::SoundArchiveFile::InternalFileInfo* groupInternalFileInfo = groupFileInfo->GetInternalFileInfo();
                SEAD_ASSERT(groupInternalFileInfo);

                u32 offsetFromFileBlockHead = groupInternalFileInfo->GetOffsetFromFileBlockHead();
                SEAD_ASSERT(offsetFromFileBlockHead != 0xFFFFFFFF);

                const void* groupFile = sead::PtrUtil::addOffset(mData, offsetFromFileBlockHead + mHeader.GetFileBlockOffset());

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

    return nullptr;
}

} } // namespace nw::snd
