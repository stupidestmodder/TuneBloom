#pragma once

#include <bfsar/Item.h>

class Group : public Item
{
public:
    class ItemInfo : public Item
    {
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

    public:
        ItemInfo(Group* owner)
            : Item()
            , mItemRefType(ItemType::Invalid)
            , mItemRef(owner)
            , mLoadFlag(LoadFlag::LoadAll)
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

        ItemType getItemRefType() const
        {
            return mItemRefType;
        }

        const ItemReference& getItemRef() const
        {
            return mItemRef;
        }

        ItemReference& getItemRef()
        {
            return mItemRef;
        }

        u32 getLoadFlag() const
        {
            return mLoadFlag;
        }

    private:
        ItemType mItemRefType;
        ItemReference mItemRef;
        u32 mLoadFlag;

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
