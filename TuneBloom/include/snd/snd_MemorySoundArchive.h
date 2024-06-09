#pragma once

#include <snd/snd_SoundArchive.h>
#include <snd/snd_SoundArchiveFile.h>

namespace nw { namespace snd {

class MemorySoundArchive : public SoundArchive
{
public:
    MemorySoundArchive();

    bool Initialize(const void* soundArchiveData);

    u32 GetStringCount() const
    {
        SEAD_ASSERT(mStringBlockBody);
        return mStringBlockBody->GetStringCount();
    }

    u32 GetSoundCount() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetSoundCount();
    }

    u32 GetGroupCount() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetGroupCount();
    }

    u32 GetPlayerCount() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetPlayerCount();
    }

    u32 GetSoundGroupCount() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetSoundGroupCount();
    }

    u32 GetBankCount() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetBankCount();
    }

    u32 GetWaveArchiveCount() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetWaveArchiveCount();
    }

    u32 detail_GetFileCount() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetFileCount();
    }

    const char* GetItemLabel(ItemId id) const
    {
        SEAD_ASSERT(mInfoBlockBody);

        if (!mStringBlockBody)
            return nullptr;

        StringId stringId = mInfoBlockBody->GetItemStringId(id);
        if (stringId == INVALID_ID)
            return nullptr;

        return mStringBlockBody->GetString(stringId);
    }

    ItemId GetItemId(const char* label) const
    {
        if (!mStringBlockBody)
            return INVALID_ID;

        return mStringBlockBody->GetItemId(label);
    }

    FileId GetItemFileId(ItemId id) const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetItemFileId(id);
    }

    u32 GetSoundUserParam(ItemId soundId) const
    {
        SEAD_ASSERT(mInfoBlockBody);

        const internal::SoundArchiveFile::SoundInfo* soundInfo = mInfoBlockBody->GetSoundInfo(soundId);
        if (!soundInfo)
            return 0;

        return soundInfo->GetUserParam();
    }

    bool ReadSoundUserParam(ItemId soundId, s32 index, u32& value) const
    {
        SEAD_ASSERT(0 <= index <= USER_PARAM_INDEX_MAX);
        SEAD_ASSERT(mInfoBlockBody);

        const internal::SoundArchiveFile::SoundInfo* soundInfo = mInfoBlockBody->GetSoundInfo(soundId);
        if (!soundInfo)
            return false;

        return soundInfo->ReadUserParam(index, value);
    }

    const char* GetString(ItemId stringId) const
    {
        SEAD_ASSERT(mStringBlockBody);
        return mStringBlockBody->GetString(stringId);
    }

    SoundType GetSoundType(ItemId soundId) const
    {
        SEAD_ASSERT(mInfoBlockBody);

        const internal::SoundArchiveFile::SoundInfo* soundInfo = mInfoBlockBody->GetSoundInfo(soundId);
        if (!soundInfo)
            return SOUND_TYPE_INVALID;

        return soundInfo->GetSoundType();
    }

    const nw::snd::internal::SoundArchiveFile::SoundInfo* GetSoundInfo(ItemId soundId) const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetSoundInfo(soundId);
    }

    const nw::snd::internal::SoundArchiveFile::BankInfo* GetBankInfo(ItemId bankId) const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetBankInfo(bankId);
    }

    const nw::snd::internal::SoundArchiveFile::PlayerInfo* GetPlayerInfo(ItemId playerId) const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetPlayerInfo(playerId);
    }

    const nw::snd::internal::SoundArchiveFile::SoundArchivePlayerInfo* GetSoundArchivePlayerInfo() const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetSoundArchivePlayerInfo();
    }

    const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* GetWaveArchiveInfo(ItemId warcId) const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetWaveArchiveInfo(warcId);
    }

    const nw::snd::internal::SoundArchiveFile::SoundGroupInfo* detail_GetSoundGroupInfo(ItemId soundGroupId) const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetSoundGroupInfo(soundGroupId);
    }

    const nw::snd::internal::SoundArchiveFile::GroupInfo* GetGroupInfo(ItemId groupId) const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetGroupInfo(groupId);
    }

    const nw::snd::internal::SoundArchiveFile::FileInfo* detail_GetFileInfo(FileId fileId) const
    {
        SEAD_ASSERT(mInfoBlockBody);
        return mInfoBlockBody->GetFileInfo(fileId);
    }

    const internal::Util::Table<ut::ResU32>* detail_GetWaveArchiveIdTable(ItemId id) const;

    const void* detail_GetFileAddress(FileId fileId, u32* outFileSize = nullptr, u32 variationId = 0, u32* outGroupFileId = nullptr) const;

    internal::SoundArchiveFile::FileHeader mHeader;
    const internal::SoundArchiveFile::StringBlockBody* mStringBlockBody;
    const internal::SoundArchiveFile::InfoBlockBody* mInfoBlockBody;
    const internal::SoundArchiveFile::FileBlockBody* mFileBlockBody;

    const void* mData;
};

} } // namespace nw::snd
