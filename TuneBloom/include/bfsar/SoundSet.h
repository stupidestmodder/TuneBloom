#pragma once

#include <bfsar/Item.h>

class SoundSet : public Item
{
public:
    enum class SoundSetType
    {
        Wave,
        Seq
    };

public:
    SoundSet()
        : Item()
        , mSoundSetType(SoundSetType::Wave)
        , mStartId(0)
        , mEndId(0)
        , mWaveArchiveType(WaveArchiveType::Invalid)
        , mWaveArchiveRef(this)
    {
        mItemType = ItemType::SoundSet;

        mWaveArchiveRef.setOnDetachCallback<SoundSet>(&SoundSet::onDetachWaveArchive_); // Temp ? Idk a good solution yet
    }

    SoundSetType getSoundSetType() const
    {
        return mSoundSetType;
    }

    void setSoundSetType(SoundSetType type)
    {
        mSoundSetType = type;
    }

    u32 getStartId() const
    {
        return mStartId;
    }

    void setStartId(u32 startId)
    {
        mStartId = startId;
    }

    u32 getEndId() const
    {
        return mEndId;
    }

    void setEndId(u32 endId)
    {
        mEndId = endId;
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

private:
    void onDetachWaveArchive_(Item* item)
    {
        mWaveArchiveType = WaveArchiveType::AutomaticShared;
    }

private:
    SoundSetType mSoundSetType;
    u32 mStartId;
    u32 mEndId;
    WaveArchiveType mWaveArchiveType;
    ItemReference mWaveArchiveRef;

    friend class Bfsar;
};
