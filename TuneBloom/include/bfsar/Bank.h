#pragma once

#include <bfsar/Item.h>
#include <bfsar/IdTable.h>

class Bank : public Item
{
public:
    Bank()
        : Item()
        , mWaveArchiveType(WaveArchiveType::Invalid)
        , mWaveArchiveRef(this)
        , mFile(this)
    {
        mItemType = ItemType::Bank;
    }

    WaveArchiveType getWaveArchiveType() const
    {
        return mWaveArchiveType;
    }

    void setWaveArchiveType(WaveArchiveType warcType)
    {
        mWaveArchiveType = warcType;
    }

    const ItemReference& getWaveArchiveRef() const
    {
        return mWaveArchiveRef;
    }

    ItemReference& getWaveArchiveRef()
    {
        return mWaveArchiveRef;
    }

    const ItemReference& getFile() const
    {
        return mFile;
    }

    ItemReference& getFile()
    {
        return mFile;
    }

private:
    WaveArchiveType mWaveArchiveType;
    ItemReference mWaveArchiveRef;
    ItemReference mFile;

    friend class Bfsar;
};
