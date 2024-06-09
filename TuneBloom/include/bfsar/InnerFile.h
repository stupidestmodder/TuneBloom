#pragma once

#include <bfsar/writer/FileWriter.h>

#include <snd/ut/ut_BinaryFileFormat.h>

#include <prim/seadPtrUtil.h>
#include <prim/seadRuntimeTypeInfo.h>

class InnerFile
{
    SEAD_RTTI_BASE(InnerFile);

public:
    InnerFile(u32 fileId)
        : mFileId(fileId)

        , mEndian(sead::Endian::eBig)
        , mVersion(0x00010000)

        , mWritePos(0xFFFFFFFF)
        , mWriteSize(0xFFFFFFFF)
    {
        //SEAD_ASSERT(mFileId != 0xFFFFFFFF);
    }

    virtual ~InnerFile()
    {
    }

    virtual void read(const void* fileAddr)
    {
        const nw::ut::BinaryFileHeader& header = *static_cast<const nw::ut::BinaryFileHeader*>(fileAddr);

        mEndian = nw::ut::GetFileEndian(header);
        
        sead::Endian::Types prevEndian = sFileEndian;
        sFileEndian = mEndian;

        mVersion = header.version;

        doRead(fileAddr);

        sFileEndian = prevEndian;
    }

    virtual u32 write(sead::FileHandle* handle, sead::WriteStream* stream, sead::Endian::Types prevEndian, bool isLast) const
    {
        stream->setBinaryEndian(mEndian);

        if (mWritePos == 0xFFFFFFFF)
        {
            mWritePos = handle->getCurrentSeekPos();
        }

        u32 fileSize = doWrite(handle, stream, isLast);

        if (mWriteSize == 0xFFFFFFFF)
        {
            mWriteSize = handle->getCurrentSeekPos() - mWritePos;
        }

        stream->setBinaryEndian(prevEndian);

        return fileSize;
    }

    virtual void drawUI();

    virtual void postInit()
    {
    }

    u32 getWritePos() const
    {
        return mWritePos;
    }

    u32 getWriteSize() const
    {
        return mWriteSize;
    }

    void clearWriteInfo()
    {
        mWritePos = 0xFFFFFFFF;
        mWriteSize = 0xFFFFFFFF;
    }

protected:
    virtual void doRead(const void* fileAddr) = 0;
    virtual u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const = 0;

protected:
    u32 mFileId;

    sead::Endian::Types mEndian;
    u32 mVersion;

    mutable u32 mWritePos;
    mutable u32 mWriteSize;

    friend class File;
};
