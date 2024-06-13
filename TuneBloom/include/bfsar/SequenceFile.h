#pragma once

#include <bfsar/Item.h>
#include <bfsar/InnerFile.h>

class SequenceFile : public Item, public InnerFile
{
public:
    SequenceFile()
        : Item()
        , InnerFile(cInvalidId)
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

    void drawUI() override
    {
        InnerFile::drawUI();
    }

private:
    void doRead(const void* fileAddr) override
    {
    }

    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override
    {
        SEAD_ASSERT(false);
        return 0;
    }

public:
    void read(const void* fileAddr, u32 fileSize)
    {
        SEAD_ASSERT(!mData);

        mData = new u8[fileSize];
        sead::MemUtil::copy(mData, fileAddr, fileSize);

        InnerFile::read(fileAddr);
    }

    const u8* getData() const
    {
        return mData;
    }

private:
    u8* mData;
};
