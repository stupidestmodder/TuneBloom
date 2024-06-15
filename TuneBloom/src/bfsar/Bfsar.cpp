#include <bfsar/Bfsar.h>

#include <bfsar/writer/FileWriter.h>
#include <bfsar/writer/FlagParameters.h>
#include <bfsar/writer/PatriciaTree.h>

#include <filedevice/seadFileDeviceMgr.h>
#include <filedevice/seadPath.h>
#include <stream/seadFileDeviceStream.h>

#include <snd/snd_BankFileReader.h>
#include <snd/snd_GroupFileReader.h>
#include <snd/snd_WaveArchiveFileReader.h>
#include <snd/snd_WaveSoundFileReader.h>

#include <md5/md5.h>

#include <unordered_map>

Bfsar::Bfsar()
    : mOpen(false)
    , mBfsarFile(nullptr)
    , mSoundArchive(nullptr)
    , mSoundDataMgr()
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

bool Bfsar::open(const sead::SafeString& filePath, sead::Heap* heap)
{
    close();

    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
    SEAD_ASSERT(device);

    sead::FileDevice::LoadArg arg;
    arg.path = filePath;
    arg.heap = heap;

    mBfsarFile = device->load(arg);

    if (sead::MemUtil::compare(mBfsarFile, "FSAR", 4) != 0)
    {
        device->unload(mBfsarFile);
        mBfsarFile = nullptr;

        return false;
    }

    mFilePath = new(heap) sead::HeapSafeString(heap, filePath);

    mSoundArchive = new(heap) nw::snd::MemorySoundArchive();
    bool success = mSoundArchive->Initialize(mBfsarFile);
    SEAD_ASSERT(success);

    mSoundDataMgr.init(mSoundArchive, heap);

    open_(heap);

    mOpen = true;

    return true;
}

bool Bfsar::save()
{
    if (!mOpen)
        return false;

    sead::FormatFixedSafeString<512> path("%s.save.bfsar", mFilePath->cstr()); // TODO

    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
    SEAD_ASSERT(device);

    sead::FileHandle handle;
    device->tryOpen(&handle, path, sead::FileDevice::FileOpenFlag::eWriteOnly, 0);

    if (!handle.getDevice())
    {
        device->tryOpen(&handle, path, sead::FileDevice::FileOpenFlag::eCreate, 0);

        if (!handle.getDevice())
            return false;
    }

    save_(handle);

    return true;
}

bool Bfsar::saveAs(const sead::SafeString& filePath)
{
    if (!mOpen)
        return false;

    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
    SEAD_ASSERT(device);

    sead::FileHandle handle;
    device->tryOpen(&handle, filePath, sead::FileDevice::FileOpenFlag::eWriteOnly, 0);

    if (!handle.getDevice())
    {
        device->tryOpen(&handle, filePath, sead::FileDevice::FileOpenFlag::eCreate, 0);

        if (!handle.getDevice())
            return false;
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

    mSoundDataMgr.deinit();

    if (mSoundArchive)
    {
        delete mSoundArchive;
        mSoundArchive = nullptr;
    }

    if (mBfsarFile)
    {
        sead::FileDeviceMgr::instance()->unload(mBfsarFile);
        mBfsarFile = nullptr;
    }

    if (mFilePath)
    {
        delete mFilePath;
        mFilePath = nullptr;
    }

    mOpen = false;
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

void Bfsar::updateList(Item::List& list)
{
    u32 i = 0;
    for (Item* item : list)
    {
        item->setId(i);

        i++;
    }
}

void Bfsar::open_(sead::Heap* heap)
{
    mEndian = nw::ut::GetFileEndian(mSoundArchive->mHeader);

    mVersion = mSoundArchive->mHeader.version;

    mIncludeStringTable = mSoundArchive->mStringBlockBody != nullptr;

    {
        const nw::snd::internal::SoundArchiveFile::SoundArchivePlayerInfo* playerInfo = mSoundArchive->GetSoundArchivePlayerInfo();

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

/*
    for (Item* item : mFileList)
    {
        File* file = static_cast<File*>(item);

        if (file->getLocationType() != File::LocationType::Internal)
        {
            continue;
        }

        const nw::snd::internal::SoundArchiveFile::FileInfo* fileInfo = mSoundArchive->detail_GetFileInfo(file->getId());
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
            const void* fileAddr = mSoundArchive->detail_GetFileAddress(file->getId(), &fileSize, variationId, &groupFileId);
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
    for (u32 i = 0; i < mSoundArchive->GetPlayerCount(); i++)
    {
        const nw::snd::internal::SoundArchiveFile::PlayerInfo* playerInfo = mSoundArchive->GetPlayerInfo(mSoundArchive->GetPlayerIdFromIndex(i));

        Player* player = new(heap) Player();
        player->mId = i;

        player->mEnableName = playerInfo->optionParameter.GetTrueCount(nw::snd::internal::PLAYER_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && playerInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            player->mName = mSoundArchive->GetString(playerInfo->GetStringId());
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

    for (u32 i = 0; i < mSoundArchive->detail_GetFileCount(); i++)
    {
        u32 seqFileSize = 0;
        const void* seqFile = mSoundArchive->detail_GetFileAddress(i, &seqFileSize);

        if (!seqFile || sead::MemUtil::compare(seqFile, "FSEQ", 4) != 0)
        {
            continue;
        }

        u32 globalId = mSequenceFileList.size();

        SequenceFile* sequence = new(heap) SequenceFile();
        sequence->mId = globalId;

        sequence->mEnableName = true;
        sequence->mName = "Sequence";

        sequence->read(seqFile, seqFileSize);

        mSequenceFileList.pushBack(sequence);

        seqFileIdxMap.try_emplace(i, globalId);
    }

    std::unordered_map<std::string, TempFile> waveFileCache;
    std::unordered_map<u64, u32> waveFileIdxMap;

    for (u32 i = 0; i < mSoundArchive->GetWaveArchiveCount(); i++)
    {
        u32 waveArchiveId = mSoundArchive->GetWaveArchiveIdFromIndex(i);
        const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* warcInfo = mSoundArchive->GetWaveArchiveInfo(waveArchiveId);

        nw::snd::internal::WaveArchiveFileReader reader(mSoundArchive->detail_GetFileAddress(warcInfo->fileId));

        for (u32 j = 0; j < reader.GetWaveFileCount(); j++)
        {
            const void* waveFile = reader.GetWaveFile(j);
            u32 waveFileSize = reader.GetWaveFileSize(j);

            std::string hash = md5(waveFile, waveFileSize);

            bool isUnique = true;
            u32 globalId;

            const auto& it = waveFileCache.find(hash);
            if (it != waveFileCache.end())
            {
                const TempFile& ref = it->second;
                if (ref.fileDataSize == waveFileSize && sead::MemUtil::compare(ref.fileData, waveFile, waveFileSize) == 0)
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
                globalId = mWaveFileList.size();
                
                WaveFile* wave = new(heap) WaveFile();
                wave->mId = globalId;

                wave->mEnableName = true;
                wave->mName = "Wave";

                wave->read(waveFile);

                mWaveFileList.pushBack(wave);

                waveFileCache.try_emplace(hash, globalId, waveFile, waveFileSize);
            }

            waveFileIdxMap.try_emplace((u64)waveArchiveId << 32 | j, globalId);
        }

        if (warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_STRING_ID) == 0)
        {
            continue;
        }

        WaveArchive* warc = new(heap) WaveArchive();
        warc->mId = i;

        warc->mEnableName = warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && warcInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            warc->mName = mSoundArchive->GetString(warcInfo->GetStringId());
        }

        warc->mIsLoadIndividual = warcInfo->isLoadIndividual;
        if (warc->mIsLoadIndividual)
        {
            SEAD_ASSERT(warcInfo->optionParameter.GetTrueCount(nw::snd::internal::WAVE_ARCHIVE_INFO_WAVE_COUNT) != 0);
        }

        mWaveArchiveList.pushBack(warc);
    }

    std::unordered_map<std::string, TempFile> bankFileCache;
    std::unordered_map<u32, u32> bankFileIdxMap;

    for (u32 i = 0; i < mSoundArchive->detail_GetFileCount(); i++)
    {
        u32 bankFileSize = 0;
        const void* bankFile = mSoundArchive->detail_GetFileAddress(i, &bankFileSize);

        if (!bankFile || sead::MemUtil::compare(bankFile, "FBNK", 4) != 0)
        {
            continue;
        }

        nw::snd::internal::BankFileReader reader(bankFile);
        SEAD_ASSERT(reader.IsInitialized());

        const nw::snd::internal::Util::WaveIdTable& waveIdTable = *reader.GetWaveIdTable();
        nw::snd::internal::Util::WaveId* waveId = const_cast<nw::snd::internal::Util::WaveId*>(&waveIdTable.table.item[0]);
        for (u32 j = 0; j < waveIdTable.GetCount(); j++, waveId++)
        {
            u32 warcId = waveId->waveArchiveId;
            u32 waveIdx = waveId->waveIndex;

            const auto& it = waveFileIdxMap.find((u64)warcId << 32 | waveIdx);
            SEAD_ASSERT(it != waveFileIdxMap.end());
            u32 globalWaveId = it->second;

            waveId->waveArchiveId = 0;
            waveId->waveIndex = globalWaveId;
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

            bank->read(bankFile);

            mBankFileList.pushBack(bank);

            bankFileCache.try_emplace(hash, globalId, bankFile, bankFileSize);
        }

        bankFileIdxMap.try_emplace(i, globalId);
    }

    for (u32 i = 0; i < mSoundArchive->GetBankCount(); i++)
    {
        const nw::snd::internal::SoundArchiveFile::BankInfo* bankInfo = mSoundArchive->GetBankInfo(mSoundArchive->GetBankIdFromIndex(i));

        Bank* bank = new(heap) Bank();
        bank->mId = i;

        bank->mEnableName = bankInfo->optionParameter.GetTrueCount(nw::snd::internal::BANK_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && bankInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            bank->mName = mSoundArchive->GetString(bankInfo->GetStringId());
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

        SEAD_ASSERT(bank->mWaveArchiveType != WaveArchiveType::Invalid);

        bank->mFile.attach(getItem(bankFileIdxMap[bankInfo->fileId], getBankFileList()));
        SEAD_ASSERT(bank->mFile.isAttached());

        // TODO: Load Bank stuff (Instruments...)

        mBankList.pushBack(bank);
    }

    for (u32 i = 0; i < mSoundArchive->GetSoundCount(); i++)
    {
        const nw::snd::internal::SoundArchiveFile::SoundInfo* soundInfo = mSoundArchive->GetSoundInfo(mSoundArchive->GetSoundIdFromIndex(i));

        Sound* sound = new(heap) Sound();
        sound->mId = i;

        sound->mEnableName = soundInfo->optionParameter.GetTrueCount(nw::snd::internal::SOUND_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && soundInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            sound->mName = mSoundArchive->GetString(soundInfo->GetStringId());
        }

        sound->mPlayerRef.attach(getItem(soundInfo->playerId, getPlayerList()));
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
                }
            }

            sound->mSequenceSoundInfo.mAllocateTrackFlags = seqSoundInfo.allocateTrackFlags;

            sound->mSequenceSoundInfo.mEnableStartOffset = seqSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::SEQ_SOUND_INFO_START_OFFSET) != 0;
            if (sound->mSequenceSoundInfo.mEnableStartOffset)
                sound->mSequenceSoundInfo.mStartOffset = seqSoundInfo.GetStartOffset();

            sound->mSequenceSoundInfo.mEnablePriority = seqSoundInfo.optionParameter.GetTrueCount(nw::snd::internal::SEQ_SOUND_INFO_PRIORITY) != 0;
            if (sound->mSequenceSoundInfo.mEnablePriority)
            {
                sound->mSequenceSoundInfo.mChannelPriority = seqSoundInfo.GetChannelPriority();
                sound->mSequenceSoundInfo.mIsReleasePriorityFix = seqSoundInfo.IsReleasePriorityFix();
            }

            u32 globalSeqIdx = seqFileIdxMap[soundInfo->fileId];
            SequenceFile* seqFile = static_cast<SequenceFile*>(getItem(globalSeqIdx, getSequenceFileList()));

            sound->mSequenceSoundInfo.mSequenceFileRef.attach(seqFile);
        }
        else if (sound->mSoundType == Sound::SoundType::Strm)
        {
            const nw::snd::internal::SoundArchiveFile::StreamSoundInfo& strmSoundInfo = soundInfo->GetStreamSoundInfo();

            const nw::snd::internal::SoundArchiveFile::ExternalFileInfo* extFileInfo = mSoundArchive->detail_GetFileInfo(soundInfo->fileId)->GetExternalFileInfo();
            SEAD_ASSERT(extFileInfo);

            sound->mStreamSoundInfo.mPath = extFileInfo->filePath;

            sound->mStreamSoundInfo.mAllocateTrackFlags = strmSoundInfo.allocateTrackFlags;
            sound->mStreamSoundInfo.mAllocateChannelCount = strmSoundInfo.allocateChannelCount;

            if (isStreamTrackInfoAvailable())
            {
                const nw::snd::internal::SoundArchiveFile::StreamTrackInfoTable* trackInfoTable = strmSoundInfo.GetTrackInfoTable();
                SEAD_ASSERT(trackInfoTable);

                for (u32 j = 0; j < nw::snd::SoundArchive::STRM_TRACK_NUM && j < trackInfoTable->GetTrackCount(); j++)
                {
                    const nw::snd::internal::SoundArchiveFile::StreamTrackInfo* trackInfo = trackInfoTable->GetTrackInfo(j);
                    SEAD_ASSERT(trackInfo);

                    Sound::StreamSoundInfo::Track& track = *sound->mStreamSoundInfo.mTracks.birthBack();

                    track.mVolume = trackInfo->volume;
                    track.mPan = trackInfo->pan;
                    track.mSPan = trackInfo->span;
                    track.mFlags = trackInfo->flags;

                    u32 channelCount = trackInfo->GetTrackChannelCount();
                    SEAD_ASSERT(channelCount <= nw::snd::WAVE_CHANNEL_MAX);

                    for (u32 k = 0; k < channelCount; k++)
                    {
                        u8& channel = *track.getChannels().birthBack();
                        channel = trackInfo->GetGlobalChannelIndex(k);
                    }

                    if (isStreamSendAvailable())
                    {
                        const nw::snd::internal::SoundArchiveFile::SendValue send = trackInfo->GetSendValue();
                        track.mMainSend = send.mainSend;

                        for (u32 k = 0; k < nw::snd::AUX_BUS_NUM; k++)
                        {
                            track.mFxSend[k] = send.fxSend[k];
                        }
                    }

                    if (isFilterSupportedVersion())
                    {
                        track.mLpfFreq = trackInfo->lpfFreq;
                        track.mBiquadType = trackInfo->biquadType;
                        track.mBiquadValue = trackInfo->biquadValue;
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

            nw::snd::internal::WaveSoundFileReader reader(mSoundArchive->detail_GetFileAddress(soundInfo->fileId));

            nw::snd::internal::WaveSoundNoteInfo noteInfo;
            bool success = reader.ReadNoteInfo(&noteInfo, waveSoundInfo.index, 0);
            SEAD_ASSERT(success);

            u32 globalWaveIdx = waveFileIdxMap[(u64)noteInfo.waveArchiveId << 32 | noteInfo.waveIndex];
            WaveFile* waveFile = static_cast<WaveFile*>(getItem(globalWaveIdx, getWaveFileList()));

            sound->mWaveSoundInfo.mWaveFileRef.attach(waveFile);
        }

        mSoundList.pushBack(sound);
    }

    for (u32 i = 0; i < mSoundArchive->GetSoundGroupCount(); i++)
    {
        const nw::snd::internal::SoundArchiveFile::SoundGroupInfo* soundSetInfo = mSoundArchive->detail_GetSoundGroupInfo(mSoundArchive->GetSoundGroupIdFromIndex(i));

        SoundSet* soundSet = new(heap) SoundSet();
        soundSet->mId = i;

        soundSet->mEnableName = soundSetInfo->optionParameter.GetTrueCount(nw::snd::internal::SOUND_GROUP_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && soundSetInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            soundSet->mName = mSoundArchive->GetString(soundSetInfo->GetStringId());
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

        SEAD_ASSERT(soundSet->mWaveArchiveType != WaveArchiveType::Invalid);

        soundSet->mStartId = nw::snd::SoundArchive::INVALID_ID;
        if (soundSetInfo->startId != nw::snd::SoundArchive::INVALID_ID)
            soundSet->mStartId = nw::snd::internal::Util::GetItemIndex(soundSetInfo->startId);

        soundSet->mEndId = nw::snd::SoundArchive::INVALID_ID;
        if (soundSetInfo->endId != nw::snd::SoundArchive::INVALID_ID)
            soundSet->mEndId = nw::snd::internal::Util::GetItemIndex(soundSetInfo->endId);

        mSoundSetList.pushBack(soundSet);
    }

    for (u32 i = 0; i < mSoundArchive->GetGroupCount(); i++)
    {
        const nw::snd::internal::SoundArchiveFile::GroupInfo* groupInfo = mSoundArchive->GetGroupInfo(mSoundArchive->GetGroupIdFromIndex(i));

        Group* group = new(heap) Group();
        group->mId = i;

        group->mEnableName = groupInfo->optionParameter.GetTrueCount(nw::snd::internal::GROUP_INFO_STRING_ID) != 0;
        if (mIncludeStringTable && groupInfo->GetStringId() != nw::snd::internal::DEFAULT_STRING_ID)
        {
            group->mName = mSoundArchive->GetString(groupInfo->GetStringId());
        }

        nw::snd::internal::GroupFileReader reader(mSoundArchive->detail_GetFileAddress(groupInfo->fileId));
        if (reader.GetGroupItemCount() > 0)
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
        }

        mGroupList.pushBack(group);
    }
}

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

    //? String Block
    if (mIncludeStringTable)
    {
        writer.openBlock(nw::snd::internal::ElementType_SoundArchiveFile_StringBlock, "STRG");

        writer.openReference("StringTable");
        writer.openReference("PatriciaTree");

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

        auto writeIdTable = [&](const IdTable& table, nw::snd::internal::ItemType type)
        {
            stream.writeU32(table.size());

            for (const IdEntry* entry : table)
            {
                SEAD_ASSERT(entry);

                stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(entry->getId(), type));
            }
        };

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

                    //stream.writeU32(sound->getFileId()); // TODO

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

                        if (sound->isEnableIsFrontBypass())
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
                                for (u32 i = numBanks - 1; i >= 0; i--)
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
                                        writer.openReference("StreamSoundExtension");

                                        if (isStreamPrefetchAvailable())
                                        {
                                            stream.writeU32(strmInfo.getPrefetchFileId());
                                        }
                                    }

                                    if (strmInfo.getTracks().size() > 0)
                                    {
                                        writer.closeReference("TrackInfoTableRef", nw::snd::internal::ElementType_Table_ReferenceTable);
                                        writer.pushOffsetBase();
                                        {
                                            writer.openReferenceTable("TrackInfoTable", strmInfo.getTracks().size());

                                            for (u32 i = 0; i < strmInfo.getTracks().size(); i++)
                                            {
                                                const Sound::StreamSoundInfo::Track& track = *strmInfo.getTracks().nth(i);

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
                                                    stream.writeU32(track.getChannels().size());

                                                    for (u32 j = 0; j < track.getChannels().size(); j++)
                                                    {
                                                        stream.writeU8(*track.getChannels().nth(j));
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

                                //stream.writeU32(waveInfo.getIndex()); // TODO
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
                    stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(soundSet->getStartId(), nw::snd::internal::ItemType_Sound));
                    stream.writeU32(nw::snd::internal::Util::GetMaskedItemId(soundSet->getEndId(), nw::snd::internal::ItemType_Sound));

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

                    //writeIdTable(soundSet->getFileIdTable(), nw::snd::internal::ItemType(0)); // TODO

                    if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave)
                    {
                        writer.closeReference("DetailSoundGroup", nw::snd::internal::ElementType_SoundArchiveFile_WaveSoundGroupInfo);

                        writer.pushOffsetBase();
                        {
                            writer.openReference("WaveArchiveIdTable");

                            stream.writeU32(0); // Option Parameter

                            writer.closeReference("WaveArchiveIdTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

                            //writeIdTable(soundSet->getWaveArchiveIdTable(), nw::snd::internal::ItemType_WaveArchive); // TODO
                        }
                        writer.popOffsetBase();
                    }
                    else
                    {
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
                    //stream.writeU32(bank->getFileId()); // TODO

                    writer.openReference("WaveArchiveIdTable");

                    std::unordered_map<u32, u32> flags;
                    if (bank->isEnableName())
                    {
                        flags[nw::snd::internal::BANK_INFO_STRING_ID] = getStringId(bank->getName());
                    }

                    FlagParameters params(flags);
                    params.write(writer);

                    writer.closeReference("WaveArchiveIdTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

                    //writeIdTable(bank->getWaveArchiveIdTable(), nw::snd::internal::ItemType_WaveArchive); // TODO
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

                //stream.writeU32(warc->getFileId()); // TODO
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
                    u32 waveCount = 0; // TODO: Get actual count from file

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

                //stream.writeU32(group->getFileId()); // TODO

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
/*
        writer.pushOffsetBase();
        {
            writer.openReferenceTable("FileInfoSectionTable", mFileList.size());

            for (const Item* item : mFileList)
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::File);
                const File* file = static_cast<const File*>(item);

                writer.addReferenceTableReference("FileInfoSectionTable", nw::snd::internal::ElementType_SoundArchiveFile_FileInfo);

                writer.pushOffsetBase();
                {
                    writer.openReference("DetailFileInfo");

                    stream.writeU32(0); // Option Parameter

                    writer.pushOffsetBase();
                    {
                        switch (file->getLocationType())
                        {
                            case File::LocationType::None:
                                writer.closeNullReference("DetailFileInfo");
                                break;

                            case File::LocationType::Internal:
                            case File::LocationType::Group:
                            {
                                writer.closeReference("DetailFileInfo", nw::snd::internal::ElementType_SoundArchiveFile_InternalFileInfo);

                                writer.openSizedReference(sead::FormatFixedSafeString<32>("File%u", file->getId()));

                                if (file->getLocationType() == File::LocationType::Group)
                                {
                                    writer.closeNullSizedReference(sead::FormatFixedSafeString<32>("File%u", file->getId()));
                                }

                                writer.openReference("AttachedGroupIdTable");
                                writer.closeReference("AttachedGroupIdTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

                                writeIdTable(file->getGroupIdTable(), nw::snd::internal::ItemType_Group);
                                break;
                            }

                            case File::LocationType::External:
                                writer.closeReference("DetailFileInfo", nw::snd::internal::ElementType_SoundArchiveFile_ExternalFileInfo);

                                stream.writeString(file->getName(), file->getName().calcLength() + 1);
                                writer.align(4);
                                break;

                            default:
                                SEAD_ASSERT_MSG(false, "invalid LocationType");

                                writer.closeNullReference("DetailFileInfo");
                                break;
                        }
                    }
                    writer.popOffsetBase();
                }
                writer.popOffsetBase();
            }

            writer.closeReferenceTable("FileInfoSectionTable");
        }
        writer.popOffsetBase();
*/

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

/*
        u32 lastFileId = 0xFFFFFFFF;
        for (const Item* item : mFileList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::File);
            const File* file = static_cast<const File*>(item);

            for (u32 i = 0; i < file->getVariations().size(); i++)
            {
                bool embeddedInGroup = file->isEmbeddedInGroup(i);
                if (file->getLocationType() == File::LocationType::Internal && !embeddedInGroup)
                {
                    lastFileId = file->getId();
                    break;
                }
            }
        }

        for (const Item* item : mFileList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::File);
            const File* file = static_cast<const File*>(item);

            for (u32 i = 0; i < file->getVariations().size(); i++)
            {
                bool embeddedInGroup = file->isEmbeddedInGroup(i);
                if (file->getLocationType() == File::LocationType::Group && !embeddedInGroup)
                {
                    //! Bad cuz file will be lost...
                    SEAD_PRINT("BRUH BAD\n");
                }

                if (file->getLocationType() == File::LocationType::Internal && !embeddedInGroup)
                {
                    writer.align(0x20);

                    const InnerFile* innerFile = file->getInnerFile(i);

                    u32 startPos = writer.getPosition();
                    u32 size = 0;
                    {
                        SEAD_ASSERT(innerFile);
                        size = innerFile->write(&handle, &stream, mEndian, file->getId() == lastFileId);
                    }

                    writer.closeSizedReference(
                        sead::FormatFixedSafeString<32>("File%u", file->getId()),
                        file->getFileType() == File::FileType::Bfgrp ? 0 : nw::snd::internal::ElementType_General_ByteStream,
                        startPos - fileBlockPos - sizeof(nw::ut::BinaryBlockHeader),
                        size
                    );
                }
            }
        }

        for (Item* item : mFileList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::File);
            File* file = static_cast<File*>(item);

            for (u32 i = 0; i < file->getVariations().size(); i++)
            {
                InnerFile* innerFile = file->getInnerFile(i);

                bool embeddedInGroup = file->isEmbeddedInGroup(i);
                if (file->getLocationType() == File::LocationType::Internal && embeddedInGroup)
                {
                    SEAD_ASSERT(innerFile);

                    u32 writePos = innerFile->getWritePos();
                    u32 writeSize = innerFile->getWriteSize();
                    SEAD_ASSERT(writePos != 0xFFFFFFFF);
                    SEAD_ASSERT(writeSize != 0xFFFFFFFF);

                    sead::FormatFixedSafeString<32> refName("File%u", file->getId());
                    if (writer.hasSizedReference(refName))
                    {
                        writer.closeSizedReference(
                            refName,
                            nw::snd::internal::ElementType_General_ByteStream,
                            writePos - fileBlockPos - sizeof(nw::ut::BinaryBlockHeader),
                            writeSize
                        );
                    }
                }

                if (innerFile)
                {
                    innerFile->clearWriteInfo();
                }
            }
        }
*/

        writer.closeBlock();
    }

    writer.closeFile();
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

bool Bfsar::validateName_(const sead::SafeString& name, const Item::List& list) const
{
    for (auto it = list.robustBegin(); it != list.robustEnd(); ++it)
    {
        Item* item = (*it).val();

        if (name == item->getName())
            return false;
    }

    return true;
}
