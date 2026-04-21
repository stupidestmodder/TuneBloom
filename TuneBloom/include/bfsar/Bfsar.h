#pragma once

#include <bfsar/Bank.h>
#include <bfsar/BankFile.h>
#include <bfsar/Group.h>
#include <bfsar/Player.h>
#include <bfsar/SequenceFile.h>
#include <bfsar/Sound.h>
#include <bfsar/SoundSet.h>
#include <bfsar/SoundDataMgr.h>
#include <bfsar/WaveArchive.h>
#include <bfsar/WaveFile.h>

#include <filedevice/seadFileDevice.h>
#include <prim/seadSafeString.h>

#include <snd/snd_MemorySoundArchive.h>

struct SoundArchivePlayerInfo
{
    SoundArchivePlayerInfo()
    {
        sead::MemUtil::fillZero(this, sizeof(SoundArchivePlayerInfo));
    }

    u16 sequenceSoundMax;
    u16 sequenceTrackMax;
    u16 streamSoundMax;
    u16 streamTrackMax;
    u16 streamChannelMax;
    u16 waveSoundMax;
    u16 waveTrackMax;
    u8 streamBufferTimes;
    u32 options;
};

class Bfsar
{
public:
    Bfsar();
    ~Bfsar();

    void create();
    bool open(u8* bfsarFile, const sead::SafeString& filePath, sead::Heap* heap);
    bool save();
    bool saveAs(const sead::SafeString& filePath);
    void close();

    bool isOpen() const
    {
        return mOpen;
    }

    const sead::SafeString& getFilePath() const
    {
        if (mFilePath)
            return *mFilePath;

        return sead::SafeString::cEmptyString;
    }

    sead::Endian::Types getEndian() const
    {
        return mEndian;
    }

    void setEndian(sead::Endian::Types endian)
    {
        mEndian = endian;
    }

    u32 getVersion() const
    {
        return mVersion;
    }

    void setVersion(u32 version)
    {
        mVersion = version;
    }

    bool isStreamTrackInfoAvailable() const
    {
        return mVersion >= 0x00020000; //! Assumption
    }

    bool isStreamSendAvailable() const
    {
        return mVersion >= 0x00020100;
    }

    bool isFilterSupportedVersion() const
    {
        return mVersion >= 0x00020100;
    }

    bool isStreamPrefetchAvailable() const
    {
        return mVersion >= 0x00020200;
    }

    bool isIncludeStringTable() const
    {
        return mIncludeStringTable;
    }

    void setIncludeStringTable(bool includeStringTable)
    {
        mIncludeStringTable = includeStringTable;
    }

    const SoundArchivePlayerInfo& getSoundArchivePlayerInfo() const
    {
        return mSoundArchivePlayerInfo;
    }

    SoundArchivePlayerInfo& getSoundArchivePlayerInfo()
    {
        return mSoundArchivePlayerInfo;
    }

    Sound::List& getSoundList()
    {
        return mSoundList;
    }

    SoundSet::List& getSoundSetList()
    {
        return mSoundSetList;
    }

    Bank::List& getBankList()
    {
        return mBankList;
    }

    WaveArchive::List& getWaveArchiveList()
    {
        return mWaveArchiveList;
    }

    Group::List& getGroupList()
    {
        return mGroupList;
    }

    Player::List& getPlayerList()
    {
        return mPlayerList;
    }

    WaveFile::List& getWaveFileList()
    {
        return mWaveFileList;
    }

    SequenceFile::List& getSequenceFileList()
    {
        return mSequenceFileList;
    }

    BankFile::List& getBankFileList()
    {
        return mBankFileList;
    }

    const Item* getItem(u32 id, const Item::List& list) const
    {
        if (id != Item::cInvalidId)
        {
            id = nw::snd::internal::Util::GetItemIndex(id);

            if (id < list.size())
            {
                return list.nth(id)->val();
            }
        }

        return nullptr;
    }

    Item* getItem(u32 id, const Item::List& list)
    {
        if (id != Item::cInvalidId)
        {
            id = nw::snd::internal::Util::GetItemIndex(id);

            if (id < list.size())
            {
                return list.nth(id)->val();
            }
        }

        return nullptr;
    }

    const Item::List& getItemList(Item::ItemType itemType)
    {
        switch (itemType)
        {
            case Item::ItemType::Sound:
                return getSoundList();

            case Item::ItemType::SoundSet:
                return getSoundSetList();

            case Item::ItemType::Bank:
                return getBankList();

            case Item::ItemType::WaveArchive:
                return getWaveArchiveList();
        }

        static const Item::List cNullList;
        return cNullList;
    }

    //? Check if name constains only allowed characters
    bool validName(const sead::SafeString& name) const;
    //? Check if name is duplicated
    bool validateName(const sead::SafeString& name) const;
    //? Check if name is duplicated (Excluding item)
    bool validateName(const Item& item) const;

    void updateList(Item::List& list);

    u32 getVersionForBfwsd() const
    {
        if (mVersion >= 0x00020100)
            return 0x00010100;

        return 0x00010000;
    }

    u32 getVersionForBfbnk() const
    {
        return 0x00010000;
    }

    u32 getVersionForBfwar() const
    {
        return 0x00010000;
    }

    u32 getVersionForBfgrp() const
    {
        return 0x00010000;
    }

    u32 getVersionForBfseq() const
    {
        if (mVersion >= 0x00020100)
            return 0x00020000;

        return 0x00010000;
    }

    u32 getVersionForBfwav() const
    {
        if (mVersion >= 0x00020200)
            return 0x00010200;
        else if (mVersion >= 0x00020000)
            return 0x00010100;

        return 0x00010000;
    }

    u32 getVersionForBfstm() const
    {
        if (mVersion >= 0x00020200)
            return 0x00040000;
        else if (mVersion >= 0x00020000)
            return 0x00030000;

        return 0x00010000;
    }

    //? Validate every item for saving
    bool validate_();

private:
    bool open_(const nw::snd::MemorySoundArchive& soundArchive, sead::Heap* heap);
    void save_(sead::FileHandle& handle);
    void close_();

    bool validateName_(const sead::SafeString& name, const Item::List& list, const Item* ignoreItem = nullptr) const;

private:
    bool mOpen;
    sead::HeapSafeString* mFilePath;

    sead::Endian::Types mEndian;
    u32 mVersion;
    bool mIncludeStringTable;
    SoundArchivePlayerInfo mSoundArchivePlayerInfo;

    Sound::List mSoundList;

    SoundSet::List mSoundSetList;

    Bank::List mBankList;

    WaveArchive::List mWaveArchiveList;

    Group::List mGroupList;

    Player::List mPlayerList;

    WaveFile::List mWaveFileList;

    SequenceFile::List mSequenceFileList;

    BankFile::List mBankFileList;
};
