#include <bfsar/BfgrpFile.h>

#include <bfsar/Bank.h>
#include <bfsar/BankFile.h>
#include <bfsar/BfwarFile.h>
#include <bfsar/BfwsdFile.h>
#include <bfsar/File.h>
#include <bfsar/Group.h>
#include <bfsar/SoundSet.h>
#include <bfsar/WaveArchive.h>

#include <snd/snd_GroupFile.h>

#include <ui/UI.h>

#include <algorithm>

u32 BfgrpFile::doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const
{
    SEAD_ASSERT(mGroup);
    SEAD_ASSERT(mItemFiles);
    SEAD_ASSERT(mFiles);
    SEAD_ASSERT(mFilesItems);
    SEAD_ASSERT(mWaveSoundSetsWarcs);
    SEAD_ASSERT(mBanksWarcs);
    SEAD_ASSERT(mWarcWaveFilesIndexes);
    SEAD_ASSERT(mGroupTargetWarcs);

    FileWriter writer(handle, stream);
    writer.openFile("FGRP", 3, mVersion);

    u32 fileSize = 0;

    //? Info Block
    {
        writer.openBlock(nw::snd::internal::ElementType_GroupFile_InfoBlock, "INFO");

        writer.openReferenceTable("ItemInfoTable", mItemFiles->size());

        for (u32 i : *mItemFiles)
        {
            const File& file = (*mFiles)[i];

            writer.addReferenceTableReference("ItemInfoTable", nw::snd::internal::ElementType_GroupFile_GroupItemInfo);

            stream->writeU32(file.id);

            sead::FormatFixedSafeString<32> refName("File%u", file.id);
            writer.openSizedReference(refName);

            if (mGroup->getOutputType() == Group::OutputType::Link)
            {
                writer.closeNullSizedReference(refName);
            }
        }

        writer.closeReferenceTable("ItemInfoTable");

        writer.align(0x20);
        writer.closeBlock();
    }

    //? File Block
    {
        writer.openBlock(nw::snd::internal::ElementType_GroupFile_FileBlock, "FILE");

        writer.align(0x20);

        // u32 lastItemInfoId = 0xFFFFFFFF;
        // for (const Item* item : mItemInfoList)
        // {
        //     SEAD_ASSERT(item->getItemType() == Item::ItemType::GroupItemInfo);
        //     const ItemInfo* itemInfo = static_cast<const ItemInfo*>(item);

        //     if (itemInfo->mEmbedType == ItemInfo::EmbedType::Embed)
        //     {
        //         if (const Item* itemFile = itemInfo->getFileRef().getItem())
        //         {
        //             SEAD_ASSERT(itemFile->getItemType() == Item::ItemType::File);
        //             const File* file = static_cast<const File*>(itemFile);

        //             const InnerFile* innerFile = file->getInnerFileByGroupFileId(mFileId);
        //             if (innerFile)
        //             {
        //                 lastItemInfoId = itemInfo->getId();
        //             }
        //         }
        //     }
        // }

        if (mGroup->getOutputType() == Group::OutputType::Embed)
        {
            for (u32 i : *mItemFiles)
            {
                const File& file = (*mFiles)[i];

                const InnerFile* innerFile = file.innerFile;
                SEAD_ASSERT(innerFile);

                writer.align(0x20);

                {
                    const BfwsdFile* bfwsdFile = sead::DynamicCast<const BfwsdFile>(innerFile);
                    if (bfwsdFile)
                    {
                        const VectorSet<const Item *>& fileItems = mFilesItems->find(file.id)->second;
                        SEAD_ASSERT(fileItems.size() == 1);

                        const Item* item = fileItems.front();
                        SEAD_ASSERT(item->getItemType() == Item::ItemType::SoundSet);

                        const SoundSet* soundSet = static_cast<const SoundSet*>(item);
                        SEAD_ASSERT(soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave);

                        const auto& itWarcs = mWaveSoundSetsWarcs->find(soundSet);
                        SEAD_ASSERT(itWarcs != mWaveSoundSetsWarcs->end());

                        const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                        SEAD_ASSERT(warcMap.size() > 0);

                        const WaveArchive* warc = nullptr;

                        const auto& itWarc = warcMap.find(mGroup);
                        if (warcMap.isInMap(itWarc))
                        {
                            warc = itWarc->second;
                        }
                        else
                        {
                            warc = warcMap.find(nullptr)->second;
                        }

                        SEAD_ASSERT(warc);

                        bfwsdFile->prepare(soundSet, warc, *mWarcWaveFilesIndexes, soundSet->getWaveArchiveType() != WaveArchiveType::AutomaticShared);
                    }

                    const BankFile* bankFile = sead::DynamicCast<const BankFile>(innerFile);
                    if (bankFile)
                    {
                        const VectorSet<const Item *>& fileItems = mFilesItems->find(file.id)->second;
                        SEAD_ASSERT(fileItems.size() == 1);

                        const Item* item = fileItems.front();
                        SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);

                        const Bank* bank = static_cast<const Bank*>(item);

                        const auto& itWarcs = mBanksWarcs->find(bank);
                        SEAD_ASSERT(itWarcs != mBanksWarcs->end());

                        const VectorMap<const Group*, const WaveArchive*>& warcMap = itWarcs->second;
                        SEAD_ASSERT(warcMap.size() > 0);

                        const WaveArchive* warc = nullptr;

                        const auto& itWarc = warcMap.find(mGroup);
                        if (warcMap.isInMap(itWarc))
                        {
                            warc = itWarc->second;
                        }
                        else
                        {
                            warc = warcMap.find(nullptr)->second;
                        }

                        SEAD_ASSERT(warc);

                        bankFile->prepare(bank, warc, *mWarcWaveFilesIndexes, bank->getWaveArchiveType() != WaveArchiveType::AutomaticShared);
                    }

                    const BfwarFile* bfwarFile = sead::DynamicCast<const BfwarFile>(innerFile);
                    if (bfwarFile)
                    {
                        const VectorSet<const Item *>& fileItems = mFilesItems->find(file.id)->second;
                        SEAD_ASSERT(fileItems.size() == 1);

                        const Item* item = fileItems.front();
                        SEAD_ASSERT(item->getItemType() == Item::ItemType::WaveArchive);

                        const WaveArchive* warc = static_cast<const WaveArchive*>(item);

                        bool isGroupWarc = false;

                        const auto& it = mGroupTargetWarcs->find(mGroup);
                        if (it != mGroupTargetWarcs->end())
                        {
                            isGroupWarc = warc == it->second;
                        }

                        bfwarFile->prepare(!isGroupWarc);
                    }
                }

                u32 pos = writer.getPosition();
                {
                    innerFile->write(handle, stream, mEndian, file.id == mItemFiles->size() - 1);
                }
                u32 size = writer.getPosition() - pos;

                writer.closeSizedReference(
                    sead::FormatFixedSafeString<32>("File%u", file.id),
                    nw::snd::internal::ElementType_General_ByteStream,
                    size
                );

                // {
                //     u32 prevPos = writer.getPosition();
                //     writer.seek(pos);
                //     stream->writeU16(file.id);
                //     writer.seek(prevPos);
                // }
            }
        }

        writer.align(0x20);
        writer.closeBlock();
    }

    //? InfoEx Block
    {
        writer.openBlock(nw::snd::internal::ElementType_GroupFile_InfoExBlock, "INFX");

        writer.openReferenceTable("ItemInfoExTable", mGroup->getItemInfoList().size());

        auto writeGroupItem = [&](const Group::ItemInfo& itemInfo)
        {
            writer.addReferenceTableReference("ItemInfoExTable", nw::snd::internal::ElementType_GroupFile_GroupItemInfoEx);

            u32 itemId = 0; //? Idk why not nw::snd::SoundArchive::INVALID_ID... 
            if (itemInfo.getItemRef().isAttached())
            {
                nw::snd::internal::ItemType itemType;
                switch (itemInfo.getItemRefType())
                {
                    case Item::ItemType::Sound:
                        itemType = nw::snd::internal::ItemType_Sound;
                        break;

                    case Item::ItemType::SoundSet:
                        itemType = nw::snd::internal::ItemType_SoundGroup;
                        break;

                    case Item::ItemType::Bank:
                        itemType = nw::snd::internal::ItemType_Bank;
                        break;

                    case Item::ItemType::WaveArchive:
                        itemType = nw::snd::internal::ItemType_WaveArchive;
                        break;

                    default:
                        itemType = nw::snd::internal::ItemType(0);
                        break;
                }

                itemId = nw::snd::internal::Util::GetMaskedItemId(itemInfo.getItemRef().getItemId(), itemType);
            }

            stream->writeU32(itemId);
            stream->writeU32(itemInfo.getLoadFlag());
        };

        if (sBfsar.getVersion() <= 0x00020000)
        {
            for (const Item* item : mGroup->getItemInfoList())
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::GroupItemInfo);
                const Group::ItemInfo* itemInfo = static_cast<const Group::ItemInfo*>(item);

                writeGroupItem(*itemInfo);
            }
        }
        else
        {
            std::vector<const Group::ItemInfo*> itemInfos;

            for (const Item* item : mGroup->getItemInfoList())
            {
                SEAD_ASSERT(item->getItemType() == Item::ItemType::GroupItemInfo);
                const Group::ItemInfo* itemInfo = static_cast<const Group::ItemInfo*>(item);

                if (!itemInfo->getItemRef().isAttached())
                {
                    continue;
                }

                itemInfos.push_back(itemInfo);
            }

            std::sort(itemInfos.begin(), itemInfos.end(), [](const Group::ItemInfo* a, const Group::ItemInfo* b) -> bool
                {
                    SEAD_ASSERT(a->getItemRef().isAttached());
                    SEAD_ASSERT(b->getItemRef().isAttached());

                    const Item* aItem = a->getItemRef().getItem();
                    const Item* bItem = b->getItemRef().getItem();

                    return aItem->getIdWithType() < bItem->getIdWithType();
                }
            );

            for (const Group::ItemInfo* itemInfo : itemInfos)
            {
                writeGroupItem(*itemInfo);
            }
        }

        writer.closeReferenceTable("ItemInfoExTable");

        fileSize = writer.getPosition();

        if (!isLast)
        {
            writer.align(0x20);
        }

        writer.closeBlock();
    }

    writer.closeFile();

    mGroup = nullptr;
    mItemFiles = nullptr;
    mFiles = nullptr;
    mFilesItems = nullptr;
    mWaveSoundSetsWarcs = nullptr;
    mBanksWarcs = nullptr;
    mWarcWaveFilesIndexes = nullptr;
    mGroupTargetWarcs = nullptr;

    return fileSize;
}
