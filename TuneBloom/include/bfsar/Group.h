#pragma once

#include <bfsar/Item.h>

class Sound;
class SoundSet;
class Bank;

class Group : public Item
{
public:
    class ItemInfo : public Item
    {
    public:
        enum LoadFlag
        {
            // Sound
            LoadSeq = 1 << 0,
            LoadWsd = 1 << 1,

            // Bank
            LoadBank = 1 << 2,

            // Waveform Archive
            LoadWarc = 1 << 3,

            LoadAll = 0xFFFFFFFF
        };

        enum class SequenceItems
        {
            All = 0,
            SequenceAndBank,
            SequenceAndWarc,
            BankAndWarc,
            Sequence,
            Bank,
            Warc,

            Num
        };

        enum class WaveSoundSetItems
        {
            All = 0,
            Wsd,
            Warc,

            Num
        };

        enum class BankItems
        {
            All = 0,
            Bank,
            Warc,

            Num
        };

        enum class WaveArchiveItems
        {
            All = 0,

            Num
        };

        static const char* sItemIdTypes[4];

    public:
        ItemInfo(Group* owner)
            : Item()
            , mItemRefType(ItemType::Invalid)
            , mItemRef(owner)
            //, mLoadFlag(LoadFlag::LoadAll)
            , mLoadItem(0) // All
        {
            mItemType = ItemType::GroupItemInfo;
        }

        sead::FixedSafeString<256> getFormattedName() const override
        {
            sead::FixedSafeString<256> name;
            name.appendWithFormat("[%u] ", getId());

            if (mItemRef.isAttached())
            {
                name.append(mItemRef.getItem()->getNameOrNull());
            }
            else
            {
                name.append("(null)");
            }

            return name;
        }

        void drawUI();

        ItemType getItemRefType() const
        {
            return mItemRefType;
        }

        void setItemRefType_(ItemType type)
        {
            mItemRefType = type;
        }

        const ItemReference& getItemRef() const
        {
            return mItemRef;
        }

        ItemReference& getItemRef()
        {
            return mItemRef;
        }

        void setLoadItem_(u32 loadItem)
        {
            mLoadItem = loadItem;
        }

        u32 getLoadFlag() const;
        const char** getLoadItems(u32* outCount) const;

        static const char** GetLoadItems(const Item* item, ItemType itemType, u32* outCount);

        void setLoadItems(u32 loadFlags);

        bool validate(sead::BufferedSafeString& error) const override;

    private:
        u32 getLoadFlagForSequence_() const;
        u32 getLoadFlagForWaveSoundSet_() const;
        u32 getLoadFlagForBank_() const;
        u32 getLoadFlagForWaveArchive_() const;

        void setLoadItemsForSequence_(u32 loadFlags);
        void setLoadItemsForWaveSoundSet_(u32 loadFlags);
        void setLoadItemsForBank_(u32 loadFlags);
        void setLoadItemsForWaveArchive_(u32 loadFlags);

        bool validateSequence_(const Sound& sound, sead::BufferedSafeString& error) const;
        bool validateWaveSoundSet_(const SoundSet& soundSet, sead::BufferedSafeString& error, u32 loadFlags) const;
        bool validateBank_(const Bank& bank, sead::BufferedSafeString& error, u32 loadFlags) const;

    private:
        ItemType mItemRefType;
        ItemReference mItemRef;
        //u32 mLoadFlag;
        u32 mLoadItem;

        friend class Bfsar;
    };

    enum class OutputType
    {
        Embed,
        Link,
        // External
    };

public:
    Group()
        : Item()
        , mOutputType(OutputType::Embed)
        , mItemInfoList()
    {
        mItemType = ItemType::Group;
    }

    OutputType getOutputType() const
    {
        return mOutputType;
    }

    void setOutputType(OutputType outputType)
    {
        mOutputType = outputType;
    }

    ItemInfo::List& getItemInfoList()
    {
        return mItemInfoList;
    }

    const ItemInfo::List& getItemInfoList() const
    {
        return mItemInfoList;
    }

private:
    OutputType mOutputType;
    ItemInfo::List mItemInfoList;

    friend class Bfsar;
};
