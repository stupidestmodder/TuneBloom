#include <bfsar/Bfsar.h>

#include <bfsar/writer/FileWriter.h>
#include <bfsar/writer/FlagParameters.h>
#include <bfsar/writer/PatriciaTree.h>
#include <bfsar/BfgrpFile.h>
#include <bfsar/BfwarFile.h>
#include <bfsar/BfwsdFile.h>
#include <bfsar/File.h>

#include <ui/PopupMgr.h>
#include <ui/UI.h>

#include <filedevice/seadFileDeviceMgr.h>
#include <filedevice/seadPath.h>
#include <stream/seadFileDeviceStream.h>

#include <snd/snd_BankFileReader.h>
#include <snd/snd_GroupFileReader.h>
#include <snd/snd_StreamSoundFileReader.h>
#include <snd/snd_WaveArchiveFileReader.h>
#include <snd/snd_WaveSoundFileReader.h>

#include <snd/StreamSoundPlayer.h> //? For cStrmTrackNum

#include <md5/md5.h>

#include <VectorMap.h>
#include <VectorSet.h>

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>

Bfsar::Bfsar()
    : mOpen(false)
    , mFilePath(nullptr)

    , mEndian(sead::Endian::eBig)
    , mVersion(0x00010000)
    , mIncludeStringTable(true)
    , mSoundArchivePlayerInfo()

    , mSoundList()

    , mSoundSetList()

    , mBankList()

    , mWaveArchiveList()

    , mGroupList()

    , mPlayerList()

    , mWaveFileList()

    , mSequenceFileList()

    , mBankFileList()
{
}

Bfsar::~Bfsar()
{
    close();
}

void Bfsar::create()
{
    close();

    // TODO: Ask user for those defaults
    mEndian = sead::Endian::eBig;
    mVersion = 0x00010000;
    mIncludeStringTable = true;
    mSoundArchivePlayerInfo.sequenceSoundMax = 64;
    mSoundArchivePlayerInfo.sequenceTrackMax = 64;
    mSoundArchivePlayerInfo.streamSoundMax = 4;
    mSoundArchivePlayerInfo.streamTrackMax = 4;
    mSoundArchivePlayerInfo.streamChannelMax = 8;
    mSoundArchivePlayerInfo.waveSoundMax = 64;
    mSoundArchivePlayerInfo.waveTrackMax = 64;
    mSoundArchivePlayerInfo.streamBufferTimes = 1;
    mSoundArchivePlayerInfo.options = 0;

    mOpen = true;
}

bool Bfsar::open(u8* bfsarFile, const sead::SafeString& filePath, sead::Heap* heap)
{
    close();

    mFilePath = new(heap) sead::HeapSafeString(heap, filePath);

    bool success;
    {
        nw::snd::MemorySoundArchive soundArchive;
        if (!soundArchive.Initialize(bfsarFile))
        {
            return false;
        }

        success = open_(soundArchive, heap);
    }

    delete bfsarFile;

    mOpen = true;

    return success;
}

bool Bfsar::save()
{
    if (!mOpen)
        return false;

    if (!mFilePath)
    {
        return SaveFileAs();
    }

    if (!validate_())
        return false;

    sead::FormatFixedSafeString<512> path("%s.save.bfsar", mFilePath->cstr()); // TODO: Remove
    // sead::SafeString path = *mFilePath;

    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
    SEAD_ASSERT(device);

    sead::FileHandle handle;
    device->tryOpen(&handle, path, sead::FileDevice::FileOpenFlag::eWriteOnly, 0);

    if (!handle.getDevice())
    {
        device->tryOpen(&handle, path, sead::FileDevice::FileOpenFlag::eCreate, 0);

        if (!handle.getDevice())
        {
            PopupMgr::instance()->addPopup({ "Coundn't open output file" });
            return false;
        }
    }

    save_(handle);

    return true;
}

bool Bfsar::saveAs(const sead::SafeString& filePath)
{
    if (!mOpen)
        return false;

    //? Should already be called even before dialog appears
    // if (!validate_())
    //     return false;

    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
    SEAD_ASSERT(device);

    sead::FileHandle handle;
    device->tryOpen(&handle, filePath, sead::FileDevice::FileOpenFlag::eWriteOnly, 0);

    if (!handle.getDevice())
    {
        device->tryOpen(&handle, filePath, sead::FileDevice::FileOpenFlag::eCreate, 0);

        if (!handle.getDevice())
        {
            PopupMgr::instance()->addPopup({ "Coundn't open output file" });
            return false;
        }
    }

    delete mFilePath;
    mFilePath = new sead::HeapSafeString(nullptr, filePath);

    save_(handle);

    return true;
}

void Bfsar::close()
{
    if (!mOpen)
        return;

    close_();

    if (mFilePath)
    {
        delete mFilePath;
        mFilePath = nullptr;
    }

    mOpen = false;
}

bool Bfsar::validName(const sead::SafeString& name) const
{
    if (name.isEmpty())
    {
        return false;
    }

    if (!name.startsWith("_") && !::isalpha(name[0]))
    {
        return false;
    }

    auto isValidNameChar = [](int c) -> int
    {
        return ::isalnum(c) || c == '_';
    };

    std::string n = name.cstr();

    if (!std::all_of(n.begin(), n.end(), isValidNameChar))
    {
        return false;
    }

    return true;
}

bool Bfsar::validateName(const sead::SafeString& name) const
{
    if (name.isEmpty())
        return false;

    if (!validateName_(name, mSoundList))
        return false;

    if (!validateName_(name, mSoundSetList))
        return false;

    if (!validateName_(name, mBankList))
        return false;

    if (!validateName_(name, mWaveArchiveList))
        return false;

    if (!validateName_(name, mGroupList))
        return false;

    if (!validateName_(name, mPlayerList))
        return false;

    return true;
}

bool Bfsar::validateName(const Item& item) const
{
    sead::SafeString name = item.getName();

    if (name.isEmpty())
        return false;

    if (!validateName_(name, mSoundList, &item))
        return false;

    if (!validateName_(name, mSoundSetList, &item))
        return false;

    if (!validateName_(name, mBankList, &item))
        return false;

    if (!validateName_(name, mWaveArchiveList, &item))
        return false;

    if (!validateName_(name, mGroupList, &item))
        return false;

    if (!validateName_(name, mPlayerList, &item))
        return false;

    return true;
}

void Bfsar::updateList(Item::List& list)
{
    u32 i = 0;
    for (Item* item : list)
    {
        item->setId(i);

        i++;
    }
}

bool Bfsar::open_(const nw::snd::MemorySoundArchive& soundArchive, sead::Heap* heap)
{
    mEndian = nw::ut::GetFileEndian(soundArchive.mHeader);

    mVersion = soundArchive.mHeader.version;

    mIncludeStringTable = soundArchive.mStringBlockBody != nullptr;

    {
        const nw::snd::internal::SoundArchiveFile::SoundArchivePlayerInfo* playerInfo = soundArchive.GetSoundArchivePlayerInfo();

        mSoundArchivePlayerInfo.sequenceSoundMax = playerInfo->sequenceSoundMax;
        mSoundArchivePlayerInfo.sequenceTrackMax = playerInfo->sequenceTrackMax;
        mSoundArchivePlayerInfo.streamSoundMax = playerInfo->streamSoundMax;
        mSoundArchivePlayerInfo.streamTrackMax = playerInfo->streamTrackMax;
        mSoundArchivePlayerInfo.streamChannelMax = playerInfo->streamChannelMax;
        mSoundArchivePlayerInfo.waveSoundMax = playerInfo->waveSoundMax;
        mSoundArchivePlayerInfo.waveTrackMax = playerInfo->waveTrackMax;
        mSoundArchivePlayerInfo.streamBufferTimes = isStreamPrefetchAvailable() ? playerInfo->streamBufferTimes : 1;
        mSoundArchivePlayerInfo.options = playerInfo->options;
    }

    //? Load Groups first in case there are external ones
    for (u32 i = 0; i < soundArchive.GetGroupCount(); i++)
    {
        u32 groupId = soundArchive.GetGroupIdFromIndex(i);
        nw::snd::internal::SoundArchiveFile::GroupInfo* groupInfo = const_cast<nw::snd::internal::SoundArchiveFile::GroupInfo*>(soundArchive.GetGroupInfo(groupId));
        if (!groupInfo)
        {
            PopupMgr::instance()->setCorruptInfo("Invalid GroupInfo");
            return false;
        }

        Group* group = new(heap) Group();
        group->mId = i;
        PopupMgr::instance()->setCurrentProcessItem(group);

        group->mEnableName = groupInfo->optionParameter.GetTrueCount(nw::snd::internal::GROUP_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && groupInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            group->mName = soundArchive.GetString(groupInfo->GetStringId());
        }
        else
        {
            group->mName.format("GROUP_%u", i);
        }

        if (groupInfo->fileId == nw::snd::SoundArchive::INVALID_ID)
        {
            //? Group is external
            group->mOutputType = Group::OutputType::External;

            const void* bfgrpFile = nullptr;
            {
                sead::FixedSafeString<512> filePath;

                const u32 filterCount = 1;
                FileFilter filters[filterCount] = {
                    { "Group File (*.bfgrp)", "*.bfgrp" }
                };

                if (OpenFileDialog(&filePath, sead::FormatFixedSafeString<512>("Open group file for '%s'", group->getFormattedName().cstr()).cstr(), filterCount, filters))
                {
                    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
                    SEAD_ASSERT(device);

                    sead::FileDevice::LoadArg arg;
                    arg.path = filePath;

                    bfgrpFile = device->tryLoad(arg);
                    if (!bfgrpFile)
                    {
                        sead::FixedSafeString<512> file;
                        if (sead::Path::getFileName(&file, filePath))
                        {
                            sead::FormatFixedSafeString<1024> msg("Couldn't load the file '%s'", file.cstr());
                            PopupMgr::instance()->pushCurrentItemError(msg);
                        }
                        else
                        {
                            sead::FormatFixedSafeString<256> msg("Couldn't load file for group '%s'", group->getFormattedName().cstr());
                            PopupMgr::instance()->pushCurrentItemError(msg);
                        }
                    }
                }
                else
                {
                    PopupMgr::instance()->pushCurrentItemError("External group file wasn't loaded");
                }
            }

            if (bfgrpFile)
            {
                //? Patch file id
                groupInfo->fileId = soundArchive.detail_GetFileCount() + static_cast<u32>(soundArchive.mExternalGroups.size());

                soundArchive.mExternalGroups.push_back(bfgrpFile);
            }
        }

        nw::snd::internal::GroupFileReader reader(soundArchive.detail_GetFileAddress(groupInfo->fileId));
        if (!reader.IsInitialized())
        {
            groupInfo->fileId = nw::snd::SoundArchive::INVALID_ID; //? Mark as invalid so it won't be used later
        }

        mGroupList.pushBack(group);
    }

/*
    for (Item* item : mFileList)
    {
        File* file = static_cast<File*>(item);

        if (file->getLocationType() != File::LocationType::Internal)
        {
            continue;
        }

        const nw::snd::internal::SoundArchiveFile::FileInfo* fileInfo = soundArchive.detail_GetFileInfo(file->getId());
        const nw::snd::internal::SoundArchiveFile::InternalFileInfo* internalFileInfo = fileInfo->GetInternalFileInfo();

        if (internalFileInfo->GetOffsetFromFileBlockHead() == nw::snd::internal::SoundArchiveFile::InternalFileInfo::INVALID_OFFSET)
        {
            file->mLocationType = File::LocationType::Group;
        }

        struct TempFile
        {
            const void* fileAddr;
            u32 fileSize;
            u32 groupFileId;
        };

        std::vector<TempFile> variations;

        for (u32 variationId = 0; ; variationId++)
        {
            u32 groupFileId = 0xFFFFFFFF;
            u32 fileSize = 0xFFFFFFFF;
            const void* fileAddr = soundArchive.detail_GetFileAddress(file->getId(), &fileSize, variationId, &groupFileId);
            if (!fileAddr)
                break;

            variations.emplace_back(fileAddr, fileSize, groupFileId);
        }

        size_t varCount = variations.size();
        if (varCount == 0)
        {
            continue;
        }
        else if (varCount > 1)
        {
            if (variations[0].groupFileId == 0xFFFFFFFF)
            {
                const void* refAddr = variations[0].fileAddr;
                for (u32 variationId = 1; variationId < varCount; variationId++)
                {
                    if (variations[variationId].fileAddr == refAddr)
                    {
                        variations.erase(variations.begin());
                        break;
                    }
                }
            }

            const void* refAddr = variations[0].fileAddr;
            u32 refSize = variations[0].fileSize;

            if (sead::MemUtil::compare(refAddr, "FSEQ", 4) == 0 || sead::MemUtil::compare(refAddr, "FWAR", 4) == 0)
            {
                for (u32 variationId = 1; variationId < varCount; variationId++)
                {
                    if (refSize != variations[variationId].fileSize || sead::MemUtil::compare(refAddr, variations[variationId].fileAddr, refSize) != 0)
                    {
                        SEAD_ASSERT(false);
                    }
                }
            }
            else if (sead::MemUtil::compare(refAddr, "FBNK", 4) == 0)
            {
                nw::snd::internal::BankFileReader readerRef(refAddr);

                const nw::snd::internal::Util::WaveIdTable& refTable = readerRef.mInfoBlockBody->GetWaveIdTable();
                const void* refWS = &refTable.table.item[0];
                const void* refWE = &refTable.table.item[refTable.table.count];

                u32 startSize = sead::PtrUtil::diff(refWS, refAddr);

                u32 endOffset = sead::PtrUtil::diff(refWE, refAddr);
                u32 endSize = refSize - endOffset;

                for (u32 variationId = 1; variationId < varCount; variationId++)
                {
                    if (refSize != variations[variationId].fileSize ||
                        sead::MemUtil::compare(refAddr, variations[variationId].fileAddr, startSize) != 0 ||
                        sead::MemUtil::compare(sead::PtrUtil::addOffset(refAddr, endOffset), sead::PtrUtil::addOffset(variations[variationId].fileAddr, endOffset), endSize) != 0)
                    {
                        SEAD_ASSERT(false);
                    }
                }
            }
            else if (sead::MemUtil::compare(refAddr, "FWSD", 4) == 0)
            {
                nw::snd::internal::WaveSoundFileReader readerRef(refAddr);

                const nw::snd::internal::Util::WaveIdTable& refTable = readerRef.mInfoBlockBody->GetWaveIdTable();
                const void* refWS = &refTable.table.item[0];
                const void* refWE = &refTable.table.item[refTable.table.count];

                u32 startSize = sead::PtrUtil::diff(refWS, refAddr);

                u32 endOffset = sead::PtrUtil::diff(refWE, refAddr);
                u32 endSize = refSize - endOffset;

                for (u32 variationId = 1; variationId < varCount; variationId++)
                {
                    if (refSize != variations[variationId].fileSize ||
                        sead::MemUtil::compare(refAddr, variations[variationId].fileAddr, startSize) != 0 ||
                        sead::MemUtil::compare(sead::PtrUtil::addOffset(refAddr, endOffset), sead::PtrUtil::addOffset(variations[variationId].fileAddr, endOffset), endSize) != 0)
                    {
                        SEAD_ASSERT(false);
                    }
                }
            }
        }

        file->read(variations[0].fileAddr, variations[0].fileSize);

        if (sead::MemUtil::compare(variations[0].fileAddr, "FBNK", 4) == 0)
        {
            for (const auto& variation : variations)
            {
                nw::snd::internal::BankFileReader reader(variation.fileAddr);
                std::vector<File::WaveId> waveIdTable;
                const nw::snd::internal::Util::Table<nw::snd::internal::Util::WaveId>& table = reader.mInfoBlockBody->GetWaveIdTable().table;
                for (u32 i = 0; i < table.count; i++)
                {
                    const nw::snd::internal::Util::WaveId& waveId = table.item[i];
                    waveIdTable.emplace_back(waveId.waveArchiveId, waveId.waveIndex);
                }
                file->pushBackWaveIdTableVariation(variation.groupFileId, std::move(waveIdTable));
            }
        }
        else if (sead::MemUtil::compare(variations[0].fileAddr, "FWSD", 4) == 0)
        {
            for (const auto& variation : variations)
            {
                nw::snd::internal::WaveSoundFileReader reader(variation.fileAddr);
                std::vector<File::WaveId> waveIdTable;
                const nw::snd::internal::Util::Table<nw::snd::internal::Util::WaveId>& table = reader.mInfoBlockBody->GetWaveIdTable().table;
                for (u32 i = 0; i < table.count; i++)
                {
                    const nw::snd::internal::Util::WaveId& waveId = table.item[i];
                    waveIdTable.emplace_back(waveId.waveArchiveId, waveId.waveIndex);
                }
                file->pushBackWaveIdTableVariation(variation.groupFileId, std::move(waveIdTable));
            }
        }
    }
*/

    //? Load Players first so Sounds can reference them
    for (u32 i = 0; i < soundArchive.GetPlayerCount(); i++)
    {
        const nw::snd::internal::SoundArchiveFile::PlayerInfo* playerInfo = soundArchive.GetPlayerInfo(soundArchive.GetPlayerIdFromIndex(i));
        if (!playerInfo)
        {
            PopupMgr::instance()->setCorruptInfo("Invalid PlayerInfo");
            return false;
        }

        Player* player = new(heap) Player();
        player->mId = i;
        PopupMgr::instance()->setCurrentProcessItem(player);

        player->mEnableName = playerInfo->optionParameter.GetTrueCount(nw::snd::internal::PLAYER_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && playerInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            player->mName = soundArchive.GetString(playerInfo->GetStringId());
        }
        else
        {
            player->mName.format("PLAYER_%u", i);
        }

        player->mPlayableSoundMax = playerInfo->playableSoundMax;

        player->mEnablePlayerHeapSize = playerInfo->optionParameter.GetTrueCount(nw::snd::internal::PLAYER_INFO_HEAP_SIZE) != 0;
        if (player->mEnablePlayerHeapSize)
            player->mPlayerHeapSize = playerInfo->GetPlayerHeapSize();

        mPlayerList.pushBack(player);
    }

    struct TempFile
    {
        u32 id;
        const void* fileData;
        u32 fileDataSize;
    };

    std::unordered_map<u32, u32> seqFileIdxMap;

    for (u32 i = 0; i < soundArchive.detail_GetFileCount(); i++)
    {
        u32 seqFileSize = 0;
        const void* seqFile = soundArchive.detail_GetFileAddress(i, &seqFileSize);

        // if (!seqFile || sead::MemUtil::compare(seqFile, "CSEQ", 4) != 0)
        if (!seqFile || sead::MemUtil::compare(seqFile, "FSEQ", 4) != 0)
        {
            continue;
        }

        u32 globalId = mSequenceFileList.size();

        SequenceFile* sequence = new(heap) SequenceFile();
        sequence->mId = globalId;

        sequence->mEnableName = true;
        sequence->mName = "Sequence";

        PopupMgr::instance()->setCurrentProcessItem(sequence);
        if (!sequence->read(seqFile))
        {
        }

        mSequenceFileList.pushBack(sequence);

        seqFileIdxMap.try_emplace(i, globalId);
    }

    // {
    //     u32 bankFileId = soundArchive.GetBankInfo(soundArchive.GetBankIdFromIndex(29))->fileId;
    //     nw::snd::internal::BankFileReader reader(soundArchive.detail_GetFileAddressGroup(bankFileId, soundArchive.GetGroupIdFromIndex(5)));
    //     SEAD_ASSERT(reader.IsInitialized());

    //     const nw::snd::internal::Util::WaveIdTable& table = *reader.GetWaveIdTable();
    //     for (u32 i = 0; i < table.GetCount(); i++)
    //     {
    //         const nw::snd::internal::Util::WaveId* waveId = table.GetWaveId(i);
    //         SEAD_ASSERT(waveId);

    //         SEAD_PRINT("WaveId %u, (%u, %u)\n", i, (u32)waveId->waveArchiveId, (u32)waveId->waveIndex);
    //     }
    // }

    struct WaveTempFile
    {
        std::string hash;
        u32 id;
        const void* fileData;
        u32 fileDataSize;
    };

    const u32 warcNum = soundArchive.GetWaveArchiveCount();
    std::vector< std::vector<WaveTempFile> > warcFileCache(warcNum);
    std::vector< std::unordered_set<u32> > waveIdMapSet(warcNum);
    if (warcNum > 0)
    {
        // Process warc 0
        {
            const u32 waveArchiveId = soundArchive.GetWaveArchiveIdFromIndex(0);
            const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(waveArchiveId);
            if (!warcInfo)
            {
                PopupMgr::instance()->setCorruptInfo("Invalid WaveArchiveInfo");
                return false;
            }

            // TODO: Validate
            nw::snd::internal::WaveArchiveFileReader reader(soundArchive.detail_GetFileAddress(warcInfo->fileId));
            const u32 waveFileCount = reader.GetWaveFileCount();
            for (u32 j = 0; j < waveFileCount; j++)
            {
                const void* waveFile = reader.GetWaveFile(j);
                u32 waveFileSize = reader.GetWaveFileSize(j);

                std::string hash = md5(waveFile, waveFileSize);

                u32 globalId = mWaveFileList.size();

                WaveFile* wave = new(heap) WaveFile();
                wave->mId = globalId;

                wave->mEnableName = true;
                wave->mName = "Wave";

                PopupMgr::instance()->setCurrentProcessItem(wave);
                if (!wave->read(waveFile))
                {
                }

                mWaveFileList.pushBack(wave);
                warcFileCache[0].emplace_back(hash, globalId, waveFile, waveFileSize);
                waveIdMapSet[0].insert(globalId);
            }
            SEAD_ASSERT(warcFileCache[0].size() == waveFileCount);
            SEAD_ASSERT(waveIdMapSet[0].size() == waveFileCount);
        }

        for (u32 i = 1; i < warcNum; i++)
        {
            const u32 waveArchiveId = soundArchive.GetWaveArchiveIdFromIndex(i);
            const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(waveArchiveId);
            if (!warcInfo)
            {
                PopupMgr::instance()->setCorruptInfo("Invalid WaveArchiveInfo");
                return false;
            }

            // TODO: Validate
            nw::snd::internal::WaveArchiveFileReader reader(soundArchive.detail_GetFileAddress(warcInfo->fileId));
            const u32 waveFileCount = reader.GetWaveFileCount();
            for (u32 j = 0; j < waveFileCount; j++)
            {
                const void* waveFile = reader.GetWaveFile(j);
                u32 waveFileSize = reader.GetWaveFileSize(j);

                std::string hash = md5(waveFile, waveFileSize);

                bool found = false;
                for (u32 other_i = 0; other_i < i; other_i++)
                {
                    const u32 other_waveArchiveId = soundArchive.GetWaveArchiveIdFromIndex(other_i);
                    const std::vector<WaveTempFile>& fileCache = warcFileCache[other_i];
                    for (u32 other_j = 0, other_fileNum = fileCache.size(); other_j < other_fileNum; other_j++)
                    {
                        const WaveTempFile& ref = fileCache[other_j];
                        if (waveIdMapSet[i].contains(ref.id))
                            continue;

                        if (ref.hash != hash || ref.fileDataSize != waveFileSize || sead::MemUtil::compare(ref.fileData, waveFile, waveFileSize) != 0)
                            continue;
                        
                        warcFileCache[i].emplace_back(ref);
                        waveIdMapSet[i].insert(ref.id);
                        found = true;
                        break;
                    }
                    if (found)
                        break;
                }
                if (found)
                    continue;

                u32 globalId = mWaveFileList.size();
                
                WaveFile* wave = new(heap) WaveFile();
                wave->mId = globalId;

                wave->mEnableName = true;
                wave->mName = "Wave";

                PopupMgr::instance()->setCurrentProcessItem(wave);
                if (!wave->read(waveFile))
                {
                }

                mWaveFileList.pushBack(wave);
                warcFileCache[i].emplace_back(hash, globalId, waveFile, waveFileSize);
                waveIdMapSet[i].insert(globalId);
            }
            SEAD_ASSERT(warcFileCache[i].size() == waveFileCount);
            SEAD_ASSERT(waveIdMapSet[i].size() == waveFileCount);
        }

        for (u32 i = 0; i < warcNum; i++)
        {
            const u32 waveArchiveId = soundArchive.GetWaveArchiveIdFromIndex(i);
            const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(waveArchiveId);
            if (!warcInfo)
            {
                PopupMgr::instance()->setCorruptInfo("Invalid WaveArchiveInfo");
                return false;
            }

            if (warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_STRING_ID) == 0)
            {
                continue;
            }

            WaveArchive* warc = new(heap) WaveArchive();
            warc->mId = i;
            PopupMgr::instance()->setCurrentProcessItem(warc);

            warc->mEnableName = warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_STRING_ID) != 0;
            if (mIncludeStringTable && warcInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
            {
                warc->mName = soundArchive.GetString(warcInfo->GetStringId());
            }
            else
            {
                warc->mName.format("WARC_%u", i);
            }

            warc->mIsLoadIndividual = warcInfo->isLoadIndividual;
            if (warc->mIsLoadIndividual)
            {
                if (warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_WAVE_COUNT) == 0)
                {
                    PopupMgr::instance()->pushCurrentItemError("Uh");
                }
            }

            mWaveArchiveList.pushBack(warc);
        }
    }

    // {
    //     const std::vector<WaveTempFile>& warc5 = warcFileCache[5];
    //     for (u32 i = 0; i < warc5.size(); i++)
    //     {
    //         const WaveTempFile& ref = warc5[i];
    //         SEAD_PRINT("WARC 5 File %u -> Global ID %u\n", i, ref.id);
    //     }
    // }

    std::unordered_map<u64, u32> bankFileWarcId;

    for (u32 i = 0; i < soundArchive.GetBankCount(); i++)
    {
        const u32 bankId = soundArchive.GetBankIdFromIndex(i);
        const nw::snd::internal::SoundArchiveFile::BankInfo* bank = soundArchive.GetBankInfo(bankId);
        if (!bank)
        {
            PopupMgr::instance()->setCorruptInfo("Invalid BankInfo");
            return false;
        }

        const u32 fileId = bank->fileId;
        u32 groupId;
        const void* file = soundArchive.detail_GetFileAddress(fileId, nullptr, 0, nullptr, &groupId);
        if (!file)
            continue;

        std::vector<std::vector<u32>> globalWaveIdTable;
        std::vector<u32> warcIdxVec;
        std::unordered_map<const void*, u32> fileMap;
        u32 variationId;

        // Step 1: Gather information about the variations
        {
            // Process variation 0
            {
                // TODO: Validate
                nw::snd::internal::BankFileReader reader(file);
                SEAD_ASSERT(reader.IsInitialized());

                const nw::snd::internal::Util::WaveIdTable* waveIdTable = reader.GetWaveIdTable();
                SEAD_ASSERT(waveIdTable);

                nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable->table.item[0]);
                const u32 waveCount = waveIdTable->GetCount();
                std::vector<u32>& globalWaveIdVec = globalWaveIdTable.emplace_back(waveCount);

                for (u32 j = 0; j < waveCount; j++)
                {
                    if (j != 0)
                    {
                        SEAD_ASSERT(waveId[j].waveArchiveId == waveId[0].waveArchiveId);
                    }
                }

                u32 warcId = nw::snd::SoundArchive::INVALID_ID;
                u32 warcIdx = nw::snd::SoundArchive::INVALID_ID;
                if (waveCount > 0)
                {
                    warcId = waveId[0].waveArchiveId;
                    SEAD_ASSERT(nw::snd::internal::Util::GetItemType(warcId) == nw::snd::internal::ItemType_WaveArchive);
                    warcIdx = nw::snd::internal::Util::GetItemIndex(warcId);
                }
                bankFileWarcId[u64(fileId) << 32 | groupId] = warcId;
                fileMap[file] = warcId;
                warcIdxVec.push_back(warcIdx);

                for (u32 j = 0; j < waveCount; j++)
                {
                    u32 globalWaveId = warcFileCache[warcIdx][waveId[j].waveIndex].id;
                    globalWaveIdVec[j] = globalWaveId;
                }
            }

            variationId = 1;
            while (true)
            {
                const void* file = soundArchive.detail_GetFileAddress(fileId, nullptr, variationId, nullptr, &groupId);
                if (!file)
                    break;

                const auto& it = fileMap.find(file);
                if (it != fileMap.end())
                {
                    bankFileWarcId[u64(fileId) << 32 | groupId] = fileMap[file];
                    variationId++;
                    continue;
                }

                // TODO: Validate
                nw::snd::internal::BankFileReader reader(file);
                SEAD_ASSERT(reader.IsInitialized());

                const nw::snd::internal::Util::WaveIdTable* waveIdTable = reader.GetWaveIdTable();
                SEAD_ASSERT(waveIdTable);

                nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable->table.item[0]);
                const u32 waveCount = waveIdTable->GetCount();
                SEAD_ASSERT(waveCount == globalWaveIdTable[0].size());
                std::vector<u32>& globalWaveIdVec = globalWaveIdTable.emplace_back(waveCount);

                for (u32 j = 0; j < waveCount; j++)
                {
                    if (j != 0)
                    {
                        SEAD_ASSERT(waveId[j].waveArchiveId == waveId[0].waveArchiveId);
                    }
                }

                u32 warcId = nw::snd::SoundArchive::INVALID_ID;
                u32 warcIdx = nw::snd::SoundArchive::INVALID_ID;
                if (waveCount > 0)
                {
                    warcId = waveId[0].waveArchiveId;
                    SEAD_ASSERT(nw::snd::internal::Util::GetItemType(warcId) == nw::snd::internal::ItemType_WaveArchive);
                    warcIdx = nw::snd::internal::Util::GetItemIndex(warcId);
                }
                bankFileWarcId[u64(fileId) << 32 | groupId] = warcId;
                fileMap[file] = warcId;
                warcIdxVec.push_back(warcIdx);

                for (u32 j = 0; j < waveCount; j++)
                {
                    u32 globalWaveId = warcFileCache[warcIdx][waveId[j].waveIndex].id;
                    globalWaveIdVec[j] = globalWaveId;
                }

                variationId++;
            }
        }

        SEAD_ASSERT(globalWaveIdTable.size() == warcIdxVec.size());
        const u32 variationCount = globalWaveIdTable.size();

        const size_t waveCount = globalWaveIdTable[0].size();
        std::vector<u32> baseWaveId(waveCount);

        // Step 2: Pick the most appropriate global ID for each wave
        for (u32 j = 0; j < waveCount; j++)
        {
            bool found = false;
            for (u32 k = 0; k < variationCount; k++)
            {
                const u32 trialId = globalWaveIdTable[k][j];
                bool consistent = true;
                for (u32 l = 0; l < variationCount; l++)
                {
                    if (k == l)
                        continue;

                    u32 warcIdx = warcIdxVec[l];
                    SEAD_ASSERT(warcIdx != nw::snd::SoundArchive::INVALID_ID && warcIdx < waveIdMapSet.size());
                    if (waveIdMapSet[warcIdx].contains(trialId))
                    {
                        consistent = false;
                        break;
                    }
                }
                if (consistent)
                {
                    baseWaveId[j] = trialId;
                    found = true;
                    break;
                }
            }
            if (!found) // Welp
            {
              //SEAD_PRINT("Bank %u: Failed to find consistent global wave ID for wave entry %u\n", i, j);
                baseWaveId[j] = globalWaveIdTable[0][j];
            }
        }

        // Step 3: Patch all files to use the determined global ID for each wave
        {
            // Process variation 0
            {
                // TODO: Validate
                nw::snd::internal::BankFileReader reader(file);
                SEAD_ASSERT(reader.IsInitialized());

                const nw::snd::internal::Util::WaveIdTable* waveIdTable = reader.GetWaveIdTable();
                SEAD_ASSERT(waveIdTable);

                nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable->table.item[0]);
                for (u32 j = 0; j < waveIdTable->GetCount(); j++)
                {
                    waveId[j].waveArchiveId = 0;
                    waveId[j].waveIndex = baseWaveId[j];
                }
            }

            variationId = 1;
            while (true)
            {
                const void* file = soundArchive.detail_GetFileAddress(fileId, nullptr, variationId, nullptr, &groupId);
                if (!file)
                    break;

                const auto& it = fileMap.find(file);
                if (it != fileMap.end())
                {
                    variationId++;
                    continue;
                }

                // TODO: Validate
                nw::snd::internal::BankFileReader reader(file);
                SEAD_ASSERT(reader.IsInitialized());

                const nw::snd::internal::Util::WaveIdTable* waveIdTable = reader.GetWaveIdTable();
                SEAD_ASSERT(waveIdTable);

                nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable->table.item[0]);
                for (u32 j = 0; j < waveIdTable->GetCount(); j++)
                {
                    waveId[j].waveArchiveId = 0;
                    waveId[j].waveIndex = baseWaveId[j];
                }

                variationId++;
            }
        }
    }

    std::unordered_map<u64, u32> wsdFileWarcId;

    for (u32 i = 0; i < soundArchive.GetSoundGroupCount(); i++)
    {
        const u32 sndGroupId = soundArchive.GetSoundGroupIdFromIndex(i);
        const nw::snd::internal::SoundArchiveFile::SoundGroupInfo* soundSetInfo = soundArchive.detail_GetSoundGroupInfo(sndGroupId);
        if (!soundSetInfo)
        {
            PopupMgr::instance()->setCorruptInfo("Invalid SoundGroupInfo");
            return false;
        }

        const nw::snd::internal::SoundArchiveFile::WaveSoundGroupInfo* waveSoundSetInfo = soundSetInfo->GetWaveSoundGroupInfo();
        if (!waveSoundSetInfo)
            continue;

        SEAD_ASSERT(soundSetInfo->GetFileIdTable()->count == 1);
        const u32 fileId = soundSetInfo->GetFileIdTable()->item[0];
        u32 groupId;
        const void* file = soundArchive.detail_GetFileAddress(fileId, nullptr, 0, nullptr, &groupId);
        if (!file)
            continue;

        std::vector<std::vector<u32>> globalWaveIdTable;
        std::vector<u32> warcIdxVec;
        std::unordered_map<const void*, u32> fileMap;
        u32 variationId;

        // Step 1: Gather information about the variations
        {
            // Process variation 0
            {
                // TODO: Validate
                nw::snd::internal::WaveSoundFileReader reader(file);
                SEAD_ASSERT(reader.IsAvailable());

                const nw::snd::internal::Util::WaveIdTable* waveIdTable = &reader.mInfoBlockBody->GetWaveIdTable();
                nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable->table.item[0]);
                const u32 waveCount = waveIdTable->GetCount();
                std::vector<u32>& globalWaveIdVec = globalWaveIdTable.emplace_back(waveCount);

                for (u32 j = 0; j < waveCount; j++)
                {
                    if (j != 0)
                    {
                        SEAD_ASSERT(waveId[j].waveArchiveId == waveId[0].waveArchiveId);
                    }
                }

                u32 warcId = nw::snd::SoundArchive::INVALID_ID;
                u32 warcIdx = nw::snd::SoundArchive::INVALID_ID;
                if (waveCount > 0)
                {
                    warcId = waveId[0].waveArchiveId;
                    SEAD_ASSERT(nw::snd::internal::Util::GetItemType(warcId) == nw::snd::internal::ItemType_WaveArchive);
                    warcIdx = nw::snd::internal::Util::GetItemIndex(warcId);
                }
                wsdFileWarcId[u64(fileId) << 32 | groupId] = warcId;
                fileMap[file] = warcId;
                warcIdxVec.push_back(warcIdx);

                for (u32 j = 0; j < waveCount; j++)
                {
                    u32 globalWaveId = warcFileCache[warcIdx][waveId[j].waveIndex].id;
                    globalWaveIdVec[j] = globalWaveId;
                }
            }

            variationId = 1;
            while (true)
            {
                const void* file = soundArchive.detail_GetFileAddress(fileId, nullptr, variationId, nullptr, &groupId);
                if (!file)
                    break;

                const auto& it = fileMap.find(file);
                if (it != fileMap.end())
                {
                    wsdFileWarcId[u64(fileId) << 32 | groupId] = fileMap[file];
                    variationId++;
                    continue;
                }

                // TODO: Validate
                nw::snd::internal::WaveSoundFileReader reader(file);
                SEAD_ASSERT(reader.IsAvailable());

                const nw::snd::internal::Util::WaveIdTable* waveIdTable = &reader.mInfoBlockBody->GetWaveIdTable();
                nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable->table.item[0]);
                const u32 waveCount = waveIdTable->GetCount();
                SEAD_ASSERT(waveCount == globalWaveIdTable[0].size());
                std::vector<u32>& globalWaveIdVec = globalWaveIdTable.emplace_back(waveCount);

                for (u32 j = 0; j < waveCount; j++)
                {
                    if (j != 0)
                    {
                        SEAD_ASSERT(waveId[j].waveArchiveId == waveId[0].waveArchiveId);
                    }
                }

                u32 warcId = nw::snd::SoundArchive::INVALID_ID;
                u32 warcIdx = nw::snd::SoundArchive::INVALID_ID;
                if (waveCount > 0)
                {
                    warcId = waveId[0].waveArchiveId;
                    SEAD_ASSERT(nw::snd::internal::Util::GetItemType(warcId) == nw::snd::internal::ItemType_WaveArchive);
                    warcIdx = nw::snd::internal::Util::GetItemIndex(warcId);
                }
                wsdFileWarcId[u64(fileId) << 32 | groupId] = warcId;
                fileMap[file] = warcId;
                warcIdxVec.push_back(warcIdx);

                for (u32 j = 0; j < waveCount; j++)
                {
                    u32 globalWaveId = warcFileCache[warcIdx][waveId[j].waveIndex].id;
                    globalWaveIdVec[j] = globalWaveId;
                }

                variationId++;
            }
        }

        SEAD_ASSERT(globalWaveIdTable.size() == warcIdxVec.size());
        const u32 variationCount = globalWaveIdTable.size();

        const size_t waveCount = globalWaveIdTable[0].size();
        std::vector<u32> baseWaveId(waveCount);

        // Step 2: Pick the most appropriate global ID for each wave
        for (u32 j = 0; j < waveCount; j++)
        {
            bool found = false;
            for (u32 k = 0; k < variationCount; k++)
            {
                const u32 trialId = globalWaveIdTable[k][j];
                bool consistent = true;
                for (u32 l = 0; l < variationCount; l++)
                {
                    if (k == l)
                        continue;

                    u32 warcIdx = warcIdxVec[l];
                    SEAD_ASSERT(warcIdx != nw::snd::SoundArchive::INVALID_ID && warcIdx < waveIdMapSet.size());
                    if (waveIdMapSet[warcIdx].contains(trialId))
                    {
                        consistent = false;
                        break;
                    }
                }
                if (consistent)
                {
                    baseWaveId[j] = trialId;
                    found = true;
                    break;
                }
            }
            if (!found) // Welp
            {
              //SEAD_PRINT("Bank %u: Failed to find consistent global wave ID for wave entry %u\n", i, j);
                baseWaveId[j] = globalWaveIdTable[0][j];
            }
        }

        // Step 3: Patch all files to use the determined global ID for each wave
        {
            // Process variation 0
            {
                // TODO: Validate
                nw::snd::internal::WaveSoundFileReader reader(file);
                SEAD_ASSERT(reader.IsAvailable());

                const nw::snd::internal::Util::WaveIdTable* waveIdTable = &reader.mInfoBlockBody->GetWaveIdTable();
                nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable->table.item[0]);
                for (u32 j = 0; j < waveIdTable->GetCount(); j++)
                {
                    waveId[j].waveArchiveId = 0;
                    waveId[j].waveIndex = baseWaveId[j];
                }
            }

            variationId = 1;
            while (true)
            {
                const void* file = soundArchive.detail_GetFileAddress(fileId, nullptr, variationId, nullptr, &groupId);
                if (!file)
                    break;

                const auto& it = fileMap.find(file);
                if (it != fileMap.end())
                {
                    variationId++;
                    continue;
                }

                // TODO: Validate
                nw::snd::internal::WaveSoundFileReader reader(file);
                SEAD_ASSERT(reader.IsAvailable());

                const nw::snd::internal::Util::WaveIdTable* waveIdTable = &reader.mInfoBlockBody->GetWaveIdTable();
                nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable->table.item[0]);
                for (u32 j = 0; j < waveIdTable->GetCount(); j++)
                {
                    waveId[j].waveArchiveId = 0;
                    waveId[j].waveIndex = baseWaveId[j];
                }

                variationId++;
            }
        }
    }

    // {
    //     u32 bankFileId = soundArchive.GetBankInfo(soundArchive.GetBankIdFromIndex(29))->fileId;
    //     nw::snd::internal::BankFileReader reader(soundArchive.detail_GetFileAddressGroup(bankFileId, soundArchive.GetGroupIdFromIndex(5)));
    //     SEAD_ASSERT(reader.IsInitialized());

    //     const nw::snd::internal::Util::WaveIdTable& table = *reader.GetWaveIdTable();
    //     for (u32 i = 0; i < table.GetCount(); i++)
    //     {
    //         const nw::snd::internal::Util::WaveId* waveId = table.GetWaveId(i);
    //         SEAD_ASSERT(waveId);

    //         SEAD_PRINT("WaveId %u, (%u, %u)\n", i, (u32)waveId->waveArchiveId, (u32)waveId->waveIndex);
    //     }
    // }

    // for (u32 i = 0; i < soundArchive.detail_GetFileCount(); i++)
    // {
    //     const void* file = soundArchive.detail_GetFileAddress(i);
    //     if (!file)
    //     {
    //         continue;
    //     }

    //     const nw::snd::internal::Util::WaveIdTable* waveIdTable = nullptr;
    //     if (sead::MemUtil::compare(file, "FBNK", 4) == 0)
    //     {
    //         nw::snd::internal::BankFileReader reader(file);
    //         SEAD_ASSERT(reader.IsInitialized());

    //         waveIdTable = reader.GetWaveIdTable();
    //     }
    //     else if (sead::MemUtil::compare(file, "FWSD", 4) == 0)
    //     {
    //         nw::snd::internal::WaveSoundFileReader reader(file);
    //         SEAD_ASSERT(reader.IsAvailable());

    //         waveIdTable = &reader.mInfoBlockBody->GetWaveIdTable();
    //     }
    //     else
    //     {
    //         continue;
    //     }

    //     SEAD_ASSERT(waveIdTable);

    //     nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable->table.item[0]);

    //     for (u32 j = 0; j < waveIdTable->GetCount(); j++)
    //     {
    //         if (j != 0)
    //         {
    //             SEAD_ASSERT(waveId[j].waveArchiveId == waveId[0].waveArchiveId);
    //         }
    //     }

    //     u32 warcId = nw::snd::SoundArchive::INVALID_ID;
    //     u32 warcIdx = nw::snd::SoundArchive::INVALID_ID;
    //     if (waveIdTable->GetCount() > 0)
    //     {
    //         warcId = waveId[0].waveArchiveId;
    //         SEAD_ASSERT(nw::snd::internal::Util::GetItemType(warcId) == nw::snd::internal::ItemType_WaveArchive);
    //         warcIdx = nw::snd::internal::Util::GetItemIndex(warcId);
    //     }
    //     bankFileWarcId[i] = warcId;

    //     for (u32 j = 0; j < waveIdTable->GetCount(); j++)
    //     {
    //         waveId[j].waveArchiveId = 0;
    //         waveId[j].waveIndex = warcFileCache[warcIdx][waveId[j].waveIndex].id;
    //     }
    // }

    // std::unordered_map<std::string, TempFile> waveFileCache;
    // std::unordered_map<u64, u32> waveFileIdxMap;

    // for (u32 i = 0; i < soundArchive.GetWaveArchiveCount(); i++)
    // {
    //     u32 waveArchiveId = soundArchive.GetWaveArchiveIdFromIndex(i);
    //     const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(waveArchiveId);

    //     nw::snd::internal::WaveArchiveFileReader reader(soundArchive.detail_GetFileAddress(warcInfo->fileId));

    //     for (u32 j = 0; j < reader.GetWaveFileCount(); j++)
    //     {
    //         const void* waveFile = reader.GetWaveFile(j);
    //         u32 waveFileSize = reader.GetWaveFileSize(j);

    //         std::string hash = md5(waveFile, waveFileSize);

    //         bool isUnique = true;
    //         u32 globalId;

    //         const auto& it = waveFileCache.find(hash);
    //         if (it != waveFileCache.end())
    //         {
    //             const TempFile& ref = it->second;
    //             if (ref.fileDataSize == waveFileSize && sead::MemUtil::compare(ref.fileData, waveFile, waveFileSize) == 0)
    //             {
    //                 isUnique = false;
    //                 globalId = ref.id;
    //             }
    //             else
    //             {
    //                 SEAD_PRINT("Hash collision!!!!!\n");
    //             }
    //         }

    //         if (isUnique)
    //         {
    //             globalId = mWaveFileList.size();
                
    //             WaveFile* wave = new(heap) WaveFile();
    //             wave->mId = globalId;

    //             wave->mEnableName = true;
    //             wave->mName = "Wave";

    //             wave->read(waveFile);

    //             mWaveFileList.pushBack(wave);

    //             waveFileCache.try_emplace(hash, globalId, waveFile, waveFileSize);
    //         }

    //         waveFileIdxMap.try_emplace(u64(waveArchiveId) << 32 | j, globalId);
    //     }

    //     if (warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_STRING_ID) == 0)
    //     {
    //         continue;
    //     }

    //     WaveArchive* warc = new(heap) WaveArchive();
    //     warc->mId = i;

    //     warc->mEnableName = warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_STRING_ID) != 0;
    //     if (mIncludeStringTable && warcInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
    //     {
    //         warc->mName = soundArchive.GetString(warcInfo->GetStringId());
    //     }

    //     warc->mIsLoadIndividual = warcInfo->isLoadIndividual;
    //     if (warc->mIsLoadIndividual)
    //     {
    //         SEAD_ASSERT(warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_WAVE_COUNT) != 0);
    //     }

    //     mWaveArchiveList.pushBack(warc);
    // }

    std::unordered_map<std::string, TempFile> bankFileCache;
    std::unordered_map<u32, u32> bankFileIdxMap;

    for (u32 i = 0; i < soundArchive.detail_GetFileCount(); i++)
    {
        u32 bankFileSize = 0;
        const void* bankFile = soundArchive.detail_GetFileAddress(i, &bankFileSize);

        // if (!bankFile || sead::MemUtil::compare(bankFile, "CBNK", 4) != 0)
        if (!bankFile || sead::MemUtil::compare(bankFile, "FBNK", 4) != 0)
        {
            continue;
        }

        std::string hash = md5(bankFile, bankFileSize);

        bool isUnique = true;
        u32 globalId;

        const auto& it = bankFileCache.find(hash);
        if (it != bankFileCache.end())
        {
            const TempFile& ref = it->second;
            if (ref.fileDataSize == bankFileSize && sead::MemUtil::compare(ref.fileData, bankFile, bankFileSize) == 0)
            {
                isUnique = false;
                globalId = ref.id;
            }
            else
            {
                SEAD_PRINT("Hash collision!!!!!\n");
            }
        }

        if (isUnique)
        {
            globalId = mBankFileList.size();
            
            BankFile* bank = new(heap) BankFile();
            bank->mId = globalId;

            bank->mEnableName = true;
            bank->mName = "Bank";

            PopupMgr::instance()->setCurrentProcessItem(bank);
            if (!bank->read(bankFile))
            {
            }

            mBankFileList.pushBack(bank);

            bankFileCache.try_emplace(hash, globalId, bankFile, bankFileSize);
        }

        bankFileIdxMap.try_emplace(i, globalId);
    }

    //? Load Banks first so Sequence Sounds can reference them
    for (u32 i = 0; i < soundArchive.GetBankCount(); i++)
    {
        const nw::snd::internal::SoundArchiveFile::BankInfo* bankInfo = soundArchive.GetBankInfo(soundArchive.GetBankIdFromIndex(i));
        if (!bankInfo)
        {
            PopupMgr::instance()->setCorruptInfo("Invalid BankInfo");
            return false;
        }

        Bank* bank = new(heap) Bank();
        bank->mId = i;
        PopupMgr::instance()->setCurrentProcessItem(bank);

        bank->mEnableName = bankInfo->optionParameter.GetTrueCount(nw::snd::internal::BANK_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && bankInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            bank->mName = soundArchive.GetString(bankInfo->GetStringId());
        }
        else
        {
            bank->mName.format("BANK_%u", i);
        }

        const nw::snd::internal::Util::Table<nw::ut::ResU32>* warcIdTable = bankInfo->GetWaveArchiveItemIdTable();
        SEAD_ASSERT(warcIdTable->count <= 1);

        if (warcIdTable->count > 0)
        {
            bank->mWaveArchiveType = WaveArchiveType::AutomaticIndividual;

            bank->mWaveArchiveRef.attach(getItem(warcIdTable->item[0], getWaveArchiveList()));
            if (bank->mWaveArchiveRef.isAttached())
            {
                bank->mWaveArchiveType = WaveArchiveType::Explicit;
            }
        }
        else
        {
            bank->mWaveArchiveType = WaveArchiveType::AutomaticShared;
        }

        if (bank->mWaveArchiveType == WaveArchiveType::Invalid)
        {
            PopupMgr::instance()->pushCurrentItemError("Invalid WaveArchiveType");
        }

        BankFile* bankFile = static_cast<BankFile*>(getItem(bankFileIdxMap[bankInfo->fileId], getBankFileList()));

        bank->mFileRef.attach(bankFile);
        if (bank->mFileRef.isAttached())
        {
            if (bankFile->mName == "Bank")
            {
                bankFile->mName.format("GUESS_%s", bank->mName.cstr());
            }
        }
        else
        {
            PopupMgr::instance()->pushCurrentItemError("Couldn't load the BankFile referenced");
        }

        mBankList.pushBack(bank);
    }

    for (u32 i = 0; i < soundArchive.GetSoundCount(); i++)
    {
        const nw::snd::internal::SoundArchiveFile::SoundInfo* soundInfo = soundArchive.GetSoundInfo(soundArchive.GetSoundIdFromIndex(i));
        if (!soundInfo)
        {
            PopupMgr::instance()->setCorruptInfo("Invalid SoundInfo");
            return false;
        }

        Sound* sound = new(heap) Sound();
        sound->mId = i;
        PopupMgr::instance()->setCurrentProcessItem(sound);

        sound->mEnableName = soundInfo->optionParameter.GetTrueCount(nw::snd::internal::SOUND_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && soundInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            sound->mName = soundArchive.GetString(soundInfo->GetStringId());
        }
        else
        {
            sound->mName.format("SOUND_%u", i);
        }

        sound->mPlayerRef.attach(getItem(soundInfo->playerId, getPlayerList()));
        if (!sound->mPlayerRef.isAttached())
        {
            PopupMgr::instance()->pushCurrentItemError("Invalid Player reference");
        }

        sound->mVolume = soundInfo->volume;
        sound->mRemoteFilter = soundInfo->remoteFilter;
        sound->mSoundType = static_cast<Sound::SoundType>(soundInfo->GetSoundType());

        sound->mEnablePanParam = soundInfo->optionParameter.GetTrueCount(nw::snd::internal::SOUND_INFO_PAN_PARAM) != 0;
        if (sound->mEnablePanParam)
        {
            sound->mPanMode = static_cast<snd::PanMode>(soundInfo->GetPanMode());
            sound->mPanCurve = static_cast<snd::PanCurve>(soundInfo->GetPanCurve());
        }

        sound->mEnablePlayerParam = soundInfo->optionParameter.GetTrueCount(nw::snd::internal::SOUND_INFO_PLAYER_PARAM) != 0;
        if (sound->mEnablePlayerParam)
        {
            sound->mPlayerPriority = soundInfo->GetPlayerPriority();
            sound->mActorPlayerId = soundInfo->GetActorPlayerId();
        }

        for (u32 j = 0; j < nw::snd::internal::USER_PARAM_COUNT; j++)
        {
            sound->mEnableUserParam[j] = soundInfo->optionParameter.GetTrueCount(nw::snd::internal::USER_PARAM_INDEX[j]) != 0;

            if (sound->mEnableUserParam[j])
                soundInfo->ReadUserParam(j, sound->mUserParam[j]);
        }

        sound->mEnableIsFrontBypass = soundInfo->optionParameter.GetTrueCount(nw::snd::internal::SOUND_INFO_OFFSET_TO_CTR_PARAM) != 0;
        if (sound->mEnableIsFrontBypass)
            sound->mIsFrontBypass = soundInfo->IsFrontBypass();

        sound->mEnableSound3DInfo = soundInfo->optionParameter.GetTrueCount(nw::snd::internal::SOUND_INFO_OFFSET_TO_3D_PARAM) != 0;
        if (sound->mEnableSound3DInfo)
        {
            const nw::snd::internal::SoundArchiveFile::Sound3DInfo* sound3DInfo = soundInfo->GetSound3DInfo();

            sound->mSound3DInfo.mFlags = sound3DInfo->flags;
            sound->mSound3DInfo.mDecayRatio = sound3DInfo->decayRatio;
            sound->mSound3DInfo.mDecayCurve = sound3DInfo->decayCurve;
            sound->mSound3DInfo.mDopplerFactor = sound3DInfo->dopplerFactor;
        }

        if (sound->mSoundType == Sound::SoundType::Seq)
        {
            const nw::snd::internal::SoundArchiveFile::SequenceSoundInfo& seqSoundInfo = soundInfo->GetSequenceSoundInfo();

            const nw::snd::internal::Util::Table<nw::ut::ResU32>& table = seqSoundInfo.GetBankIdTable();
            for (u32 j = 0; j < table.count; j++)
            {
                if (table.item[j] != nw::snd::SoundArchive::INVALID_ID)
                {
                    sound->mSequenceSoundInfo.mBankRefs[j]->attach(getItem(table.item[j], getBankList()));
                    if (!sound->mSequenceSoundInfo.mBankRefs[j]->isAttached())
                    {
                        sead::FormatFixedSafeString<64> msg("Bank[%u] reference is invalid", j);
                        PopupMgr::instance()->pushCurrentItemError(msg);
                    }
                }
            }

            u32 globalSeqIdx = seqFileIdxMap[soundInfo->fileId];
            SequenceFile* seqFile = static_cast<SequenceFile*>(getItem(globalSeqIdx, getSequenceFileList()));

            sound->mSequenceSoundInfo.mSequenceFileRef.attach(seqFile);
            if (sound->mSequenceSoundInfo.mSequenceFileRef.isAttached())
            {
                if (seqFile->mName == "Sequence")
                {
                    seqFile->mName.format("GUESS_%s", sound->mName.cstr());
                }
            }
            else
            {
                PopupMgr::instance()->pushCurrentItemError("Couldn't load the SequenceFile referenced");
            }

            sound->mSequenceSoundInfo.mEnableStartOffset = seqSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::SEQ_SOUND_INFO_START_OFFSET) != 0;
            if (sound->mSequenceSoundInfo.mEnableStartOffset && seqFile)
            {
                std::string label = seqFile->getLabelFromParsedOffset(seqSoundInfo.GetStartOffset(), seqSoundInfo.allocateTrackFlags);
                sound->mSequenceSoundInfo.mStartLabel.copy(label.c_str());
            }

            sound->mSequenceSoundInfo.mEnablePriority = seqSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::SEQ_SOUND_INFO_PRIORITY) != 0;
            if (sound->mSequenceSoundInfo.mEnablePriority)
            {
                sound->mSequenceSoundInfo.mChannelPriority = seqSoundInfo.GetChannelPriority();
                sound->mSequenceSoundInfo.mIsReleasePriorityFix = seqSoundInfo.IsReleasePriorityFix();
            }
        }
        else if (sound->mSoundType == Sound::SoundType::Strm)
        {
            const nw::snd::internal::SoundArchiveFile::StreamSoundInfo& strmSoundInfo = soundInfo->GetStreamSoundInfo();

            const nw::snd::internal::SoundArchiveFile::ExternalFileInfo* extFileInfo = soundArchive.detail_GetFileInfo(soundInfo->fileId)->GetExternalFileInfo();
            SEAD_ASSERT(extFileInfo);

            sound->mStreamSoundInfo.mPath = extFileInfo->filePath;

            //sound->mStreamSoundInfo.mAllocateTrackFlags = strmSoundInfo.allocateTrackFlags;
            //sound->mStreamSoundInfo.mAllocateChannelCount = strmSoundInfo.allocateChannelCount;

            sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
            SEAD_ASSERT(device);

            sead::FixedSafeString<512> dir;
            bool b = sead::Path::getDirectoryName(&dir, getFilePath());
            SEAD_ASSERT(b);

            if (sound->mStreamSoundInfo.mPath.isEmpty())
            {
                PopupMgr::instance()->pushCurrentItemError("Path is empty");
            }

            const char* filePath = sound->mStreamSoundInfo.mPath.cstr();

            sead::FixedSafeString<512> path;
            path.format("%s/%s", dir.cstr(), filePath);
            //SEAD_PRINT("%s\n", path.cstr());

            sead::FileDevice::LoadArg loadArg;
            loadArg.path = path;

            bool validStrmFile = false;

            u8* strmFile = device->tryLoad(loadArg);
            if (strmFile)
            {
                // if (sead::MemUtil::compare(strmFile, "CSTM", 4) == 0)
                if (sead::MemUtil::compare(strmFile, "FSTM", 4) == 0)
                {
                    nw::snd::internal::StreamSoundFileReader reader;
                    reader.Initialize(strmFile);

                    if (reader.IsAvailable())
                    {
                        validStrmFile = true;

                        // If track information is embedded in bXstm (up to binary version 0.2.0.0)
                        if (reader.IsTrackInfoAvailable())
                        {
                            u32 trackCount = reader.GetTrackCount();
                            if (trackCount > cStrmTrackNum)
                            {
                                trackCount = cStrmTrackNum;
                            }

                            // Read track information.
                            for (u32 j = 0; j < trackCount; j++)
                            {
                                nw::snd::internal::StreamSoundFileReader::TrackInfo trackInfo;
                                b = reader.ReadStreamTrackInfo(&trackInfo, j);
                                SEAD_ASSERT(b);

                                Sound::StreamSoundInfo::Track* track = new(heap) Sound::StreamSoundInfo::Track();
                                track->mId = j;

                                track->mEnableName = true;
                                track->mName = "Track";

                                track->mVolume = trackInfo.volume;
                                track->mPan = trackInfo.pan;
                                track->mSPan = trackInfo.span;
                                track->mFlags = trackInfo.flags;

                                u8 channelCount = trackInfo.channelCount;
                                SEAD_ASSERT(channelCount <= nw::snd::WAVE_CHANNEL_MAX);

                                for (u8 k = 0; k < channelCount; k++)
                                {
                                    u8& channel = *track->mChannels.birthBack();
                                    channel = trackInfo.globalChannelIndex[k];
                                }

                                sound->mStreamSoundInfo.mTrackList.pushBack(track);
                            }
                        }
                    }
                }
                else
                {
                    sead::FormatFixedSafeString<1024> msg("The file '%s' is not a valid BFSTM file", path.cstr());
                    PopupMgr::instance()->pushCurrentItemError(msg);
                }
            }
            else
            {
                sead::FormatFixedSafeString<1024> msg("Couldn't load '%s'", filePath);
                PopupMgr::instance()->pushCurrentItemError(msg);
            }

            if (isStreamTrackInfoAvailable())
            {
                if (sound->mStreamSoundInfo.mTrackList.isEmpty())
                {
                    const nw::snd::internal::SoundArchiveFile::StreamTrackInfoTable* trackInfoTable = strmSoundInfo.GetTrackInfoTable();
                    //SEAD_ASSERT(trackInfoTable);

                    if (trackInfoTable)
                    {
                        for (u32 j = 0; j < nw::snd::SoundArchive::STRM_TRACK_NUM && j < trackInfoTable->GetTrackCount(); j++)
                        {
                            const nw::snd::internal::SoundArchiveFile::StreamTrackInfo* trackInfo = trackInfoTable->GetTrackInfo(j);
                            SEAD_ASSERT(trackInfo);

                            Sound::StreamSoundInfo::Track* track = new(heap) Sound::StreamSoundInfo::Track();
                            track->mId = j;

                            track->mEnableName = true;
                            track->mName = "Track";

                            track->mVolume = trackInfo->volume;
                            track->mPan = trackInfo->pan;
                            track->mSPan = trackInfo->span;
                            track->mFlags = trackInfo->flags;

                            u32 channelCount = trackInfo->GetTrackChannelCount();
                            SEAD_ASSERT(channelCount <= nw::snd::WAVE_CHANNEL_MAX);

                            for (u32 k = 0; k < channelCount; k++)
                            {
                                u8& channel = *track->mChannels.birthBack();
                                channel = trackInfo->GetGlobalChannelIndex(k);
                            }

                            if (isStreamSendAvailable())
                            {
                                const nw::snd::internal::SoundArchiveFile::SendValue send = trackInfo->GetSendValue();
                                track->mMainSend = send.mainSend;

                                for (u32 k = 0; k < nw::snd::AUX_BUS_NUM; k++)
                                {
                                    track->mFxSend[k] = send.fxSend[k];
                                }
                            }

                            if (isFilterSupportedVersion())
                            {
                                track->mLpfFreq = trackInfo->lpfFreq;
                                track->mBiquadType = trackInfo->biquadType;
                                track->mBiquadValue = trackInfo->biquadValue;
                            }

                            sound->mStreamSoundInfo.mTrackList.pushBack(track);
                        }
                    }
                }

                if (isStreamSendAvailable())
                {
                    sound->mStreamSoundInfo.mPitch = strmSoundInfo.pitch;

                    const nw::snd::internal::SoundArchiveFile::SendValue send = strmSoundInfo.GetSendValue();
                    sound->mStreamSoundInfo.mMainSend = send.mainSend;

                    for (u32 j = 0; j < nw::snd::AUX_BUS_NUM; j++)
                    {
                        sound->mStreamSoundInfo.mFxSend[j] = send.fxSend[j];
                    }

                    sound->mStreamSoundInfo.mEnableStreamSoundExtension = strmSoundInfo.GetStreamSoundExtension() != nullptr;
                    if (sound->mStreamSoundInfo.mEnableStreamSoundExtension)
                    {
                        const nw::snd::internal::SoundArchiveFile::StreamSoundExtension* soundExt = strmSoundInfo.GetStreamSoundExtension();

                        sound->mStreamSoundInfo.mStreamType = static_cast<Sound::StreamSoundInfo::StreamType>(soundExt->GetStreamFileType());
                        sound->mStreamSoundInfo.mIsLoop = soundExt->IsLoop();
                        sound->mStreamSoundInfo.mLoopStartFrame = soundExt->loopStartFrame;
                        sound->mStreamSoundInfo.mLoopEndFrame = soundExt->loopEndFrame;
                    }

                    if (isStreamPrefetchAvailable())
                    {
                        // TODO
                    }
                }
            }

            if (validStrmFile)
            {
                extern bool ReadStreamWaves(Sound* sound, const void* strmFile);

                // TODO: Only load the same bfstm file once ?
                ReadStreamWaves(sound, strmFile);
            }

            if (strmFile)
            {
                device->unload(strmFile);
            }

            if (sound->mStreamSoundInfo.mTrackList.isEmpty())
            {
                PopupMgr::instance()->pushCurrentItemError("Couldn't find any Track information");
            }
        }
        else if (sound->mSoundType == Sound::SoundType::Wave)
        {
            const nw::snd::internal::SoundArchiveFile::WaveSoundInfo& waveSoundInfo = soundInfo->GetWaveSoundInfo();

            sound->mWaveSoundInfo.mAllocateTrackCount = waveSoundInfo.allocateTrackCount;

            sound->mWaveSoundInfo.mEnablePriority = waveSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::WAVE_SOUND_INFO_PRIORITY) != 0;
            if (sound->mWaveSoundInfo.mEnablePriority)
            {
                sound->mWaveSoundInfo.mChannelPriority = waveSoundInfo.GetChannelPriority();
                sound->mWaveSoundInfo.mIsReleasePriorityFix = waveSoundInfo.GetIsReleasePriorityFix();
            }

            const void* wsdFile = soundArchive.detail_GetFileAddress(soundInfo->fileId);
            if (wsdFile)
            {
                // TODO: Validate
                nw::snd::internal::WaveSoundFileReader reader(wsdFile);
                SEAD_ASSERT(reader.IsAvailable());

                nw::snd::internal::WaveSoundNoteInfo noteInfo;
                bool success = reader.ReadNoteInfo(&noteInfo, waveSoundInfo.index, 0);
                SEAD_ASSERT(success);

                //? noteInfo.waveIndex is patched with global wave index already
                SEAD_ASSERT(noteInfo.waveArchiveId == 0);
                WaveFile* waveFile = static_cast<WaveFile*>(getItem(noteInfo.waveIndex, getWaveFileList()));

                sound->mWaveSoundInfo.mWaveFileRef.attach(waveFile);
                if (sound->mWaveSoundInfo.mWaveFileRef.isAttached())
                {
                    if (waveFile->mName == "Wave")
                    {
                        waveFile->mName.format("GUESS_%s", sound->mName.cstr());
                    }
                }
                else
                {
                    PopupMgr::instance()->pushCurrentItemError("Couldn't load the WaveFile referenced");
                }

                const nw::snd::internal::WaveSoundFile::WaveSoundInfo& innerWaveSoundInfo = reader.GetWaveSoundInfo(waveSoundInfo.index);

                sound->mWaveSoundInfo.mEnablePan = innerWaveSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::WAVE_SOUND_INFO_PAN) != 0;
                if (sound->mWaveSoundInfo.mEnablePan)
                {
                    sound->mWaveSoundInfo.mPan = innerWaveSoundInfo.GetPan();
                    sound->mWaveSoundInfo.mSurroundPan = innerWaveSoundInfo.GetSurroundPan();
                }

                sound->mWaveSoundInfo.mEnablePitch = innerWaveSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::WAVE_SOUND_INFO_PITCH) != 0;
                if (sound->mWaveSoundInfo.mEnablePitch)
                {
                    sound->mWaveSoundInfo.mPitch = innerWaveSoundInfo.GetPitch();
                }

                sound->mWaveSoundInfo.mEnableSend = innerWaveSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::WAVE_SOUND_INFO_SEND) != 0;
                if (sound->mWaveSoundInfo.mEnableSend)
                {
                    innerWaveSoundInfo.GetSendValue(&sound->mWaveSoundInfo.mMainSend, sound->mWaveSoundInfo.mFxSend, nw::snd::AUX_BUS_NUM);
                }

                sound->mWaveSoundInfo.mEnableEnvelope = innerWaveSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::WAVE_SOUND_INFO_ENVELOPE) != 0;
                if (sound->mWaveSoundInfo.mEnableEnvelope)
                {
                    const nw::snd::AdshrCurve& adshrCurveInfo = innerWaveSoundInfo.GetAdshrCurve();
                    sound->mWaveSoundInfo.mAdshrCurve.attack = adshrCurveInfo.attack;
                    sound->mWaveSoundInfo.mAdshrCurve.decay = adshrCurveInfo.decay;
                    sound->mWaveSoundInfo.mAdshrCurve.sustain = adshrCurveInfo.sustain;
                    sound->mWaveSoundInfo.mAdshrCurve.hold = adshrCurveInfo.hold;
                    sound->mWaveSoundInfo.mAdshrCurve.release = adshrCurveInfo.release;
                }

                if (BfwsdFile::isFilterSupportedVersion(reader.mHeader->header.version))
                {
                    sound->mWaveSoundInfo.mEnableFilter = innerWaveSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::WAVE_SOUND_INFO_FILTER) != 0;
                    if (sound->mWaveSoundInfo.mEnableFilter)
                    {
                        sound->mWaveSoundInfo.mLpfFreq = innerWaveSoundInfo.GetLpfFreq();
                        sound->mWaveSoundInfo.mBiquadType = innerWaveSoundInfo.GetBiquadType();
                        sound->mWaveSoundInfo.mBiquadValue = innerWaveSoundInfo.GetBiquadValue();
                    }
                }
            }
            else
            {
                PopupMgr::instance()->pushCurrentItemError("Couldn't load the BFWSD file referenced");
            }
        }

        mSoundList.pushBack(sound);
    }

    //? Guess Bank Wave File names after Wave Sounds because they reference them directly
    for (Item* bankItem : mBankList)
    {
        Bank* bank = static_cast<Bank*>(bankItem);
        if (!bank->mFileRef.isAttached())
        {
            continue;
        }

        BankFile* bankFile = static_cast<BankFile*>(bank->mFileRef.getItem());

        for (Item* instrItem : bankFile->getInstrumentList())
        {
            BankFile::Instrument& instr = *static_cast<BankFile::Instrument*>(instrItem);
            for (Item* keyRegionItem : instr.getKeyRegionList())
            {
                BankFile::KeyRegion& keyRegion = *static_cast<BankFile::KeyRegion*>(keyRegionItem);
                for (Item* velRegionItem : keyRegion.getVelocityRegionList())
                {
                    BankFile::VelocityRegion& velRegion = *static_cast<BankFile::VelocityRegion*>(velRegionItem);
                    if (velRegion.getWaveFileRef().isAttached())
                    {
                        Item* waveFile = velRegion.getWaveFileRef().getItem();
                        if (waveFile->mName == "Wave")
                        {
                            waveFile->mName.format("GUESS_%s_INSTR_%u", bank->mName.cstr(), instr.mId);
                        }
                    }
                }
            }
        }
    }

    for (u32 i = 0; i < soundArchive.GetSoundGroupCount(); i++)
    {
        const nw::snd::internal::SoundArchiveFile::SoundGroupInfo* soundSetInfo = soundArchive.detail_GetSoundGroupInfo(soundArchive.GetSoundGroupIdFromIndex(i));
        if (!soundSetInfo)
        {
            PopupMgr::instance()->setCorruptInfo("Invalid SoundGroupInfo");
            return false;
        }

        SoundSet* soundSet = new(heap) SoundSet();
        soundSet->mId = i;
        PopupMgr::instance()->setCurrentProcessItem(soundSet);

        soundSet->mEnableName = soundSetInfo->optionParameter.GetTrueCount(nw::snd::internal::SOUND_GROUP_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && soundSetInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            soundSet->mName = soundArchive.GetString(soundSetInfo->GetStringId());
        }
        else
        {
            if (soundSetInfo->GetWaveSoundGroupInfo())
            {
                soundSet->mName.format("WSDSET_%u", i);
            }
            else
            {
                soundSet->mName.format("SEQSET_%u", i);
            }
        }

        const nw::snd::internal::SoundArchiveFile::WaveSoundGroupInfo* waveSoundSetInfo = soundSetInfo->GetWaveSoundGroupInfo();
        if (waveSoundSetInfo)
        {
            soundSet->mSoundSetType = SoundSet::SoundSetType::Wave;

            const nw::snd::internal::Util::Table<nw::ut::ResU32>* warcIdTable = waveSoundSetInfo->GetWaveArchiveItemIdTable();
            SEAD_ASSERT(warcIdTable->count <= 1);

            if (warcIdTable->count > 0)
            {
                soundSet->mWaveArchiveType = WaveArchiveType::AutomaticIndividual;

                soundSet->mWaveArchiveRef.attach(getItem(warcIdTable->item[0], getWaveArchiveList()));
                if (soundSet->mWaveArchiveRef.isAttached())
                {
                    soundSet->mWaveArchiveType = WaveArchiveType::Explicit;
                }
            }
            else
            {
                soundSet->mWaveArchiveType = WaveArchiveType::AutomaticShared;
            }
        }
        else
        {
            soundSet->mSoundSetType = SoundSet::SoundSetType::Seq;

            //? In case the user change the SoundSet type to Wave use this as the default WARC type
            soundSet->mWaveArchiveType = WaveArchiveType::AutomaticShared;
        }

        if (soundSet->mWaveArchiveType == WaveArchiveType::Invalid)
        {
            PopupMgr::instance()->pushCurrentItemError("Invalid WaveArchiveType");
        }

        if (soundSetInfo->startId == nw::snd::SoundArchive::INVALID_ID && soundSetInfo->endId == nw::snd::SoundArchive::INVALID_ID)
        {
            soundSet->mIsEmpty = true;
        }
        else
        {
            soundSet->mStartId = nw::snd::internal::Util::GetItemIndex(soundSetInfo->startId);
            soundSet->mEndId = nw::snd::internal::Util::GetItemIndex(soundSetInfo->endId);
        }

        mSoundSetList.pushBack(soundSet);
    }

    //? Read Bfgrp file
    for (u32 i = 0; i < soundArchive.GetGroupCount(); i++)
    {
        u32 groupId = soundArchive.GetGroupIdFromIndex(i);
        const nw::snd::internal::SoundArchiveFile::GroupInfo* groupInfo = soundArchive.GetGroupInfo(groupId);
        if (!groupInfo)
        {
            PopupMgr::instance()->setCorruptInfo("Invalid GroupInfo");
            return false;
        }

        Group* group = static_cast<Group*>(mGroupList.nth(i)->val());
        PopupMgr::instance()->setCurrentProcessItem(group);

        if (groupInfo->fileId != nw::snd::SoundArchive::INVALID_ID)
        {
            nw::snd::internal::GroupFileReader reader(soundArchive.detail_GetFileAddress(groupInfo->fileId));
            if (groupInfo->fileId >= soundArchive.detail_GetFileCount())
            {
                group->mOutputType = Group::OutputType::External;
            }
            else if (reader.GetGroupItemCount() > 0)
            {
                nw::snd::internal::GroupItemLocationInfo locationInfo;
                bool success = reader.ReadGroupItemLocationInfo(&locationInfo, 0);
                SEAD_ASSERT(success);

                if (locationInfo.address)
                {
                    group->mOutputType = Group::OutputType::Embed;
                }
                else
                {
                    group->mOutputType = Group::OutputType::Link;
                }
            }
            else
            {
                //? Can't know OutputType... Fallback to Embed
                group->mOutputType = Group::OutputType::Embed;
                PopupMgr::instance()->pushCurrentItemError("Couldn't find Output Type");
            }

            auto addGroupItem = [&](u32 itemIdx, bool assertNotDisabled)
            {
                nw::snd::internal::GroupFile::GroupItemInfoEx itemInfoEx;
                bool success = reader.ReadGroupItemInfoEx(&itemInfoEx, itemIdx);
                SEAD_ASSERT(success);

                bool itemDisabled = itemInfoEx.itemId == 0 || itemInfoEx.itemId == nw::snd::SoundArchive::INVALID_ID;
                if (assertNotDisabled)
                {
                    if (itemDisabled)
                    {
                        sead::FormatFixedSafeString<1024> msg("Item %u is invalid", itemIdx);
                        PopupMgr::instance()->pushCurrentItemError(msg);
                    }
                }

                Group::ItemInfo* itemInfo = new(heap) Group::ItemInfo(group);
                itemInfo->mId = itemIdx;

                itemInfo->mIsDisabled = itemDisabled;

                if (!itemDisabled)
                {
                    switch (nw::snd::internal::Util::GetItemType(itemInfoEx.itemId))
                    {
                        case nw::snd::internal::ItemType_Sound:
                            itemInfo->mItemRefType = Item::ItemType::Sound;
                            break;

                        case nw::snd::internal::ItemType_SoundGroup:
                            itemInfo->mItemRefType = Item::ItemType::SoundSet;
                            break;

                        case nw::snd::internal::ItemType_Bank:
                            itemInfo->mItemRefType = Item::ItemType::Bank;
                            break;

                        case nw::snd::internal::ItemType_WaveArchive:
                            itemInfo->mItemRefType = Item::ItemType::WaveArchive;
                            break;

                        default:
                            itemInfo->mItemRefType = Item::ItemType::Invalid;
                            break;
                    }

                    itemInfo->mItemRef.attach(getItem(itemInfoEx.itemId, getItemList(itemInfo->mItemRefType)));
                    if (!itemInfo->mItemRef.isAttached())
                    {
                        sead::FormatFixedSafeString<1024> msg("Item %u is invalid", itemIdx);
                        PopupMgr::instance()->pushCurrentItemError(msg);
                    }

                    itemInfo->setLoadItems(itemInfoEx.loadFlag);
                }

                group->mItemInfoList.pushBack(itemInfo);
            };

            // if (mVersion <= BfgrpFile::cIncludeDisabledItemsVersion)
            // {
            //     for (u32 j = 0; j < reader.GetGroupItemExCount(); j++)
            //     {
            //         addGroupItem(j, false);
            //     }
            // }
            // else
            {
                std::vector<VectorSet<u32>> itemFileMap(reader.GetGroupItemExCount());

                for (u32 j = 0; j < reader.GetGroupItemExCount(); j++)
                {
                    nw::snd::internal::GroupFile::GroupItemInfoEx itemInfoEx;
                    bool success = reader.ReadGroupItemInfoEx(&itemInfoEx, j);
                    SEAD_ASSERT(success);

                    auto addBankFiles = [&](u32 bankId)
                    {
                        const nw::snd::internal::SoundArchiveFile::BankInfo* bankInfo = soundArchive.GetBankInfo(bankId);
                        SEAD_ASSERT(bankInfo);

                        if (itemInfoEx.loadFlag & Group::ItemInfo::LoadFlag::LoadBank)
                        {
                            itemFileMap[j].insert(bankInfo->fileId);
                        }

                        if (itemInfoEx.loadFlag & Group::ItemInfo::LoadFlag::LoadWarc)
                        {
                            const Item* bankItem = getItem(bankId, getBankList());
                            SEAD_ASSERT(bankItem);
                            const Bank* bank = static_cast<const Bank*>(bankItem);

                            switch (bank->getWaveArchiveType())
                            {
                                case WaveArchiveType::AutomaticShared:
                                {
                                    const void* bankFile = soundArchive.detail_GetFileAddressGroup(bankInfo->fileId, groupId);
                                    if (!bankFile)
                                    {
                                        SEAD_ASSERT(group->mOutputType == Group::OutputType::Link);
                                        bankFile = soundArchive.detail_GetFileAddressSimple(bankInfo->fileId);
                                        SEAD_ASSERT(bankFile);
                                    }

                                    // TODO: Validate
                                    nw::snd::internal::BankFileReader reader(bankFile);
                                    SEAD_ASSERT(reader.IsInitialized());

                                    const nw::snd::internal::Util::WaveIdTable* waveIdTable = reader.GetWaveIdTable();
                                    if (waveIdTable->GetCount() > 0)
                                    {
                                        u32 realGroupId = group->mOutputType == Group::OutputType::Link ? 0xFFFFFFFF : groupId;
                                        const auto& it = bankFileWarcId.find(u64(bankInfo->fileId) << 32 | realGroupId);
                                        SEAD_ASSERT(it != bankFileWarcId.end());

                                        u32 warcId = it->second;

                                        const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(warcId);
                                        SEAD_ASSERT(warcInfo);

                                        itemFileMap[j].insert(warcInfo->fileId);
                                    }
                                    break;
                                }

                                case WaveArchiveType::AutomaticIndividual:
                                case WaveArchiveType::Explicit:
                                {
                                    SEAD_ASSERT(bankInfo->GetWaveArchiveItemIdTable()->count == 1);
                                    u32 warcId = bankInfo->GetWaveArchiveItemIdTable()->item[0];

                                    const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(warcId);
                                    SEAD_ASSERT(warcInfo);

                                    itemFileMap[j].insert(warcInfo->fileId);
                                    break;
                                }
                            }
                        }
                    };

                    auto addSoundFiles = [&](u32 soundId, bool assertSeq)
                    {
                        const nw::snd::internal::SoundArchiveFile::SoundInfo* soundInfo = soundArchive.GetSoundInfo(soundId);
                        SEAD_ASSERT(soundInfo);
                        if (soundInfo->GetSoundType() != nw::snd::SoundArchive::SOUND_TYPE_SEQ)
                        {
                            if (assertSeq)
                            {
                                sead::FormatFixedSafeString<1024> msg("Item %u is invalid", j);
                                PopupMgr::instance()->pushCurrentItemError(msg);
                            }

                            // itemFileMap[j].insert(soundInfo->fileId);
                            return;
                        }

                        if (itemInfoEx.loadFlag & Group::ItemInfo::LoadFlag::LoadSeq)
                        {
                            itemFileMap[j].insert(soundInfo->fileId);
                        }

                        const nw::snd::internal::SoundArchiveFile::SequenceSoundInfo& seqSoundInfo = soundInfo->GetSequenceSoundInfo();
                        u32 bankIds[nw::snd::SoundArchive::SEQ_BANK_MAX];
                        seqSoundInfo.GetBankIds(bankIds);

                        for (u32 k = 0; k < nw::snd::SoundArchive::SEQ_BANK_MAX; k++)
                        {
                            if (bankIds[k] == nw::snd::SoundArchive::INVALID_ID)
                            {
                                continue;
                            }

                            addBankFiles(bankIds[k]);
                        }
                    };

                    switch (nw::snd::internal::Util::GetItemType(itemInfoEx.itemId))
                    {
                        case nw::snd::internal::ItemType_Sound:
                        {
                            addSoundFiles(itemInfoEx.itemId, true);
                            // addSoundFiles(itemInfoEx.itemId, false);
                            break;
                        }

                        case nw::snd::internal::ItemType_SoundGroup:
                        {
                            const nw::snd::internal::SoundArchiveFile::SoundGroupInfo* soundSetInfo = soundArchive.detail_GetSoundGroupInfo(itemInfoEx.itemId);
                            SEAD_ASSERT(soundSetInfo);

                            const nw::snd::internal::SoundArchiveFile::WaveSoundGroupInfo* waveSoundSetInfo = soundSetInfo->GetWaveSoundGroupInfo();
                            if (waveSoundSetInfo)
                            {
                                SEAD_ASSERT(soundSetInfo->GetFileIdTable()->count == 1);
                                u32 waveSoundFileId = soundSetInfo->GetFileIdTable()->item[0];
                                if (itemInfoEx.loadFlag & Group::ItemInfo::LoadFlag::LoadWsd)
                                {
                                    itemFileMap[j].insert(waveSoundFileId);
                                }

                                if (itemInfoEx.loadFlag & Group::ItemInfo::LoadFlag::LoadWarc)
                                {
                                    const Item* soundSetItem = getItem(itemInfoEx.itemId, getSoundSetList());
                                    SEAD_ASSERT(soundSetItem);
                                    const SoundSet* soundSet = static_cast<const SoundSet*>(soundSetItem);
                                    SEAD_ASSERT(soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave);

                                    switch (soundSet->getWaveArchiveType())
                                    {
                                        case WaveArchiveType::AutomaticShared:
                                        {
                                            const void* waveSoundFile = soundArchive.detail_GetFileAddressGroup(waveSoundFileId, groupId);
                                            if (!waveSoundFile)
                                            {
                                                SEAD_ASSERT(group->mOutputType == Group::OutputType::Link);
                                                waveSoundFile = soundArchive.detail_GetFileAddressSimple(waveSoundFileId);
                                                SEAD_ASSERT(waveSoundFile);
                                            }

                                            // TODO: Validate
                                            nw::snd::internal::WaveSoundFileReader reader(waveSoundFile);
                                            SEAD_ASSERT(reader.IsAvailable());

                                            const nw::snd::internal::Util::WaveIdTable& waveIdTable = reader.mInfoBlockBody->GetWaveIdTable();
                                            if (waveIdTable.GetCount() > 0)
                                            {
                                                u32 realGroupId = group->mOutputType == Group::OutputType::Link ? 0xFFFFFFFF : groupId;
                                                const auto& it = wsdFileWarcId.find(u64(waveSoundFileId) << 32 | realGroupId);
                                                SEAD_ASSERT(it != wsdFileWarcId.end());

                                                u32 warcId = it->second;

                                                const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(warcId);
                                                SEAD_ASSERT(warcInfo);

                                                itemFileMap[j].insert(warcInfo->fileId);
                                            }
                                            break;
                                        }

                                        case WaveArchiveType::AutomaticIndividual:
                                        case WaveArchiveType::Explicit:
                                        {
                                            SEAD_ASSERT(waveSoundSetInfo->GetWaveArchiveItemIdTable()->count == 1);
                                            u32 warcId = waveSoundSetInfo->GetWaveArchiveItemIdTable()->item[0];

                                            const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(warcId);
                                            SEAD_ASSERT(warcInfo);

                                            itemFileMap[j].insert(warcInfo->fileId);
                                            break;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                if (soundSetInfo->startId != soundSetInfo->endId || soundSetInfo->startId != nw::snd::SoundArchive::INVALID_ID)
                                {
                                    for (u32 id = soundSetInfo->startId; id <= soundSetInfo->endId; id++)
                                    {
                                        addSoundFiles(id, false);
                                    }
                                }
                            }
                            break;
                        }

                        case nw::snd::internal::ItemType_Bank:
                        {
                            addBankFiles(itemInfoEx.itemId);
                            break;
                        }

                        case nw::snd::internal::ItemType_WaveArchive:
                        {
                            SEAD_ASSERT(itemInfoEx.loadFlag == Group::ItemInfo::LoadFlag::LoadAll);

                            const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(itemInfoEx.itemId);
                            SEAD_ASSERT(warcInfo);

                            itemFileMap[j].insert(warcInfo->fileId);
                            break;
                        }
                    }
                }

                auto isAnonWarc = [&](u32 fileId)
                {
                    for (u32 j = 0; j < soundArchive.GetWaveArchiveCount(); j++)
                    {
                        const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = soundArchive.GetWaveArchiveInfo(soundArchive.GetWaveArchiveIdFromIndex(j));
                        SEAD_ASSERT(warcInfo);

                        if (warcInfo->fileId == fileId)
                        {
                            return warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_STRING_ID) == 0;
                        }
                    }

                    return false;
                };

                for (u32 j = 0; j < reader.GetGroupItemCount(); j++)
                {
                    nw::snd::internal::GroupItemLocationInfo locationInfo;
                    bool success = reader.ReadGroupItemLocationInfo(&locationInfo, j);
                    SEAD_ASSERT(success);

                    if (!isAnonWarc(locationInfo.fileId))
                    {
                        continue;
                    }

                    SEAD_ASSERT(j != 0);

                    nw::snd::internal::GroupItemLocationInfo prevLocationInfo;
                    success = reader.ReadGroupItemLocationInfo(&prevLocationInfo, j - 1);
                    SEAD_ASSERT(success);

                    for (auto& itemFiles : itemFileMap)
                    {
                        auto it = std::find(itemFiles.begin(), itemFiles.end(), prevLocationInfo.fileId);

                        if (it == itemFiles.end())
                        {
                            continue;
                        }

                        itemFiles.insert(std::next(it), locationInfo.fileId);
                    }
                }

                // SEAD_PRINT(" %s\n", group->getNameOrNull().cstr());
                // for (u32 j = 0; j < itemFileMap.size(); j++)
                // {
                //     SEAD_PRINT(" - %u\n", j);
                //     const auto& itemFiles = itemFileMap[j];
                //     for (u32 fileId : itemFiles)
                //     {
                //         SEAD_PRINT("   - %u\n", fileId);
                //     }
                // }

                std::vector<u32> fileIds;

                for (u32 j = 0; j < reader.GetGroupItemCount(); j++)
                {
                    nw::snd::internal::GroupItemLocationInfo locationInfo;
                    bool success = reader.ReadGroupItemLocationInfo(&locationInfo, j);
                    SEAD_ASSERT(success);

                    fileIds.push_back(locationInfo.fileId);
                }

                std::vector<u32> validSorting;
                validSorting.reserve(itemFileMap.size());

                std::vector<bool> used(itemFileMap.size(), false);

                VectorSet<u32> tmpFileIds;
                tmpFileIds.reserve(fileIds.size());

                std::function<bool(void)> backtrack;
                backtrack = [&]()
                {
                    if (validSorting.size() == itemFileMap.size())
                    {
                        return tmpFileIds == fileIds;
                    }

                    for (u32 j = 0; j < itemFileMap.size(); j++)
                    {
                        if (used[j])
                        {
                            continue;
                        }

                        used[j] = true;
                        const auto& itemFiles = itemFileMap[j];
                        const size_t initialSize = tmpFileIds.size();

                        for (u32 fileId : itemFiles)
                        {
                            tmpFileIds.insert(fileId);
                        }

                        bool matchingPrefix = tmpFileIds.size() <= fileIds.size();
                        if (matchingPrefix)
                        {
                            for (u32 k = 0; k < tmpFileIds.size(); k++)
                            {
                                if (tmpFileIds[k] != fileIds[k])
                                {
                                    matchingPrefix = false;
                                    break;
                                }
                            }
                        }

                        if (matchingPrefix)
                        {
                            validSorting.push_back(j);
                            if (backtrack())
                            {
                                return true;
                            }
                            validSorting.pop_back();
                        }

                        tmpFileIds.resize(initialSize);
                        used[j] = false;
                    }

                    return false;
                };

                bool success = backtrack();
                if (success)
                {
                    // for (u32 itemId : validSorting)
                    // {
                    //     SEAD_PRINT(" itemID - %u\n", itemId);
                    // }

                    for (u32 j : validSorting)
                    {
                        addGroupItem(j, false);
                    }
                }
                else
                {
                    for (u32 j = 0; j < reader.GetGroupItemExCount(); j++)
                    {
                        addGroupItem(j, false);
                    }

                    if (reader.GetGroupItemExCount() > 0)
                    {
                        PopupMgr::instance()->pushCurrentItemError("Failed to find real Items order");
                    }
                    else if (reader.GetGroupItemCount() > 0)
                    {
                        PopupMgr::instance()->pushCurrentItemError("Group has internal files but no Items");
                    }
                }
            }

            updateList(group->getItemInfoList());
        }
    }

    //? Remove unused WaveFiles
    {
        for (auto it = mWaveFileList.robustBegin(); it != mWaveFileList.robustEnd(); ++it)
        {
            if (it->val()->getReferences().isEmpty())
            {
                delete it->val();
            }
        }

        updateList(mWaveFileList);
    }

    // for (const Item* sound : mSoundList)
    // {
    //     SEAD_PRINT("%s\n", sound->getNameOrNull().cstr());
    // }

    return true;
}

template <typename T>
inline void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9E3779B9 + (seed << 6) + (seed >> 2);
}

struct pair_hash
{
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& v) const
    {
        std::size_t seed = 0;
        hash_combine(seed, v.first);
        hash_combine(seed, v.second);
        return seed;
    }
};

struct pair_equal
{
    template <class T1, class T2>
    bool operator()(const std::pair<T1, T2>& lhs, const std::pair<T1, T2>& rhs) const
    {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
};

void Bfsar::save_(sead::FileHandle& handle)
{
    sead::FileDeviceWriteStream stream(&handle, sead::Stream::Modes::eBinary);
    stream.setBinaryEndian(mEndian);

    FileWriter writer(&handle, &stream);
    writer.openFile("FSAR", nw::snd::internal::SoundArchiveFile::BLOCK_SIZE, mVersion);

    std::vector<std::string> strings;
    auto getStringId = [&](const sead::SafeString& str) -> u32
    {
        auto it = std::find(strings.begin(), strings.end(), str.cstr());

        if (it != strings.end())
            return it - strings.begin();
        else
            return nw::snd::internal::DEFAULT_STRING_ID;
    };

    std::unordered_map<const Item*, VectorSet<const Group*>> itemAttachedGroups;
    std::unordered_map<const WaveArchive*, VectorSet<const WaveFile*>> warcWaveFiles;
    std::unordered_map<const WaveArchive*, std::unordered_map<const WaveFile*, u32>> warcWaveFilesIndexes;

    auto delegateSoundSetWaveFiles = [&](const SoundSet* soundSet, const WaveArchive* warc)
    {
        SEAD_ASSERT(soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave);

        for (const Item::ListNode* itemNode = getItem(soundSet->getStartId(), getSoundList()); itemNode && itemNode->val()->getId() <= soundSet->getEndId(); itemNode = getSoundList().next(itemNode))
        {
            SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
            const Sound* sound = static_cast<const Sound*>(itemNode->val());

            if (sound->getSoundType() != Sound::SoundType::Wave)
            {
                continue;
            }

            const Item* waveFileItem = sound->getWaveSoundInfo().getWaveFileRef().getItem();
            SEAD_ASSERT(waveFileItem);
            SEAD_ASSERT(waveFileItem->getItemType() == Item::ItemType::WaveFile);

            const WaveFile* waveFile = static_cast<const WaveFile*>(waveFileItem);

            VectorSet<const WaveFile*>& waveFiles = warcWaveFiles[warc];

            warcWaveFilesIndexes[warc].try_emplace(waveFile, waveFiles.size());
            waveFiles.insert(waveFile);
        }
    };

    auto delegateBankWaveFiles = [&](const Bank* bank, const WaveArchive* warc)
    {
        const BankFile* bankFile = static_cast<const BankFile*>(bank->getFileRef().getItem());
        SEAD_ASSERT(bankFile);

        for (const Item* instrumentItem : bankFile->getInstrumentList())
        {
            SEAD_ASSERT(instrumentItem->getItemType() == Item::ItemType::BankFileInstrument);
            const BankFile::Instrument* instrument = static_cast<const BankFile::Instrument*>(instrumentItem);

            for (const Item* keyRegionItem : instrument->getKeyRegionList())
            {
                SEAD_ASSERT(keyRegionItem->getItemType() == Item::ItemType::BankFileKeyRegion);
                const BankFile::KeyRegion* keyRegion = static_cast<const BankFile::KeyRegion*>(keyRegionItem);

                for (const Item* velocityRegionItem : keyRegion->getVelocityRegionList())
                {
                    SEAD_ASSERT(velocityRegionItem->getItemType() == Item::ItemType::BankFileVelocityRegion);
                    const BankFile::VelocityRegion* velocityRegion = static_cast<const BankFile::VelocityRegion*>(velocityRegionItem);

                    const Item* waveFileItem = velocityRegion->getWaveFileRef().getItem();
                    SEAD_ASSERT(waveFileItem);
                    SEAD_ASSERT(waveFileItem->getItemType() == Item::ItemType::WaveFile);

                    const WaveFile* waveFile = static_cast<const WaveFile*>(waveFileItem);

                    VectorSet<const WaveFile*>& waveFiles = warcWaveFiles[warc];

                    warcWaveFilesIndexes[warc].try_emplace(waveFile, waveFiles.size());
                    waveFiles.insert(waveFile);
                }
            }
        }
    };

    auto delegateWaveFiles = [&](const Item* item, const WaveArchive* warc)
    {
        if (item->getItemType() == Item::ItemType::SoundSet)
        {
            delegateSoundSetWaveFiles(static_cast<const SoundSet*>(item), warc);
        }
        else if (item->getItemType() == Item::ItemType::Bank)
        {
            delegateBankWaveFiles(static_cast<const Bank*>(item), warc);
        }
    };

    std::unordered_map<const SoundSet*, VectorMap<const Group*, const WaveArchive*>> waveSoundSetsWarcs;
    std::unordered_map<const Bank*, VectorMap<const Group*, const WaveArchive*>> banksWarcs;

    std::unordered_map<const Sound*, u32> wsdIndexes;

    //? AssignItems
    {
        for (const Item* item : mSoundSetList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::SoundSet);
            const SoundSet* soundSet = static_cast<const SoundSet*>(item);

            if (soundSet->getSoundSetType() != SoundSet::SoundSetType::Wave)
            {
                continue;
            }

            if (soundSet->getWaveArchiveType() == WaveArchiveType::Explicit)
            {
                const WaveArchive* warc = static_cast<const WaveArchive*>(soundSet->getWaveArchiveRef().getItem());
                SEAD_ASSERT(warc);

                waveSoundSetsWarcs[soundSet].insert(nullptr, warc);
            }

            u32 i = 0;
            for (const Item::ListNode* itemNode = getItem(soundSet->getStartId(), getSoundList()); itemNode && itemNode->val()->getId() <= soundSet->getEndId(); itemNode = getSoundList().next(itemNode))
            {
                SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
                const Sound* sound = static_cast<const Sound*>(itemNode->val());

                if (sound->getSoundType() != Sound::SoundType::Wave)
                {
                    continue;
                }

                SEAD_ASSERT_MSG(!wsdIndexes.contains(sound), "Wave Sound is included in multiple Wave Sound Sets");

                wsdIndexes[sound] = i;
                i++;
            }
        }

        for (const Item* item : mBankList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);
            const Bank* bank = static_cast<const Bank*>(item);

            if (bank->getWaveArchiveType() != WaveArchiveType::Explicit)
            {
                continue;
            }

            const WaveArchive* warc = static_cast<const WaveArchive*>(bank->getWaveArchiveRef().getItem());
            SEAD_ASSERT(warc);

            banksWarcs[bank].insert(nullptr, warc);
        }
    }

    //? ResolveNames
    {
        for (const Item* item : mSoundSetList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::SoundSet);
            const SoundSet* soundSet = static_cast<const SoundSet*>(item);

            if (soundSet->getSoundSetType() != SoundSet::SoundSetType::Wave || soundSet->getWaveArchiveType() != WaveArchiveType::Explicit)
            {
                continue;
            }

            const WaveArchive* warc = static_cast<const WaveArchive*>(soundSet->getWaveArchiveRef().getItem());
            SEAD_ASSERT(warc);

            delegateWaveFiles(soundSet, warc);
        }

        for (const Item* item : mBankList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);
            const Bank* bank = static_cast<const Bank*>(item);

            if (bank->getWaveArchiveType() != WaveArchiveType::Explicit)
            {
                continue;
            }

            const WaveArchive* warc = static_cast<const WaveArchive*>(bank->getWaveArchiveRef().getItem());
            SEAD_ASSERT(warc);

            delegateWaveFiles(bank, warc);
        }

        auto attachWarcToGroup = [&](const WaveArchive* warc, u32 loadFlag, const Group* group)
        {
            if (loadFlag & Group::ItemInfo::LoadFlag::LoadWarc)
            {
                itemAttachedGroups.try_emplace(warc).first->second.insert(group);
            }
        };

        auto attachBankToGroup = [&](const Bank* bank, u32 loadFlag, const Group* group)
        {
            if (loadFlag & Group::ItemInfo::LoadFlag::LoadBank)
            {
                itemAttachedGroups.try_emplace(bank).first->second.insert(group);
            }

            if (bank->getWaveArchiveType() == WaveArchiveType::Explicit)
            {
                const WaveArchive* warc = static_cast<const WaveArchive*>(bank->getWaveArchiveRef().getItem());
                SEAD_ASSERT(warc);

                attachWarcToGroup(warc, loadFlag, group);
            }
        };

        auto attachSoundToGroup = [&](const Sound* sound, u32 loadFlag, const Group* group)
        {
            if (sound->getSoundType() == Sound::SoundType::Seq)
            {
                if (loadFlag & Group::ItemInfo::LoadFlag::LoadSeq)
                {
                    itemAttachedGroups.try_emplace(sound).first->second.insert(group);

                    SEAD_ASSERT(sound->getSequenceSoundInfo().getSequenceFileRef().isAttached());
                    const Item* seqFile = sound->getSequenceSoundInfo().getSequenceFileRef().getItem();

                    itemAttachedGroups.try_emplace(seqFile).first->second.insert(group);
                }

                for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
                {
                    const ItemReference& bankRef = sound->getSequenceSoundInfo().getBankRef(i);
                    if (!bankRef.isAttached())
                    {
                        continue;
                    }

                    const Bank* bank = static_cast<const Bank*>(bankRef.getItem());
                    attachBankToGroup(bank, loadFlag, group);
                }
            }
            else if (sound->getSoundType() == Sound::SoundType::Strm)
            {
                if (loadFlag == Group::ItemInfo::LoadFlag::LoadAll)
                {
                    itemAttachedGroups.try_emplace(sound).first->second.insert(group);
                }
            }
            else
            {
                SEAD_ASSERT(false);
            }
        };

        auto attachSoundSetToGroup = [&](const SoundSet* soundSet, u32 loadFlag, const Group* group)
        {
            if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave)
            {
                if (loadFlag & Group::ItemInfo::LoadFlag::LoadWsd)
                {
                    itemAttachedGroups.try_emplace(soundSet).first->second.insert(group);
                }

                if (soundSet->getWaveArchiveType() == WaveArchiveType::Explicit)
                {
                    const WaveArchive* warc = static_cast<const WaveArchive*>(soundSet->getWaveArchiveRef().getItem());
                    SEAD_ASSERT(warc);

                    attachWarcToGroup(warc, loadFlag, group);
                }
            }
            else if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Seq)
            {
                for (const Item::ListNode* itemNode = getItem(soundSet->getStartId(), getSoundList()); itemNode && itemNode->val()->getId() <= soundSet->getEndId(); itemNode = getSoundList().next(itemNode))
                {
                    SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
                    const Sound* sound = static_cast<const Sound*>(itemNode->val());

                    attachSoundToGroup(sound, loadFlag, group);
                }
            }
        };

        for (const Item* item : mGroupList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::Group);
            const Group* group = static_cast<const Group*>(item);

            for (const Item* groupItem : group->getItemInfoList())
            {
                SEAD_ASSERT(groupItem->getItemType() == Item::ItemType::GroupItemInfo);
                const Group::ItemInfo* itemInfo = static_cast<const Group::ItemInfo*>(groupItem);

                if (itemInfo->getIsDisabled() || !itemInfo->getItemRef().isAttached())
                {
                    continue;
                }

                if (itemInfo->getItemRefType() == Item::ItemType::Sound)
                {
                    const Sound* sound = static_cast<const Sound*>(itemInfo->getItemRef().getItem());
                    attachSoundToGroup(sound, itemInfo->getLoadFlag(), group);
                }
                else if (itemInfo->getItemRefType() == Item::ItemType::SoundSet)
                {
                    const SoundSet* soundSet = static_cast<const SoundSet*>(itemInfo->getItemRef().getItem());
                    attachSoundSetToGroup(soundSet, itemInfo->getLoadFlag(), group);
                }
                else if (itemInfo->getItemRefType() == Item::ItemType::Bank)
                {
                    const Bank* bank = static_cast<const Bank*>(itemInfo->getItemRef().getItem());
                    attachBankToGroup(bank, itemInfo->getLoadFlag(), group);
                }
                else if (itemInfo->getItemRefType() == Item::ItemType::WaveArchive)
                {
                    const WaveArchive* warc = static_cast<const WaveArchive*>(itemInfo->getItemRef().getItem());
                    attachWarcToGroup(warc, itemInfo->getLoadFlag(), group);
                }
            }
        }
    }

    std::vector<const WaveArchive*> generatedWarcs;
    std::unordered_map<const Group*, const WaveArchive*> groupTargetWarcs;

    //? GenerateItems
    {
        for (const Item* item : mGroupList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::Group);
            const Group* group = static_cast<const Group*>(item);

            bool groupHasSharedWarc = false;
            if (group->getOutputType() == Group::OutputType::Embed)
            {
                auto seqSoundHasSharedWarc = [](const Sound* sound)
                {
                    SEAD_ASSERT(sound->getSoundType() == Sound::SoundType::Seq);

                    for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
                    {
                        const ItemReference& bankRef = sound->getSequenceSoundInfo().getBankRef(i);
                        if (!bankRef.isAttached())
                        {
                            continue;
                        }

                        const Bank* bank = static_cast<const Bank*>(bankRef.getItem());

                        if (bank->getWaveArchiveType() == WaveArchiveType::AutomaticShared)
                        {
                            return true;
                        }
                    }

                    return false;
                };

                for (const Item* groupItem : group->getItemInfoList())
                {
                    SEAD_ASSERT(groupItem->getItemType() == Item::ItemType::GroupItemInfo);
                    const Group::ItemInfo* itemInfo = static_cast<const Group::ItemInfo*>(groupItem);

                    if (itemInfo->getIsDisabled() || !itemInfo->getItemRef().isAttached())
                    {
                        continue;
                    }

                    if ((itemInfo->getLoadFlag() & Group::ItemInfo::LoadFlag::LoadWsd) == 0
                     && (itemInfo->getLoadFlag() & Group::ItemInfo::LoadFlag::LoadBank) == 0)
                    {
                        continue;
                    }

                    if (itemInfo->getItemRefType() == Item::ItemType::Sound)
                    {
                        const Sound* sound = static_cast<const Sound*>(itemInfo->getItemRef().getItem());

                        if (sound->getSoundType() == Sound::SoundType::Seq)
                        {
                            if (seqSoundHasSharedWarc(sound))
                            {
                                SEAD_ASSERT(itemInfo->getLoadFlag() == Group::ItemInfo::LoadFlag::LoadAll || ((itemInfo->getLoadFlag() & Group::ItemInfo::LoadFlag::LoadBank) && (itemInfo->getLoadFlag() & Group::ItemInfo::LoadFlag::LoadWarc)));

                                groupHasSharedWarc = true;
                                break;
                            }
                        }
                    }
                    else if (itemInfo->getItemRefType() == Item::ItemType::SoundSet)
                    {
                        const SoundSet* soundSet = static_cast<const SoundSet*>(itemInfo->getItemRef().getItem());

                        if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave)
                        {
                            if (soundSet->getWaveArchiveType() == WaveArchiveType::AutomaticShared)
                            {
                                SEAD_ASSERT(itemInfo->getLoadFlag() == Group::ItemInfo::LoadFlag::LoadAll);

                                groupHasSharedWarc = true;
                                break;
                            }
                        }
                        else if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Seq)
                        {
                            for (const Item::ListNode* itemNode = getItem(soundSet->getStartId(), getSoundList()); itemNode && itemNode->val()->getId() <= soundSet->getEndId(); itemNode = getSoundList().next(itemNode))
                            {
                                SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
                                const Sound* sound = static_cast<const Sound*>(itemNode->val());

                                if (sound->getSoundType() == Sound::SoundType::Seq)
                                {
                                    if (seqSoundHasSharedWarc(sound))
                                    {
                                        SEAD_ASSERT(itemInfo->getLoadFlag() == Group::ItemInfo::LoadFlag::LoadAll || ((itemInfo->getLoadFlag() & Group::ItemInfo::LoadFlag::LoadBank) && (itemInfo->getLoadFlag() & Group::ItemInfo::LoadFlag::LoadWarc)));

                                        groupHasSharedWarc = true;
                                        break;
                                    }
                                }
                            }

                            if (groupHasSharedWarc)
                            {
                                break;
                            }
                        }
                    }
                    else if (itemInfo->getItemRefType() == Item::ItemType::Bank)
                    {
                        const Bank* bank = static_cast<const Bank*>(itemInfo->getItemRef().getItem());

                        if (bank->getWaveArchiveType() == WaveArchiveType::AutomaticShared)
                        {
                            SEAD_ASSERT(itemInfo->getLoadFlag() == Group::ItemInfo::LoadFlag::LoadAll);

                            groupHasSharedWarc = true;
                            break;
                        }
                    }
                }
            }

            if (!groupHasSharedWarc)
            {
                continue;
            }

            WaveArchive* warc = new WaveArchive();
            warc->mId = mWaveArchiveList.size();

            warc->mEnableName = false;
            warc->mName.format("%s_WaveArchive@AutoGenerated", group->getNameOrNull().cstr());

            mWaveArchiveList.pushBack(warc);

            itemAttachedGroups.try_emplace(warc).first->second.insert(group);

            groupTargetWarcs[group] = warc;

            generatedWarcs.push_back(warc);
        }

        for (const Item* item : mSoundSetList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::SoundSet);
            const SoundSet* soundSet = static_cast<const SoundSet*>(item);

            if (soundSet->getSoundSetType() != SoundSet::SoundSetType::Wave)
            {
                continue;
            }

            bool generateOriginalWarc = false;
            switch (soundSet->getWaveArchiveType())
            {
                case WaveArchiveType::AutomaticShared:
                {
                    const VectorSet<const Group*>& attachGroups = itemAttachedGroups[soundSet];
                    if (attachGroups.size() == 0)
                    {
                        generateOriginalWarc = true;
                        break;
                    }

                    for (const Group* group : attachGroups)
                    {
                        if (group->getOutputType() == Group::OutputType::Link)
                        {
                            generateOriginalWarc = true;
                            break;
                        }
                    }

                    break;
                }

                case WaveArchiveType::AutomaticIndividual:
                    generateOriginalWarc = true;
                    break;
            }

            if (generateOriginalWarc)
            {
                WaveArchive* warc = new WaveArchive();
                warc->mId = mWaveArchiveList.size();

                warc->mEnableName = false;
                warc->mName.format("%s_WaveArchive@AutoGenerated", soundSet->getNameOrNull().cstr());

                mWaveArchiveList.pushBack(warc);

                waveSoundSetsWarcs[soundSet].insert(nullptr, warc);

                delegateWaveFiles(soundSet, warc);

                generatedWarcs.push_back(warc);
            }

            if (soundSet->getWaveArchiveType() == WaveArchiveType::AutomaticShared)
            {
                const VectorSet<const Group*>& attachGroups = itemAttachedGroups[soundSet];

                for (const Group* group : attachGroups)
                {
                    if (group->getOutputType() == Group::OutputType::Embed)
                    {
                        const WaveArchive* warc = groupTargetWarcs[group];

                        waveSoundSetsWarcs[soundSet].insert(group, warc);

                        delegateWaveFiles(soundSet, warc);
                    }
                }
            }
        }

        for (const Item* item : mBankList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);
            const Bank* bank = static_cast<const Bank*>(item);

            bool generateOriginalWarc = false;
            switch (bank->getWaveArchiveType())
            {
                case WaveArchiveType::AutomaticShared:
                {
                    const VectorSet<const Group*>& attachGroups = itemAttachedGroups[bank];
                    if (attachGroups.size() == 0)
                    {
                        generateOriginalWarc = true;
                        break;
                    }

                    for (const Group* group : attachGroups)
                    {
                        if (group->getOutputType() == Group::OutputType::Link)
                        {
                            generateOriginalWarc = true;
                            break;
                        }
                    }

                    break;
                }

                case WaveArchiveType::AutomaticIndividual:
                    generateOriginalWarc = true;
                    break;
            }

            if (generateOriginalWarc)
            {
                WaveArchive* warc = new WaveArchive();
                warc->mId = mWaveArchiveList.size();

                warc->mEnableName = false;
                warc->mName.format("%s_WaveArchive@AutoGenerated", bank->getNameOrNull().cstr());

                mWaveArchiveList.pushBack(warc);

                banksWarcs[bank].insert(nullptr, warc);

                delegateWaveFiles(bank, warc);

                generatedWarcs.push_back(warc);
            }

            if (bank->getWaveArchiveType() == WaveArchiveType::AutomaticShared)
            {
                const VectorSet<const Group*>& attachGroups = itemAttachedGroups[bank];

                for (const Group* group : attachGroups)
                {
                    if (group->getOutputType() == Group::OutputType::Embed)
                    {
                        const WaveArchive* warc = groupTargetWarcs[group];

                        banksWarcs[bank].insert(group, warc);

                        delegateWaveFiles(bank, warc);
                    }
                }
            }
        }
    }

    // for (const auto& pair : warcWaveFiles)
    // {
    //     const WaveArchive* warc = pair.first;

    //     SEAD_PRINT(" %s\n", warc->getFormattedName().cstr());
    //     for (const auto& waveFile : warcWaveFiles[warc])
    //     {
    //         SEAD_PRINT(" - %s\n", waveFile->getFormattedName().cstr());
    //     }
    // }

    std::unordered_map<const Item*, File> itemFileIds;
    std::vector<File> files;
    std::unordered_map<u32, VectorSet<const Item*>> filesItems;

    std::vector<InnerFile*> generatedInnerFiles;

    //? Setup Files
    {
        std::unordered_map<std::string, File> strmFiles;

        auto addStrmSoundFile = [&](const Sound* sound)
        {
            SEAD_ASSERT(sound->getSoundType() == Sound::SoundType::Strm);

            const char* path = sound->getStreamSoundInfo().getPath().cstr();

            const auto& it = strmFiles.find(path);
            if (it == strmFiles.end())
            {
                File file(files.size(), path);

                strmFiles.try_emplace(path, file);
                itemFileIds.try_emplace(sound, file);
                files.push_back(file);

                filesItems[file.id].insert(sound);
            }
            else
            {
                const File& file = it->second;

                itemFileIds.try_emplace(sound, file);

                filesItems[file.id].insert(sound);
            }
        };

        //? Stream files are placed first
        if (mVersion <= 0x00020100)
        {
            for (const Item* item : mSoundList)
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::Sound);
                const Sound* sound = static_cast<const Sound*>(item);

                switch (sound->getSoundType())
                {
                    case Sound::SoundType::Strm:
                        addStrmSoundFile(sound);
                        break;
                }
            }
        }

        auto itemInEmbedGroup = [&](const Item* item)
        {
            const VectorSet<const Group*>& attachGroups = itemAttachedGroups[item];
            if (attachGroups.size() == 0)
            {
                return false;
            }

            for (const Group* group : attachGroups)
            {
                if (group->getOutputType() == Group::OutputType::Embed)
                {
                    return true;
                }
            }

            return false;
        };

        auto itemInLinkGroup = [&](const Item* item)
        {
            const VectorSet<const Group*>& attachGroups = itemAttachedGroups[item];
            if (attachGroups.size() == 0)
            {
                return false;
            }

            for (const Group* group : attachGroups)
            {
                if (group->getOutputType() == Group::OutputType::Link)
                {
                    return true;
                }
            }

            return false;
        };

        std::unordered_map<const SequenceFile*, File> bfseqFiles;

        //? BFSEQ
        for (const Item* item : mSoundList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::Sound);
            const Sound* sound = static_cast<const Sound*>(item);

            switch (sound->getSoundType())
            {
                case Sound::SoundType::Seq:
                {
                    const ItemReference& seqFileRef = sound->getSequenceSoundInfo().getSequenceFileRef();
                    const Item* seqFileItem = seqFileRef.getItem();
                    SEAD_ASSERT(seqFileItem);
                    SEAD_ASSERT(seqFileItem->getItemType() == Item::ItemType::SequenceFile);

                    const SequenceFile* seqFile = static_cast<const SequenceFile*>(seqFileItem);

                    const auto& it = bfseqFiles.find(seqFile);
                    if (it == bfseqFiles.end())
                    {
                        File file(files.size(), seqFile, !itemInEmbedGroup(sound) && !itemInEmbedGroup(seqFile));

                        bfseqFiles.try_emplace(seqFile, file);
                        itemFileIds.try_emplace(sound, file);
                        files.push_back(file);

                        filesItems[file.id].insert(sound);
                    }
                    else
                    {
                        const File& file = it->second;

                        itemFileIds.try_emplace(sound, file);

                        filesItems[file.id].insert(sound);
                    }

                    break;
                }
            }
        }

        std::unordered_map<const WaveArchive*, bool> includeGeneratedWarcInBfsar;

        //? BFWSD
        for (const Item* item : mSoundSetList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::SoundSet);
            const SoundSet* soundSet = static_cast<const SoundSet*>(item);

            if (soundSet->getSoundSetType() != SoundSet::SoundSetType::Wave)
            {
                continue;
            }

            const VectorSet<const Group*>& attachGroups = itemAttachedGroups[soundSet];
            bool includeInBfsar = attachGroups.size() == 0;
            if (!includeInBfsar)
            {
                switch (soundSet->getWaveArchiveType())
                {
                    case WaveArchiveType::AutomaticShared:
                        includeInBfsar = itemInLinkGroup(soundSet);

                        if (includeInBfsar)
                        {
                            const auto& itWarcs = waveSoundSetsWarcs.find(soundSet);
                            SEAD_ASSERT(itWarcs != waveSoundSetsWarcs.end());

                            const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                            SEAD_ASSERT(warcMap.size() > 0);

                            const auto& itWarc = warcMap.find(nullptr);
                            SEAD_ASSERT(warcMap.isInMap(itWarc));

                            const WaveArchive* warc = itWarc->second;
                            SEAD_ASSERT(warc);
                            SEAD_ASSERT(itemAttachedGroups[warc].size() == 0);

                            includeGeneratedWarcInBfsar[warc] = true;
                        }

                        break;

                    case WaveArchiveType::AutomaticIndividual:
                    {
                        includeInBfsar = !itemInEmbedGroup(soundSet);

                        const auto& itWarcs = waveSoundSetsWarcs.find(soundSet);
                        SEAD_ASSERT(itWarcs != waveSoundSetsWarcs.end());

                        const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                        SEAD_ASSERT(warcMap.size() == 1);

                        const auto& itWarc = warcMap.find(nullptr);
                        SEAD_ASSERT(warcMap.isInMap(itWarc));

                        const WaveArchive* warc = itWarc->second;
                        SEAD_ASSERT(warc);
                        SEAD_ASSERT(itemAttachedGroups[warc].size() == 0);

                        includeGeneratedWarcInBfsar[warc] = includeInBfsar;
                        break;
                    }

                    case WaveArchiveType::Explicit:
                        includeInBfsar = !itemInEmbedGroup(soundSet);
                        break;

                    default:
                        SEAD_ASSERT_MSG(false, "Invalid WaveArchiveType");
                }
            }
            else
            {
                switch (soundSet->getWaveArchiveType())
                {
                    case WaveArchiveType::AutomaticShared:
                    case WaveArchiveType::AutomaticIndividual:
                    {
                        const auto& itWarcs = waveSoundSetsWarcs.find(soundSet);
                        SEAD_ASSERT(itWarcs != waveSoundSetsWarcs.end());

                        const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                        SEAD_ASSERT(warcMap.size() == 1);

                        const auto& itWarc = warcMap.find(nullptr);
                        SEAD_ASSERT(warcMap.isInMap(itWarc));

                        const WaveArchive* warc = itWarc->second;
                        SEAD_ASSERT(warc);
                        SEAD_ASSERT(itemAttachedGroups[warc].size() == 0);

                        includeGeneratedWarcInBfsar[warc] = true;
                        break;
                    }

                    case WaveArchiveType::Explicit:
                        break;

                    default:
                        SEAD_ASSERT_MSG(false, "Invalid WaveArchiveType");
                }
            }

            InnerFile* innerFile = new BfwsdFile(mEndian, getVersionForBfwsd());
            generatedInnerFiles.push_back(innerFile);

            File file(files.size(), innerFile, includeInBfsar);

            itemFileIds.try_emplace(soundSet, file);
            files.push_back(file);

            filesItems[file.id].insert(soundSet);

            for (const Item::ListNode* itemNode = getItem(soundSet->getStartId(), getSoundList()); itemNode && itemNode->val()->getId() <= soundSet->getEndId(); itemNode = getSoundList().next(itemNode))
            {
                SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
                const Sound* sound = static_cast<const Sound*>(itemNode->val());

                if (sound->getSoundType() != Sound::SoundType::Wave)
                {
                    continue;
                }

                SEAD_ASSERT_MSG(!itemFileIds.contains(sound), "Wave Sound is included in multiple Wave Sound Sets");

                itemFileIds.try_emplace(sound, file);
            }
        }

        //? BFBNK
        for (const Item* item : mBankList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);
            const Bank* bank = static_cast<const Bank*>(item);

            const Item* bankFileItem = bank->getFileRef().getItem();
            SEAD_ASSERT(bankFileItem);
            SEAD_ASSERT(bankFileItem->getItemType() == Item::ItemType::BankFile);

            const BankFile* bankFile = static_cast<const BankFile*>(bankFileItem);

            const VectorSet<const Group*>& attachGroups = itemAttachedGroups[bank];
            bool includeInBfsar = attachGroups.size() == 0;
            if (!includeInBfsar)
            {
                switch (bank->getWaveArchiveType())
                {
                    case WaveArchiveType::AutomaticShared:
                        includeInBfsar = itemInLinkGroup(bank);

                        if (includeInBfsar)
                        {
                            const auto& itWarcs = banksWarcs.find(bank);
                            SEAD_ASSERT(itWarcs != banksWarcs.end());

                            const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                            SEAD_ASSERT(warcMap.size() > 0);

                            const auto& itWarc = warcMap.find(nullptr);
                            SEAD_ASSERT(warcMap.isInMap(itWarc));

                            const WaveArchive* warc = itWarc->second;
                            SEAD_ASSERT(warc);
                            SEAD_ASSERT(itemAttachedGroups[warc].size() == 0);

                            includeGeneratedWarcInBfsar[warc] = true;
                        }

                        break;

                    case WaveArchiveType::AutomaticIndividual:
                    {
                        includeInBfsar = !itemInEmbedGroup(bank);

                        const auto& itWarcs = banksWarcs.find(bank);
                        SEAD_ASSERT(itWarcs != banksWarcs.end());

                        const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                        SEAD_ASSERT(warcMap.size() == 1);

                        const auto& itWarc = warcMap.find(nullptr);
                        SEAD_ASSERT(warcMap.isInMap(itWarc));

                        const WaveArchive* warc = itWarc->second;
                        SEAD_ASSERT(warc);
                        SEAD_ASSERT(itemAttachedGroups[warc].size() == 0);

                        includeGeneratedWarcInBfsar[warc] = includeInBfsar;
                        break;
                    }

                    case WaveArchiveType::Explicit:
                        includeInBfsar = !itemInEmbedGroup(bank);
                        break;

                    default:
                        SEAD_ASSERT_MSG(false, "Invalid WaveArchiveType");
                }
            }
            else
            {
                switch (bank->getWaveArchiveType())
                {
                    case WaveArchiveType::AutomaticShared:
                    case WaveArchiveType::AutomaticIndividual:
                    {
                        const auto& itWarcs = banksWarcs.find(bank);
                        SEAD_ASSERT(itWarcs != banksWarcs.end());

                        const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                        SEAD_ASSERT(warcMap.size() == 1);

                        const auto& itWarc = warcMap.find(nullptr);
                        SEAD_ASSERT(warcMap.isInMap(itWarc));

                        const WaveArchive* warc = itWarc->second;
                        SEAD_ASSERT(warc);
                        SEAD_ASSERT(itemAttachedGroups[warc].size() == 0);

                        includeGeneratedWarcInBfsar[warc] = true;
                        break;
                    }

                    case WaveArchiveType::Explicit:
                        break;

                    default:
                        SEAD_ASSERT_MSG(false, "Invalid WaveArchiveType");
                }
            }

            bankFile->setup(mEndian, getVersionForBfbnk());

            File file(files.size(), bankFile, includeInBfsar);

            itemFileIds.try_emplace(bank, file);
            files.push_back(file);

            filesItems[file.id].insert(bank);
        }

        //? BFWAR
        for (const Item* item : mWaveArchiveList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::WaveArchive);
            const WaveArchive* warc = static_cast<const WaveArchive*>(item);

            if (warc->getName().endsWith("@AutoGenerated"))
            {
                continue;
            }

            InnerFile* innerFile = new BfwarFile(mEndian, getVersionForBfwar(), warcWaveFiles[warc]);
            generatedInnerFiles.push_back(innerFile);

            File file(files.size(), innerFile, !itemInEmbedGroup(warc));

            itemFileIds.try_emplace(warc, file);
            files.push_back(file);

            filesItems[file.id].insert(warc);
        }

        //? BFGRP
        for (const Item* item : mGroupList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::Group);
            const Group* group = static_cast<const Group*>(item);

            InnerFile* innerFile = new BfgrpFile(mEndian, getVersionForBfgrp());
            generatedInnerFiles.push_back(innerFile);

            File file(files.size(), innerFile, true);

            itemFileIds.try_emplace(group, file);
            files.push_back(file);

            filesItems[file.id].insert(group);
        }

        //? Other BFWAR
        for (const WaveArchive* warc : generatedWarcs)
        {
            const VectorSet<const Group*>& attachGroups = itemAttachedGroups[warc];
            bool includeInBfsar = false;
            if (attachGroups.size() == 0)
            {
                includeInBfsar = includeGeneratedWarcInBfsar[warc];
            }

            InnerFile* innerFile = new BfwarFile(mEndian, getVersionForBfwar(), warcWaveFiles[warc]);
            generatedInnerFiles.push_back(innerFile);

            File file(files.size(), innerFile, includeInBfsar);

            itemFileIds.try_emplace(warc, file);
            files.push_back(file);

            filesItems[file.id].insert(warc);
        }

        //? Stream files are placed last
        if (mVersion > 0x00020100)
        {
            for (const Item* item : mSoundList)
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::Sound);
                const Sound* sound = static_cast<const Sound*>(item);

                switch (sound->getSoundType())
                {
                    case Sound::SoundType::Strm:
                        addStrmSoundFile(sound);
                        break;
                }
            }
        }
    }

    //std::unordered_map<const Group*, VectorSet<const File*>> groupItemFiles;
    std::unordered_map<const Group*, VectorSet<u32>> groupItemFiles;

    //? OptimizeFiles
    {
        auto attachWarcFileToGroup = [&](const WaveArchive* warc, u32 loadFlag, const Group* group)
        {
            if (loadFlag & Group::ItemInfo::LoadFlag::LoadWarc)
            {
                const auto& it = itemFileIds.find(warc);
                SEAD_ASSERT(it != itemFileIds.end());

                groupItemFiles[group].insert(it->second.id);
            }
        };

        auto attachBankFileToGroup = [&](const Bank* bank, u32 loadFlag, const Group* group)
        {
            if (loadFlag & Group::ItemInfo::LoadFlag::LoadBank)
            {
                const auto& it = itemFileIds.find(bank);
                SEAD_ASSERT(it != itemFileIds.end());

                groupItemFiles[group].insert(it->second.id);
            }

            const WaveArchive* warc = nullptr;
            if (bank->getWaveArchiveType() == WaveArchiveType::Explicit)
            {
                warc = static_cast<const WaveArchive*>(bank->getWaveArchiveRef().getItem());
            }
            else
            {
                const VectorMap<const Group*, const WaveArchive*>& warcMap = banksWarcs[bank];
                SEAD_ASSERT(warcMap.size() > 0);

                auto it = warcMap.find(group);
                if (warcMap.isInMap(it))
                {
                    warc = it->second;
                }
                else
                {
                    it = warcMap.find(nullptr);
                    if (warcMap.isInMap(it))
                    {
                        warc = it->second;
                    }
                }
            }

            if (warc)
            {
                attachWarcFileToGroup(warc, loadFlag, group);
            }
        };

        auto attachSoundFileToGroup = [&](const Sound* sound, u32 loadFlag, const Group* group)
        {
            if (sound->getSoundType() == Sound::SoundType::Seq)
            {
                if (loadFlag & Group::ItemInfo::LoadFlag::LoadSeq)
                {
                    const auto& it = itemFileIds.find(sound);
                    SEAD_ASSERT(it != itemFileIds.end());

                    groupItemFiles[group].insert(it->second.id);
                }

                for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
                {
                    const ItemReference& bankRef = sound->getSequenceSoundInfo().getBankRef(i);
                    if (!bankRef.isAttached())
                    {
                        continue;
                    }

                    const Bank* bank = static_cast<const Bank*>(bankRef.getItem());
                    attachBankFileToGroup(bank, loadFlag, group);
                }
            }
            else if (sound->getSoundType() == Sound::SoundType::Strm)
            {
                if (loadFlag == Group::ItemInfo::LoadFlag::LoadAll)
                {
                    const auto& it = itemFileIds.find(sound);
                    SEAD_ASSERT(it != itemFileIds.end());

                    groupItemFiles[group].insert(it->second.id);
                }
            }
            else
            {
                SEAD_ASSERT(false);
            }
        };

        auto attachSoundSetFileToGroup = [&](const SoundSet* soundSet, u32 loadFlag, const Group* group)
        {
            if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave)
            {
                if (loadFlag & Group::ItemInfo::LoadFlag::LoadWsd)
                {
                    const auto& it = itemFileIds.find(soundSet);
                    SEAD_ASSERT(it != itemFileIds.end());

                    groupItemFiles[group].insert(it->second.id);
                }

                const WaveArchive* warc = nullptr;
                if (soundSet->getWaveArchiveType() == WaveArchiveType::Explicit)
                {
                    warc = static_cast<const WaveArchive*>(soundSet->getWaveArchiveRef().getItem());
                }
                else
                {
                    const VectorMap<const Group*, const WaveArchive*>& warcMap = waveSoundSetsWarcs[soundSet];
                    SEAD_ASSERT(warcMap.size() > 0);

                    auto it = warcMap.find(group);
                    if (warcMap.isInMap(it))
                    {
                        warc = it->second;
                    }
                    else
                    {
                        it = warcMap.find(nullptr);
                        if (warcMap.isInMap(it))
                        {
                            warc = it->second;
                        }
                    }
                }

                if (warc)
                {
                    attachWarcFileToGroup(warc, loadFlag, group);
                }
            }
            else if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Seq)
            {
                for (const Item::ListNode* itemNode = getItem(soundSet->getStartId(), getSoundList()); itemNode && itemNode->val()->getId() <= soundSet->getEndId(); itemNode = getSoundList().next(itemNode))
                {
                    SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
                    const Sound* sound = static_cast<const Sound*>(itemNode->val());

                    attachSoundFileToGroup(sound, loadFlag, group);
                }
            }
            else
            {
                SEAD_ASSERT(false);
            }
        };

        for (const Item* item : mGroupList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::Group);
            const Group* group = static_cast<const Group*>(item);

            for (const Item* groupItem : group->getItemInfoList())
            {
                SEAD_ASSERT(groupItem->getItemType() == Item::ItemType::GroupItemInfo);
                const Group::ItemInfo* itemInfo = static_cast<const Group::ItemInfo*>(groupItem);

                if (itemInfo->getIsDisabled() || !itemInfo->getItemRef().isAttached())
                {
                    continue;
                }

                if (itemInfo->getItemRefType() == Item::ItemType::Sound)
                {
                    const Sound* sound = static_cast<const Sound*>(itemInfo->getItemRef().getItem());
                    attachSoundFileToGroup(sound, itemInfo->getLoadFlag(), group);
                }
                else if (itemInfo->getItemRefType() == Item::ItemType::SoundSet)
                {
                    const SoundSet* soundSet = static_cast<const SoundSet*>(itemInfo->getItemRef().getItem());
                    attachSoundSetFileToGroup(soundSet, itemInfo->getLoadFlag(), group);
                }
                else if (itemInfo->getItemRefType() == Item::ItemType::Bank)
                {
                    const Bank* bank = static_cast<const Bank*>(itemInfo->getItemRef().getItem());
                    attachBankFileToGroup(bank, itemInfo->getLoadFlag(), group);
                }
                else if (itemInfo->getItemRefType() == Item::ItemType::WaveArchive)
                {
                    const WaveArchive* warc = static_cast<const WaveArchive*>(itemInfo->getItemRef().getItem());
                    attachWarcFileToGroup(warc, itemInfo->getLoadFlag(), group);
                }
            }

            const auto& it = groupTargetWarcs.find(group);
            if (it != groupTargetWarcs.end())
            {
                const WaveArchive* warc = it->second;
                SEAD_ASSERT(warc);

                attachWarcFileToGroup(warc, Group::ItemInfo::LoadFlag::LoadAll, group);
            }
            else
            {
                // TODO: AddGeneratedWaveArchives - is this needed ??
            }
        }
    }

    //? Add strings because even if the BFSAR don't include the String Block, items still have a string id...
    {
        auto addListNames = [&](const Item::List& list)
        {
            for (const Item* item : list)
            {
                if (!item->isNameValid())
                {
                    continue;
                }

                if (getStringId(item->getName()) != nw::snd::internal::DEFAULT_STRING_ID)
                {
                    SEAD_ASSERT_MSG(false, "name(%s) is duped", item->getName().cstr());
                    continue;
                }

                strings.push_back(item->getName().cstr());
            }
        };

        addListNames(mSoundList);
        addListNames(mSoundSetList);
        addListNames(mBankList);
        addListNames(mWaveArchiveList);
        addListNames(mGroupList);
        addListNames(mPlayerList);
    }

    //? String Block
    if (mIncludeStringTable)
    {
        writer.openBlock(nw::snd::internal::ElementType_SoundArchiveFile_StringBlock, "STRG");

        writer.openReference("StringTable");
        writer.openReference("PatriciaTree");

        writer.closeReference("StringTable", nw::snd::internal::ElementType_SoundArchiveFile_StringTable);
        writer.pushOffsetBase();
        {
            stream.writeU32(strings.size());

            for (u32 i = 0; i < strings.size(); i++)
            {
                writer.openReference(sead::FormatFixedSafeString<64>("StringEntry%u", i).cstr());

                stream.writeU32(strings[i].size() + 1);
            }

            for (u32 i = 0; i < strings.size(); i++)
            {
                writer.closeReference(sead::FormatFixedSafeString<64>("StringEntry%u", i).cstr(), nw::snd::internal::ElementType_General_String);

                stream.writeString(strings[i].c_str(), strings[i].size() + 1);
            }

            writer.align(4);
        }
        writer.popOffsetBase();

        writer.closeReference("PatriciaTree", nw::snd::internal::ElementType_SoundArchiveFile_PatriciaTree);

        PatriciaTree tree;

        auto addListTree = [&](const Item::List& list, nw::snd::internal::ItemType type)
        {
            for (const Item* item : list)
            {
                if (item->isNameValid())
                {
                    SEAD_ASSERT(getStringId(item->getName()) != nw::snd::internal::DEFAULT_STRING_ID);

                    tree.insert(
                        item->getName().cstr(),
                        PatriciaTree::NodeData(getStringId(item->getName()), nw::snd::internal::Util::GetMaskedItemId(item->getId(), type))
                    );
                }
            }
        };

        addListTree(mSoundList, nw::snd::internal::ItemType_Sound);
        addListTree(mSoundSetList, nw::snd::internal::ItemType_SoundGroup);
        addListTree(mBankList, nw::snd::internal::ItemType_Bank);
        addListTree(mPlayerList, nw::snd::internal::ItemType_Player);
        addListTree(mGroupList, nw::snd::internal::ItemType_Group);
        addListTree(mWaveArchiveList, nw::snd::internal::ItemType_WaveArchive);

        stream.writeU32(tree.rootIdx);
        stream.writeU32(tree.nodeTable.size());

        for (u32 i = 0; i < tree.nodeTable.size(); i++)
        {
            const PatriciaTree::Node* node = &tree.nodeTable[i];
            const PatriciaTree::NodeData* data = &node->nodeData;

            stream.writeU16(node->flags);
            stream.writeU16(node->flags & PatriciaTree::Node::FLAG_LEAF ? 0xFFFF : node->bit);
            stream.writeU32(node->getLeftIdx());
            stream.writeU32(node->getRightIdx());

            stream.writeU32(data->stringId);
            stream.writeU32(data->itemId);
        }

        writer.align(0x20);
        writer.closeBlock();
    }
    else
    {
        writer.nullBlock(nw::snd::internal::ElementType_SoundArchiveFile_StringBlock);
    }

    //? Info Block
    {
        writer.openBlock(nw::snd::internal::ElementType_SoundArchiveFile_InfoBlock, "INFO");

        writer.openReference("SoundInfoSection");
        writer.openReference("SoundGroupInfoSection");
        writer.openReference("BankInfoSection");
        writer.openReference("WaveArchiveInfoSection");
        writer.openReference("GroupInfoSection");
        writer.openReference("PlayerInfoSection");
        writer.openReference("FileInfoSection");
        writer.openReference("SoundArchivePlayerInfo");

        writer.closeReference("SoundInfoSection", nw::snd::internal::ElementType_SoundArchiveFile_SoundInfoSection);
        writer.pushOffsetBase();
        {
            writer.openReferenceTable("SoundInfoSectionTable", mSoundList.size());

            for (const Item* item : mSoundList)
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::Sound);
                const Sound* sound = static_cast<const Sound*>(item);

                writer.addReferenceTableReference("SoundInfoSectionTable", nw::snd::internal::ElementType_SoundArchiveFile_SoundInfo);

                writer.pushOffsetBase();
                {
                    u32 pos = writer.getPosition();

                    stream.writeU32(itemFileIds[sound].id);

                    stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(sound->getPlayerId(), nw::snd::internal::ItemType_Player));
                    stream.writeU8(sound->getVolume());
                    stream.writeU8(sound->getRemoteFilter());
                    stream.writeU16(0); // Padding1

                    writer.openReference("DetailSoundInfo");

                    {
                        u32 sound3DInfoParamOffset = writer.getPosition() + 4;

                        std::unordered_map<u32, u32> flags;
                        if (sound->isEnableName())
                        {
                            flags[nw::snd::internal::SOUND_INFO_STRING_ID] = getStringId(sound->getName());

                            sound3DInfoParamOffset += 4;
                        }

                        if (sound->isEnablePanParam())
                        {
                            u32 flag = (u32)sound->getPanMode() | ((u32)sound->getPanCurve() << 8);
                            flags[nw::snd::internal::SOUND_INFO_PAN_PARAM] = flag;

                            sound3DInfoParamOffset += 4;
                        }

                        if (sound->isEnablePlayerParam())
                        {
                            u32 flag = sound->getPlayerPriority() | (sound->getActorPlayerId() << 8);
                            flags[nw::snd::internal::SOUND_INFO_PLAYER_PARAM] = flag;

                            sound3DInfoParamOffset += 4;
                        }

                        if (sound->isEnableSound3DInfo())
                        {
                            flags[nw::snd::internal::SOUND_INFO_OFFSET_TO_3D_PARAM] = 0;
                        }

                        if (sound->isEnableIsFrontBypass() && sound->getSoundType() != Sound::SoundType::Strm)
                        {
                            flags[nw::snd::internal::SOUND_INFO_OFFSET_TO_CTR_PARAM] = sound->getIsFrontBypass();
                        }

                        if (sound->isEnableUserParam(0))
                        {
                            flags[nw::snd::internal::SOUND_INFO_USER_PARAM] = sound->getUserParam(0);
                        }

                        if (sound->isEnableUserParam(1))
                        {
                            flags[nw::snd::internal::SOUND_INFO_USER_PARAM1] = sound->getUserParam(1);
                        }

                        if (sound->isEnableUserParam(2))
                        {
                            flags[nw::snd::internal::SOUND_INFO_USER_PARAM2] = sound->getUserParam(2);
                        }

                        if (sound->isEnableUserParam(3))
                        {
                            flags[nw::snd::internal::SOUND_INFO_USER_PARAM3] = sound->getUserParam(3);
                        }

                        FlagParameters params(flags);
                        params.write(writer);

                        if (sound->isEnableSound3DInfo())
                        {
                            u32 sound3DInfoPos = writer.getPosition();
                            writer.seek(sound3DInfoParamOffset);
                            stream.writeU32(sound3DInfoPos - pos);
                            writer.seek(sound3DInfoPos);

                            const Sound::Sound3DInfo& sound3DInfo = sound->getSound3DInfo();

                            stream.writeU32(sound3DInfo.getFlags());
                            stream.writeF32(sound3DInfo.getDecayRatio());
                            stream.writeU8(sound3DInfo.getDecayCurve());
                            stream.writeU8(sound3DInfo.getDopplerFactor());
                            stream.writeU16(0); // Padding
                            stream.writeU32(0); // Option Parameter
                        }
                    }

                    if (sound->getSoundType() == Sound::SoundType::Wave || sound->getSoundType() == Sound::SoundType::Seq)
                    {
                        stream.writeU32(sound->getIsFrontBypass()); // BRUH
                    }

                    writer.pushOffsetBase();
                    {
                        switch (sound->getSoundType())
                        {
                            case Sound::SoundType::Seq:
                            {
                                writer.closeReference("DetailSoundInfo", nw::snd::internal::ElementType_SoundArchiveFile_SequenceSoundInfo);

                                const Sound::SequenceSoundInfo& seqInfo = sound->getSequenceSoundInfo();

                                writer.openReference("BankTable");
                                stream.writeU32(seqInfo.getAllocateTrackFlags());

                                std::unordered_map<u32, u32> flags;
                                if (seqInfo.isEnableStartOffset())
                                {
                                    flags[nw::snd::internal::SEQ_SOUND_INFO_START_OFFSET] = seqInfo.getStartOffset();
                                }

                                if (seqInfo.isEnablePriority())
                                {
                                    u32 flag = seqInfo.getChannelPriority() | (seqInfo.getIsReleasePriorityFix() << 8);
                                    flags[nw::snd::internal::SEQ_SOUND_INFO_PRIORITY] = flag;
                                }

                                FlagParameters params(flags);
                                params.write(writer);

                                writer.closeReference("BankTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

                                u32 numBanks = nw::snd::SoundArchive::SEQ_BANK_MAX;
                                for (s32 i = numBanks - 1; i >= 0; i--)
                                {
                                    if (seqInfo.getBankId(i) == nw::snd::SoundArchive::INVALID_ID)
                                        numBanks--;
                                    else
                                        break;
                                }

                                stream.writeU32(numBanks);

                                for (u32 i = 0; i < numBanks; i++)
                                {
                                    stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(seqInfo.getBankId(i), nw::snd::internal::ItemType_Bank));
                                }
                                break;
                            }

                            case Sound::SoundType::Strm:
                            {
                                writer.closeReference("DetailSoundInfo", nw::snd::internal::ElementType_SoundArchiveFile_StreamSoundInfo);

                                const Sound::StreamSoundInfo& strmInfo = sound->getStreamSoundInfo();

                                stream.writeU16(strmInfo.getAllocateTrackFlags());
                                stream.writeU16(strmInfo.getAllocateChannelCount());

                                if (isStreamTrackInfoAvailable())
                                {
                                    writer.openReference("TrackInfoTableRef");

                                    if (isStreamSendAvailable())
                                    {
                                        stream.writeF32(strmInfo.getPitch());

                                        writer.openReference("SendValue");

                                        if (isStreamPrefetchAvailable())
                                        {
                                            writer.openReference("StreamSoundExtension");
                                            stream.writeU32(strmInfo.getPrefetchFileId());
                                        }
                                    }
                                    else
                                    {
                                        stream.writeU32(0); //! Unknown
                                    }

                                    if (strmInfo.getTrackList().size() > 0)
                                    {
                                        writer.closeReference("TrackInfoTableRef", nw::snd::internal::ElementType_Table_ReferenceTable);
                                        writer.pushOffsetBase();
                                        {
                                            writer.openReferenceTable("TrackInfoTable", strmInfo.getTrackList().size());

                                            u32 channelIdxStart = 0;
                                            for (u32 i = 0; i < strmInfo.getTrackList().size(); i++)
                                            {
                                                const Item* trackItem = strmInfo.getTrackList().nth(i)->val();
                                                const Sound::StreamSoundInfo::Track& track = *static_cast<const Sound::StreamSoundInfo::Track*>(trackItem);

                                                writer.addReferenceTableReference("TrackInfoTable", nw::snd::internal::ElementType_SoundArchiveFile_StreamSoundTrackInfo);

                                                writer.pushOffsetBase();
                                                {
                                                    stream.writeU8(track.getVolume());
                                                    stream.writeU8(track.getPan());
                                                    stream.writeU8(track.getSPan());
                                                    stream.writeU8(track.getFlags());

                                                    writer.openReference("GlobalChannelIndexTable");

                                                    if (isStreamSendAvailable())
                                                    {
                                                        writer.openReference("TrackSendValue");
                                                    }

                                                    if (isFilterSupportedVersion())
                                                    {
                                                        stream.writeU8(track.getLpfFreq());
                                                        stream.writeU8(track.getBiquadType());
                                                        stream.writeU8(track.getBiquadValue());
                                                        stream.writeU8(0); // Padding
                                                    }

                                                    writer.closeReference("GlobalChannelIndexTable", nw::snd::internal::ElementType_Table_EmbeddingTable);
                                                    stream.writeU32(track.getChannelCount());

                                                    for (u32 j = 0; j < track.getChannelCount(); j++)
                                                    {
                                                        stream.writeU8(channelIdxStart + j);
                                                    }

                                                    writer.align(0x4);

                                                    if (isStreamSendAvailable())
                                                    {
                                                        writer.closeReference("TrackSendValue", nw::snd::internal::ElementType_SoundArchiveFile_SendInfo);

                                                        stream.writeU8(track.getMainSend());

                                                        for (u32 j = 0; j < nw::snd::AUX_BUS_NUM; j++)
                                                        {
                                                            stream.writeU8(track.getFxSend(j));
                                                        }

                                                        writer.align(0x4);
                                                        stream.writeU32(0); //! Unknown
                                                    }
                                                }
                                                writer.popOffsetBase();

                                                channelIdxStart += track.getChannelCount();
                                            }

                                            writer.closeReferenceTable("TrackInfoTable");
                                        }
                                        writer.popOffsetBase();
                                    }
                                    else
                                    {
                                        writer.closeNullReference("TrackInfoTableRef");
                                    }

                                    if (isStreamSendAvailable())
                                    {
                                        writer.closeReference("SendValue", nw::snd::internal::ElementType_SoundArchiveFile_SendInfo);

                                        stream.writeU8(strmInfo.getMainSend());

                                        for (u32 j = 0; j < nw::snd::AUX_BUS_NUM; j++)
                                        {
                                            stream.writeU8(strmInfo.getFxSend(j));
                                        }

                                        writer.align(0x4);
                                        stream.writeU32(0); //! Unknown

                                        if (isStreamPrefetchAvailable())
                                        {
                                            if (strmInfo.isEnableStreamSoundExtension())
                                            {
                                                writer.closeReference("StreamSoundExtension", nw::snd::internal::ElementType_SoundArchiveFile_StreamSoundExtensionInfo);

                                                u32 streamTypeInfo = strmInfo.getStreamType() | (strmInfo.getIsLoop() << 8);
                                                stream.writeU32(streamTypeInfo);

                                                stream.writeU32(strmInfo.getLoopStartFrame());
                                                stream.writeU32(strmInfo.getLoopEndFrame());
                                            }
                                            else
                                            {
                                                writer.closeNullReference("StreamSoundExtension");
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    stream.writeU32(0); //! Unknown if this exists
                                }
                                break;
                            }

                            case Sound::SoundType::Wave:
                            {
                                writer.closeReference("DetailSoundInfo", nw::snd::internal::ElementType_SoundArchiveFile_WaveSoundInfo);

                                const Sound::WaveSoundInfo& waveInfo = sound->getWaveSoundInfo();

                                const auto& it = wsdIndexes.find(sound);
                                SEAD_ASSERT(it != wsdIndexes.end());

                                stream.writeU32(it->second);

                                SEAD_ASSERT(waveInfo.getAllocateTrackCount() == 1);
                                stream.writeU32(waveInfo.getAllocateTrackCount());

                                std::unordered_map<u32, u32> flags;
                                if (waveInfo.isEnablePriority())
                                {
                                    u32 flag = waveInfo.getChannelPriority() | (waveInfo.getIsReleasePriorityFix() << 8);
                                    flags[nw::snd::internal::WAVE_SOUND_INFO_PRIORITY] = flag;
                                }

                                FlagParameters params(flags);
                                params.write(writer);
                                break;
                            }

                            default:
                                SEAD_ASSERT_MSG(false, "invalid SoundType");
                                break;
                        }
                    }
                    writer.popOffsetBase();
                }
                writer.popOffsetBase();
            }

            writer.closeReferenceTable("SoundInfoSectionTable");
        }
        writer.popOffsetBase();

        writer.closeReference("SoundGroupInfoSection", nw::snd::internal::ElementType_SoundArchiveFile_SoundGroupInfoSection);
        writer.pushOffsetBase();
        {
            writer.openReferenceTable("SoundGroupInfoSectionTable", mSoundSetList.size());

            for (const Item* item : mSoundSetList)
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::SoundSet);
                const SoundSet* soundSet = static_cast<const SoundSet*>(item);

                writer.addReferenceTableReference("SoundGroupInfoSectionTable", nw::snd::internal::ElementType_SoundArchiveFile_SoundGroupInfo);

                writer.pushOffsetBase();
                {
                    if (soundSet->getIsEmpty())
                    {
                        stream.writeU32(nw::snd::SoundArchive::INVALID_ID);
                        stream.writeU32(nw::snd::SoundArchive::INVALID_ID);
                    }
                    else
                    {
                        stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(soundSet->getStartId(), nw::snd::internal::ItemType_Sound));
                        stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(soundSet->getEndId(), nw::snd::internal::ItemType_Sound));
                    }

                    writer.openReference("FileIdTable");
                    writer.openReference("DetailSoundGroup");

                    std::unordered_map<u32, u32> flags;
                    if (soundSet->isEnableName())
                    {
                        flags[nw::snd::internal::SOUND_GROUP_INFO_STRING_ID] = getStringId(soundSet->getName());
                    }

                    FlagParameters params(flags);
                    params.write(writer);

                    writer.closeReference("FileIdTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

                    if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave)
                    {
                        stream.writeU32(1); // FileIdTable count
                        stream.writeU32(itemFileIds[soundSet].id); // FileIdTable[0]

                        writer.closeReference("DetailSoundGroup", nw::snd::internal::ElementType_SoundArchiveFile_WaveSoundGroupInfo);

                        writer.pushOffsetBase();
                        {
                            writer.openReference("WaveArchiveIdTable");

                            stream.writeU32(0); // Option Parameter

                            writer.closeReference("WaveArchiveIdTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

                            if (soundSet->getWaveArchiveType() != WaveArchiveType::AutomaticShared)
                            {
                                const auto& warcs = waveSoundSetsWarcs[soundSet];

                                stream.writeU32(warcs.size()); // WaveArchiveIdTable count

                                for (const WaveArchive* warc : warcs)
                                {
                                    stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(warc->getId(), nw::snd::internal::ItemType_WaveArchive)); // WaveArchiveIdTable entries
                                }
                            }
                            else
                            {
                                stream.writeU32(0); // WaveArchiveIdTable count
                            }
                        }
                        writer.popOffsetBase();
                    }
                    else
                    {
                        VectorSet<u32> filesIds;

                        for (const Item::ListNode* itemNode = getItem(soundSet->getStartId(), getSoundList()); itemNode && itemNode->val()->getId() <= soundSet->getEndId(); itemNode = getSoundList().next(itemNode))
                        {
                            SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
                            const Sound* sound = static_cast<const Sound*>(itemNode->val());

                            if (sound->getSoundType() != Sound::SoundType::Seq)
                            {
                                continue;
                            }

                            filesIds.insert(itemFileIds[sound].id);
                        }

                        stream.writeU32(filesIds.size()); // FileIdTable count

                        for (u32 fileId : filesIds)
                        {
                            stream.writeU32(fileId); // FileIdTable entries
                        }

                        writer.closeNullReference("DetailSoundGroup");
                    }
                }
                writer.popOffsetBase();
            }

            writer.closeReferenceTable("SoundGroupInfoSectionTable");
        }
        writer.popOffsetBase();

        writer.closeReference("BankInfoSection", nw::snd::internal::ElementType_SoundArchiveFile_BankInfoSection);
        writer.pushOffsetBase();
        {
            writer.openReferenceTable("BankInfoSectionTable", mBankList.size());

            for (const Item* item : mBankList)
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);
                const Bank* bank = static_cast<const Bank*>(item);

                writer.addReferenceTableReference("BankInfoSectionTable", nw::snd::internal::ElementType_SoundArchiveFile_BankInfo);

                writer.pushOffsetBase();
                {
                    stream.writeU32(itemFileIds[bank].id);

                    writer.openReference("WaveArchiveIdTable");

                    std::unordered_map<u32, u32> flags;
                    if (bank->isEnableName())
                    {
                        flags[nw::snd::internal::BANK_INFO_STRING_ID] = getStringId(bank->getName());
                    }

                    FlagParameters params(flags);
                    params.write(writer);

                    writer.closeReference("WaveArchiveIdTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

                    if (bank->getWaveArchiveType() != WaveArchiveType::AutomaticShared)
                    {
                        const auto& warcs = banksWarcs[bank];

                        stream.writeU32(warcs.size()); // WaveArchiveIdTable count

                        for (const WaveArchive* warc : warcs)
                        {
                            stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(warc->getId(), nw::snd::internal::ItemType_WaveArchive)); // WaveArchiveIdTable entries
                        }
                    }
                    else
                    {
                        stream.writeU32(0); // WaveArchiveIdTable count
                    }
                }
                writer.popOffsetBase();
            }

            writer.closeReferenceTable("BankInfoSectionTable");
        }
        writer.popOffsetBase();

        writer.closeReference("WaveArchiveInfoSection", nw::snd::internal::ElementType_SoundArchiveFile_WaveArchiveInfoSection);
        writer.pushOffsetBase();
        {
            writer.openReferenceTable("WaveArchiveInfoSectionTable", mWaveArchiveList.size());

            for (const Item* item : mWaveArchiveList)
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::WaveArchive);
                const WaveArchive* warc = static_cast<const WaveArchive*>(item);

                writer.addReferenceTableReference("WaveArchiveInfoSectionTable", nw::snd::internal::ElementType_SoundArchiveFile_WaveArchiveInfo);

                stream.writeU32(itemFileIds[warc].id);
                stream.writeU8(warc->getIsLoadIndividual());
                stream.writeU8(0); // Padding1
                stream.writeU16(0); // Padding2

                std::unordered_map<u32, u32> flags;
                if (warc->isEnableName())
                {
                    flags[nw::snd::internal::WAVE_ARCHIVE_INFO_STRING_ID] = getStringId(warc->getName());
                }

                if (warc->getIsLoadIndividual())
                {
                    u32 waveCount = warcWaveFiles[warc].size();

                    flags[nw::snd::internal::WAVE_ARCHIVE_INFO_WAVE_COUNT] = waveCount;
                }

                FlagParameters params(flags);
                params.write(writer);
            }

            writer.closeReferenceTable("WaveArchiveInfoSectionTable");
        }
        writer.popOffsetBase();

        writer.closeReference("GroupInfoSection", nw::snd::internal::ElementType_SoundArchiveFile_GroupInfoSection);
        writer.pushOffsetBase();
        {
            writer.openReferenceTable("GroupInfoSectionTable", mGroupList.size());

            for (const Item* item : mGroupList)
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::Group);
                const Group* group = static_cast<const Group*>(item);

                writer.addReferenceTableReference("GroupInfoSectionTable", nw::snd::internal::ElementType_SoundArchiveFile_GroupInfo);

                stream.writeU32(itemFileIds[group].id);

                std::unordered_map<u32, u32> flags;
                if (group->isEnableName())
                {
                    flags[nw::snd::internal::GROUP_INFO_STRING_ID] = getStringId(group->getName());
                }

                FlagParameters params(flags);
                params.write(writer);
            }

            writer.closeReferenceTable("GroupInfoSectionTable");
        }
        writer.popOffsetBase();

        writer.closeReference("PlayerInfoSection", nw::snd::internal::ElementType_SoundArchiveFile_PlayerInfoSection);
        writer.pushOffsetBase();
        {
            writer.openReferenceTable("PlayerInfoSectionTable", mPlayerList.size());

            for (const Item* item : mPlayerList)
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::Player);
                const Player* player = static_cast<const Player*>(item);

                writer.addReferenceTableReference("PlayerInfoSectionTable", nw::snd::internal::ElementType_SoundArchiveFile_PlayerInfo);

                stream.writeU32(player->getPlayableSoundMax());

                std::unordered_map<u32, u32> flags;
                if (player->isEnableName())
                {
                    flags[nw::snd::internal::PLAYER_INFO_STRING_ID] = getStringId(player->getName());
                }

                if (player->isEnablePlayerHeapSize())
                {
                    flags[nw::snd::internal::PLAYER_INFO_HEAP_SIZE] = player->getPlayerHeapSize();
                }

                FlagParameters params(flags);
                params.write(writer);
            }

            writer.closeReferenceTable("PlayerInfoSectionTable");
        }
        writer.popOffsetBase();

        writer.closeReference("FileInfoSection", nw::snd::internal::ElementType_SoundArchiveFile_FileInfoSection);

        writer.pushOffsetBase();
        {
            writer.openReferenceTable("FileInfoSectionTable", files.size());

            for (const File& file : files)
            {
                writer.addReferenceTableReference("FileInfoSectionTable", nw::snd::internal::ElementType_SoundArchiveFile_FileInfo);

                writer.pushOffsetBase();
                {
                    writer.openReference("DetailFileInfo");

                    stream.writeU32(0); // Option Parameter

                    writer.pushOffsetBase();
                    {
                        if (file.innerFile)
                        {
                            writer.closeReference("DetailFileInfo", nw::snd::internal::ElementType_SoundArchiveFile_InternalFileInfo);

                            writer.openSizedReference(sead::FormatFixedSafeString<32>("File%u", file.id));

                            writer.openReference("AttachedGroupIdTable");
                            writer.closeReference("AttachedGroupIdTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

                            std::vector<u32> groupIds;

                            const VectorSet<const Item*>& fileItems = filesItems[file.id];
                            for (const Item* fileItem : fileItems)
                            {
                                const VectorSet<const Group*>& attachedGroups = itemAttachedGroups[fileItem];
                                for (const Group* group : attachedGroups)
                                {
                                    groupIds.push_back(group->getId());
                                }
                            }

                            stream.writeU32(groupIds.size()); // AttachedGroupIdTable count

                            for (u32 groupId : groupIds)
                            {
                                stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(groupId, nw::snd::internal::ItemType_Group)); // AttachedGroupIdTable entries
                            }
                        }
                        else if (file.external)
                        {
                            writer.closeReference("DetailFileInfo", nw::snd::internal::ElementType_SoundArchiveFile_ExternalFileInfo);

                            stream.writeString(file.externalPath.c_str(), file.externalPath.size() + 1);
                            writer.align(4);
                        }
                        else
                        {
                            writer.closeNullReference("DetailFileInfo");
                        }
                    }
                    writer.popOffsetBase();
                }
                writer.popOffsetBase();
            }

            writer.closeReferenceTable("FileInfoSectionTable");
        }
        writer.popOffsetBase();

        writer.closeReference("SoundArchivePlayerInfo", nw::snd::internal::ElementType_SoundArchiveFile_SoundArchivePlayerInfo);
        writer.pushOffsetBase();
        {
            stream.writeU16(mSoundArchivePlayerInfo.sequenceSoundMax);
            stream.writeU16(mSoundArchivePlayerInfo.sequenceTrackMax);
            stream.writeU16(mSoundArchivePlayerInfo.streamSoundMax);
            stream.writeU16(mSoundArchivePlayerInfo.streamTrackMax);
            stream.writeU16(mSoundArchivePlayerInfo.streamChannelMax);
            stream.writeU16(mSoundArchivePlayerInfo.waveSoundMax);
            stream.writeU16(mSoundArchivePlayerInfo.waveTrackMax);
            stream.writeU8(isStreamPrefetchAvailable() ? mSoundArchivePlayerInfo.streamBufferTimes : 0);
            stream.writeU8(0); // Padding
            stream.writeU32(mSoundArchivePlayerInfo.options);
        }
        writer.popOffsetBase();

        writer.align(0x20);
        writer.closeBlock();
    }

    //? File Block
    {
        writer.openBlock(nw::snd::internal::ElementType_SoundArchiveFile_FileBlock, "FILE");

        u32 fileBlockPos = writer.getPosition() - sizeof(nw::ut::BinaryBlockHeader);
        writer.align(0x20);

        u32 lastFileId = nw::snd::SoundArchive::INVALID_ID;
        for (auto it = files.rbegin(); it != files.rend(); ++it)
        {
            if (it->includeInBfsar)
            {
                lastFileId = it->id;
                break;
            }
        }

        for (const File& file : files)
        {
            if (file.external || !file.includeInBfsar || !file.innerFile)
            {
                continue;
            }

            writer.align(0x20);

            const InnerFile* innerFile = file.innerFile;

            bool isGroup = false;

            {
                const BfwsdFile* bfwsdFile = sead::DynamicCast<const BfwsdFile>(innerFile);
                if (bfwsdFile)
                {
                    const VectorSet<const Item *>& fileItems = filesItems[file.id];
                    SEAD_ASSERT(fileItems.size() == 1);

                    const Item* item = fileItems.front();
                    SEAD_ASSERT(item->getItemType() == Item::ItemType::SoundSet);

                    const SoundSet* soundSet = static_cast<const SoundSet*>(item);
                    SEAD_ASSERT(soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave);

                    const auto& itWarcs = waveSoundSetsWarcs.find(soundSet);
                    SEAD_ASSERT(itWarcs != waveSoundSetsWarcs.end());

                    const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                    SEAD_ASSERT(warcMap.size() > 0);

                    const auto& itWarc = warcMap.find(nullptr);
                    SEAD_ASSERT(warcMap.isInMap(itWarc));

                    const WaveArchive* warc = itWarc->second;
                    SEAD_ASSERT(warc);

                    bfwsdFile->prepare(soundSet, warc, warcWaveFilesIndexes, true);
                }

                const BankFile* bankFile = sead::DynamicCast<const BankFile>(innerFile);
                if (bankFile)
                {
                    const VectorSet<const Item *>& fileItems = filesItems[file.id];
                    SEAD_ASSERT(fileItems.size() == 1);

                    const Item* item = fileItems.front();
                    SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);

                    const Bank* bank = static_cast<const Bank*>(item);

                    const auto& itWarcs = banksWarcs.find(bank);
                    SEAD_ASSERT(itWarcs != banksWarcs.end());

                    const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                    SEAD_ASSERT(warcMap.size() > 0);

                    const auto& itWarc = warcMap.find(nullptr);
                    SEAD_ASSERT(warcMap.isInMap(itWarc));

                    const WaveArchive* warc = itWarc->second;
                    SEAD_ASSERT(warc);

                    bankFile->prepare(bank, warc, warcWaveFilesIndexes, true);
                }

                const BfwarFile* bfwarFile = sead::DynamicCast<const BfwarFile>(innerFile);
                if (bfwarFile)
                {
                    bfwarFile->prepare(true);
                }

                const BfgrpFile* bfgrpFile = sead::DynamicCast<const BfgrpFile>(innerFile);
                if (bfgrpFile)
                {
                    isGroup = true;

                    const VectorSet<const Item *>& fileItems = filesItems[file.id];
                    SEAD_ASSERT(fileItems.size() == 1);

                    const Item* item = fileItems.front();
                    SEAD_ASSERT(item->getItemType() == Item::ItemType::Group);

                    const Group* group = static_cast<const Group*>(item);

                    bfgrpFile->prepare(
                        group,
                        groupItemFiles[group],
                        files,
                        filesItems,
                        waveSoundSetsWarcs,
                        banksWarcs,
                        warcWaveFilesIndexes,
                        groupTargetWarcs
                    );
                }
            }

            u32 startPos = writer.getPosition();
            u32 size = 0;
            {
                SEAD_ASSERT(innerFile);
                size = innerFile->write(&handle, &stream, mEndian, file.id == lastFileId);
            }

            writer.closeSizedReference(
                sead::FormatFixedSafeString<32>("File%u", file.id),
                isGroup ? 0 : nw::snd::internal::ElementType_General_ByteStream,
                startPos - fileBlockPos - sizeof(nw::ut::BinaryBlockHeader),
                size
            );

            // {
            //     u32 prevPos = writer.getPosition();
            //     writer.seek(startPos);
            //     stream.writeU16(file.id);
            //     writer.seek(prevPos);
            // }

            innerFile->clearWriteInfo();
        }

        for (const File& file : files)
        {
            if (file.external || file.includeInBfsar || !file.innerFile)
            {
                continue;
            }

            const InnerFile* innerFile = file.innerFile;

            u32 writePos = innerFile->getWritePos();
            if (writePos != 0xFFFFFFFF)
            {
                u32 writeSize = innerFile->getWriteSize();
                SEAD_ASSERT(writeSize != 0xFFFFFFFF);

                writer.closeSizedReference(
                    sead::FormatFixedSafeString<32>("File%u", file.id),
                    nw::snd::internal::ElementType_General_ByteStream,
                    writePos - fileBlockPos - sizeof(nw::ut::BinaryBlockHeader),
                    writeSize
                );
            }
            else
            {
                u32 writeSize = innerFile->getWriteSize();
                SEAD_ASSERT(writeSize == 0xFFFFFFFF);

                writer.closeNullSizedReference(sead::FormatFixedSafeString<32>("File%u", file.id));
            }

            innerFile->clearWriteInfo();
        }

        writer.closeBlock();
    }

    writer.closeFile();

    for (InnerFile* innerFile : generatedInnerFiles)
    {
        delete innerFile;
    }

    for (const WaveArchive* warc : generatedWarcs)
    {
        delete warc;
    }

    updateList(mWaveArchiveList);

    //? Stream Files
    {
        extern bool CreateDirectoryRecursively(const std::string& directory);
        extern bool WriteBfstmFile(sead::FileHandle& handle, const Sound::StreamSoundInfo& soundInfo);

        std::unordered_set<std::string> writenFiles;

        for (const Item* item : mSoundList)
        {
            const Sound* sound = static_cast<const Sound*>(item);
            if (sound->getSoundType() != Sound::SoundType::Strm)
            {
                continue;
            }

            const Sound::StreamSoundInfo& strmSoundInfo = sound->getStreamSoundInfo();

            SEAD_ASSERT(!strmSoundInfo.getPath().isEmpty());
            const char* path = strmSoundInfo.getPath().cstr();

            if (writenFiles.contains(path))
            {
                continue;
            }

            sead::FixedSafeString<512> dir;
            bool b = sead::Path::getDirectoryName(&dir, getFilePath());
            SEAD_ASSERT(b);

            sead::FormatFixedSafeString<512> savePath("%s/%s.save.bfstm", dir.cstr(), path); // TODO: Remove
            // sead::FormatFixedSafeString<512> savePath("%s/%s", dir.cstr(), path);
            //SEAD_PRINT("%s\n", savePath.cstr());

            b = sead::Path::getDirectoryName(&dir, savePath);
            SEAD_ASSERT(b);

            if (!CreateDirectoryRecursively(dir.cstr()))
            {
                SEAD_ASSERT_MSG(false, "Could not create stream path directory (%s)", dir.cstr());
            }

            sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
            SEAD_ASSERT(device);

            sead::FileHandle strmHandle;
            device->tryOpen(&strmHandle, savePath, sead::FileDevice::FileOpenFlag::eWriteOnly, 0);

            if (!strmHandle.getDevice())
            {
                device->tryOpen(&strmHandle, savePath, sead::FileDevice::FileOpenFlag::eCreate, 0);

                if (!strmHandle.getDevice())
                {
                    SEAD_ASSERT_MSG(false, "Error opening stream file (%s)", savePath.cstr());
                }
            }

            WriteBfstmFile(strmHandle, strmSoundInfo);

            writenFiles.emplace(path);
        }
    }
}

void Bfsar::close_()
{
    mSoundList.clear();
    mSoundSetList.clear();
    mBankList.clear();
    mWaveArchiveList.clear();
    mGroupList.clear();
    mPlayerList.clear();
    mWaveFileList.clear();
    mSequenceFileList.clear();
    mBankFileList.clear();
}

bool Bfsar::validate_()
{
    sead::FixedSafeString<1024> error;

    auto validateList = [&error](const ItemList& list, bool validateWaveSoundSet = false)
    {
        for (auto it = list.robustBegin(); it != list.robustEnd(); ++it)
        {
            const Item* item = (*it).val();
            SEAD_ASSERT(item);

            const Item* problemItem = nullptr;
            if (!validateWaveSoundSet)
            {
                if (item->getItemType() == Item::ItemType::Sound)
                {
                    const Sound* sound = static_cast<const Sound*>(item);
                    sound->mOwnerSet = nullptr; //? Reset for when validating SoundSets
                }

                problemItem = item->validate(error);
            }
            else
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::Sound);
                const Sound* sound = static_cast<const Sound*>(item);
                if (sound->getSoundType() == Sound::SoundType::Wave)
                {
                    const SoundSet* soundSet = sound->mOwnerSet;
                    if (!soundSet)
                    {
                        error = "Sound of type Wave must be in a SoundSet";
                        problemItem = item;
                    }
                    else if (soundSet->getSoundSetType() != SoundSet::SoundSetType::Wave)
                    {
                        error = "Sound of type Wave must be in a SoundSet of type Wave";
                        problemItem = soundSet; //? Set error item to SoundSet since it's the one with the issue
                    }
                }
            }

            if (problemItem)
            {
                if (error.isEmpty())
                {
                    error = "Unknown";
                }

                PopupMgr::PopupInfo info = { error.cstr(), const_cast<Item*>(problemItem) };
                if (problemItem->getItemType() == Item::ItemType::StreamTrack ||
                    problemItem->getItemType() == Item::ItemType::GroupItemInfo ||
                    problemItem->getItemType() == Item::ItemType::BankFileInstrument)
                {
                    info.super = const_cast<Item*>(item);
                }

                PopupMgr::instance()->addPopup(info);
                return false;
            }

            error.clear();
        }

        return true;
    };

    if (!validateList(mSoundList))
    {
        return false;
    }

    if (!validateList(mSoundSetList))
    {
        return false;
    }

    if (!validateList(mSoundList, true)) // Validate that Wave sounds are in SoundSets of type Wave
    {
        return false;
    }

    if (!validateList(mBankList))
    {
        return false;
    }

    if (!validateList(mWaveArchiveList))
    {
        return false;
    }

    if (!validateList(mGroupList))
    {
        return false;
    }

    if (!validateList(mPlayerList))
    {
        return false;
    }

    if (!validateList(mWaveFileList))
    {
        return false;
    }

    if (!validateList(mSequenceFileList))
    {
        return false;
    }

    if (!validateList(mBankFileList))
    {
        return false;
    }

    return true;
}

bool Bfsar::validateName_(const sead::SafeString& name, const Item::List& list, const Item* ignoreItem) const
{
    for (auto it = list.robustBegin(); it != list.robustEnd(); ++it)
    {
        const Item* item = (*it).val();
        SEAD_ASSERT(item);

        if (ignoreItem && item == ignoreItem)
        {
            continue;
        }

        if (name == item->getName())
        {
            return false;
        }
    }

    return true;
}
