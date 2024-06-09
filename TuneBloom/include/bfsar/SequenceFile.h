#pragma once

#include <bfsar/Item.h>

class SequenceFile : public Item
{
public:
    SequenceFile()
        : Item()
        , mData(nullptr)
    {
        mItemType = ItemType::SequenceFile;
    }

    ~SequenceFile()
    {
        if (mData)
        {
            delete[] mData;
            mData = nullptr;
        }
    }

    void read(const void* fileData, u32 fileSize)
    {
        SEAD_ASSERT(!mData);

        mData = new u8[fileSize];
        sead::MemUtil::copy(mData, fileData, fileSize);
    }

    const u8* getData() const
    {
        return mData;
    }

private:
    u8* mData;
};
