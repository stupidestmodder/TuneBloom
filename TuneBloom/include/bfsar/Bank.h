#pragma once

#include <bfsar/Item.h>

class Bank : public Item
{
public:
    Bank()
        : Item()
        , mWaveArchiveType(WaveArchiveType::Invalid)
        , mWaveArchiveRef(this)
        , mFileRef(this)
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

    const ItemReference& getFileRef() const
    {
        return mFileRef;
    }

    ItemReference& getFileRef()
    {
        return mFileRef;
    }

private:
    WaveArchiveType mWaveArchiveType;
    ItemReference mWaveArchiveRef;
    ItemReference mFileRef;

    friend class Bfsar;
};
