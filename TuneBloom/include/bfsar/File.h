#pragma once

#include <bfsar/Item.h>
#include <bfsar/InnerFile.h>
#include <bfsar/WaveFile.h>

class BfwsdFile : public InnerFile
{
    SEAD_RTTI_OVERRIDE(BfwsdFile, InnerFile);

public:
    BfwsdFile()
        : InnerFile()
    {
    }

    void drawUI() override;

private:
    void doRead(const void* fileAddr) override;
};

class BfwarFile : public InnerFile
{
    SEAD_RTTI_OVERRIDE(BfwarFile, InnerFile);

public:
    BfwarFile()
        : InnerFile()
        , mWaveList()
    {
    }

    void drawUI() override;

private:
    void doRead(const void* fileAddr) override;
    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override;

public:
    s32 getWaveCount() const
    {
        return mWaveList.size();
    }

private:
    WaveFile::List mWaveList;
};
