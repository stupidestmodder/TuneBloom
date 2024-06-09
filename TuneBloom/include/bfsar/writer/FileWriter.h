#pragma once

#include <container/seadObjList.h>
#include <stream/seadFileDeviceStream.h>

#include <stack>
#include <string>
#include <unordered_map>

class FileWriter
{
public:
    struct Block
    {
        Block()
            : type(0)
            , size(0)
        {
        }

        u16 type;
        u32 size;
    };

    struct Reference
    {
        Reference()
            : pos(0)
            , offset(0)
        {
        }

        Reference(u32 pos_, u32 offset_)
            : pos(pos_)
            , offset(offset_)
        {
        }

        u32 pos;
        u32 offset;
    };

    struct ReferenceTable
    {
        struct Reference
        {
            Reference(u16 type_, s32 offset_, u32 size_)
                : type(type_)
                , offset(offset_)
                , size(size_)
            {
            }

            u16 type;
            s32 offset;
            u32 size;
        };

        ReferenceTable()
            : count(0)
            , pos(0)
            , offset(0)
            , references()
        {
        }

        ReferenceTable(u32 count_, u32 pos_, u32 offset_)
            : count(count_)
            , pos(pos_)
            , offset(offset_)
            , references()
        {
        }

        void add(u16 type, s32 refOffset, u32 size = 0)
        {
            if (references.size() >= count)
            {
                SEAD_ASSERT_MSG(false, "too much references");
                return;
            }

            references.push_back(Reference(type, refOffset, size));
        }

        void adjustSize(u32 size)
        {
            SEAD_ASSERT(references.size() > 0);

            references.back().size = size;
        }

        u32 count;
        u32 pos;
        u32 offset;

        std::vector<Reference> references;
    };

public:
    FileWriter(sead::FileHandle* handle, sead::WriteStream* stream)
        : mHandle(handle)
        , mStream(stream)
        , mFilePos(0)
        , mHeaderPos(0)
        , mHeaderSize(0)
        , mBlocks()
        , mBlockOpen(false)
        , mBlockPos(0)
        , mOffsetBases()
        , mOffsetBase(0)
        , mReferences()
        , mSizedReferences()
        , mReferenceTables()
        , mSizedReferenceTables()
    {
        SEAD_ASSERT(mHandle);
        SEAD_ASSERT(mStream);

        mFilePos = mHandle->getCurrentSeekPos();
    }

    ~FileWriter()
    {
        mBlocks.freeBuffer();
    }

    u32 getPosition() const
    {
        return mHandle->getCurrentSeekPos() - mFilePos;
    }

    void seek(u32 offset, sead::FileDevice::SeekOrigin origin = sead::FileDevice::SeekOrigin::eBegin)
    {
        mHandle->seek(offset + mFilePos, origin);
    }

    void openFile(const sead::SafeString& magic, u32 numBlocks, u32 version);
    void closeFile();

    void openBlock(u16 typeId, const sead::SafeString& magic);
    void closeBlock();
    void nullBlock(u16 typeId);
    void align(u32 alignment);

    void openReference(const sead::SafeString& name);
    void closeReference(const sead::SafeString& name, u16 typeId);
    void closeReference(const sead::SafeString& name, u16 typeId, s32 offset);
    void closeNullReference(const sead::SafeString& name);

    void openSizedReference(const sead::SafeString& name);
    void closeSizedReference(const sead::SafeString& name, u16 typeId, u32 size);
    void closeSizedReference(const sead::SafeString& name, u16 typeId, s32 offset, u32 size);
    void closeNullSizedReference(const sead::SafeString& name);

    bool hasSizedReference(const sead::SafeString& name) const
    {
        return mSizedReferences.contains(name.cstr());
    }

    void pushOffsetBase();
    void popOffsetBase();

    void openReferenceTable(const sead::SafeString& name, u32 count);
    void closeReferenceTable(const sead::SafeString& name);
    void addReferenceTableReference(const sead::SafeString& name, u16 typeId);

    void openSizedReferenceTable(const sead::SafeString& name, u32 count);
    void closeSizedReferenceTable(const sead::SafeString& name);
    void addSizedReferenceTableReference(const sead::SafeString& name, u16 typeId, u32 size);
    void setSizedReferenceTableReferenceSize(const sead::SafeString& name, u32 size);

    sead::FileHandle* mHandle;
    sead::WriteStream* mStream;

    u32 mFilePos;

    u32 mHeaderPos;
    u16 mHeaderSize;

    sead::ObjList<Block> mBlocks;
    bool mBlockOpen;
    u32 mBlockPos;

    std::stack<u32> mOffsetBases;
    u32 mOffsetBase;

    std::unordered_map<std::string, Reference> mReferences;
    std::unordered_map<std::string, Reference> mSizedReferences;

    std::unordered_map<std::string, ReferenceTable> mReferenceTables;
    std::unordered_map<std::string, ReferenceTable> mSizedReferenceTables;
};
