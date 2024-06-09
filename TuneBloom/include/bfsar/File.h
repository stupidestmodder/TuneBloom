#pragma once

#include <bfsar/Item.h>
#include <bfsar/IdTable.h>
#include <bfsar/InnerFile.h>
#include <bfsar/WaveFile.h>

#include <memory>

class UnknownFile : public InnerFile
{
    SEAD_RTTI_OVERRIDE(UnknownFile, InnerFile);

public:
    UnknownFile(u32 fileId, u32 fileSize)
        : InnerFile(fileId)
        , mFileData(nullptr)
        , mFileSize(fileSize)
    {
    }

    ~UnknownFile() override
    {
        if (mFileData)
        {
            delete[] mFileData;
            mFileData = nullptr;
        }
    }

private:
    void doRead(const void* fileAddr) override
    {
        if (mFileSize == 0xFFFFFFFF)
        {
            const nw::ut::BinaryFileHeader& header = *static_cast<const nw::ut::BinaryFileHeader*>(fileAddr);

            mFileSize = header.fileSize;
        }

        mFileData = new u8[mFileSize];
        sead::MemUtil::copy(mFileData, fileAddr, mFileSize);
    }

    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override
    {
        SEAD_ASSERT(mFileData);
        SEAD_ASSERT(mFileSize != 0);
        SEAD_ASSERT(mFileSize != 0xFFFFFFFF);

        stream->writeMemBlock(mFileData, mFileSize);

        return mFileSize;
    }

public:
    const void* getFileData() const
    {
        return mFileData;
    }

    u32 getFileSize() const
    {
        return mFileSize;
    }

private:
    void* mFileData;
    u32 mFileSize;
};

class BfwsdFile : public InnerFile
{
    SEAD_RTTI_OVERRIDE(BfwsdFile, InnerFile);

public:
    BfwsdFile(u32 fileId)
        : InnerFile(fileId)
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
    BfwarFile(u32 fileId)
        : InnerFile(fileId)
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

class BfgrpFile : public InnerFile
{
    SEAD_RTTI_OVERRIDE(BfgrpFile, InnerFile);

public:
    class ItemInfo : public Item
    {
    public:
        enum class EmbedType
        {
            Embed,
            Link
        };

    public:
        ItemInfo()
            : Item()
            , mFileId(cInvalidId)
            , mEmbedType(EmbedType::Embed)
        {
            mItemType = ItemType::GroupItemInfo;
        };

        void updateName();
        void drawUI();

        EmbedType getEmbedType() const
        {
            return mEmbedType;
        }

        void setEmbedType(EmbedType embedType)
        {
            mEmbedType = embedType;
        }

    private:
        u32 mFileId;
        EmbedType mEmbedType;

        friend class BfgrpFile;
    };

    class ItemInfoEx : public Item
    {
    public:    
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
        ItemInfoEx()
            : Item()
            , mItemIdType(ItemType::Invalid)
            , mItemId(cInvalidId)
            , mLoadFlag(0)

            , mItemRef(this)
        {
            mItemType = ItemType::GroupItemInfoEx;
        };

        void updateName();
        void drawUI();

        void setItemIdType(ItemType itemIdType)
        {
            mItemIdType = itemIdType;
        }

        const ItemReference& getItemRef() const
        {
            return mItemRef;
        }

        ItemReference& getItemRef()
        {
            return mItemRef;
        }

        static const List& getItemList(ItemType itemType);

    private:
        ItemType mItemIdType;
        u32 mItemId;
        u32 mLoadFlag;

        ItemReference mItemRef;

        friend class BfgrpFile;
    };

public:
    BfgrpFile(u32 fileId)
        : InnerFile(fileId)
        , mItemInfoList()
        , mItemInfoExList()
    {
    }

    void drawUI() override;
    void postInit() override;

private:
    void doRead(const void* fileAddr) override;
    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override;

private:
    ItemInfo::List mItemInfoList;
    ItemInfoEx::List mItemInfoExList;
};
