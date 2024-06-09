#pragma once

#include <bfsar/Item.h>

class Player : public Item
{
public:
    Player()
        : Item()
        , mPlayableSoundMax(1)
        , mEnablePlayerHeapSize(true)
        , mPlayerHeapSize(0)
    {
        mItemType = ItemType::Player;
    }

    u32 getPlayableSoundMax() const
    {
        return mPlayableSoundMax;
    }

    void setPlayableSoundMax(u32 playableSoundMax)
    {
        playableSoundMax = sead::Mathu::clampMax(playableSoundMax, 255);
        mPlayableSoundMax = playableSoundMax;
    }

    bool isEnablePlayerHeapSize() const
    {
        return mEnablePlayerHeapSize;
    }

    void setEnablePlayerHeapSize(bool enable)
    {
        mEnablePlayerHeapSize = enable;
    }

    u32 getPlayerHeapSize() const
    {
        if (mEnablePlayerHeapSize)
            return mPlayerHeapSize;

        return 0;
    }

    void setPlayerHeapSize(u32 playerHeapSize)
    {
        playerHeapSize = sead::Mathu::clampMax(playerHeapSize, static_cast<u32>(sead::Mathi::maxNumber()));
        mPlayerHeapSize = playerHeapSize;
    }

private:
    u32 mPlayableSoundMax;
    bool mEnablePlayerHeapSize;
    u32 mPlayerHeapSize;

    friend class Bfsar;
};
