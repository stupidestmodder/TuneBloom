#pragma once

#include <bfsar/Item.h>
#include <bfsar/InnerFile.h>

#include <snd/Global.h>

#include <container/seadObjList.h>

class WaveFile : public Item, public InnerFile
{
public:
    enum class Encoding
    {
        Pcm8 = 0,
        Pcm16,
        DspAdpcm,
        //ImaAdpcm
    };

    class Channel
    {
    public:
        Channel()
            : mOwnsData(false)
            , mData(nullptr)
            , mDataSize(0)
            , mOriginalDataOffset(0)
        {
            sead::MemUtil::fillZero(&mAdpcmParam, sizeof(mAdpcmParam));
            sead::MemUtil::fillZero(&mAdpcmLoopParam, sizeof(mAdpcmLoopParam));
        }

        ~Channel();

        const void* getData() const
        {
            return mData;
        }

        u32 getDataSize() const
        {
            return mDataSize;
        }

        const snd::DspAdpcmParam& getAdpcmParam() const
        {
            return mAdpcmParam;
        }

        const snd::internal::DspAdpcmLoopParam& getAdpcmLoopParam() const
        {
            return mAdpcmLoopParam;
        }

    private:
        bool mOwnsData;
        const void* mData;
        u32 mDataSize;
        s32 mOriginalDataOffset;

        snd::DspAdpcmParam mAdpcmParam;
        snd::internal::DspAdpcmLoopParam mAdpcmLoopParam;

        friend class WaveFile;
    };

    static const char* sEncodingTypes[3];

public:
    WaveFile()
        : Item()
        , InnerFile()
        , mDataEndian(sead::Endian::eBig)
        , mEncoding(Encoding::DspAdpcm)
        , mIsLoop(false)
        , mSampleRate(0)
        , mLoopStartFrame(0)
        , mLoopEndFrame(0)
        , mOriginalLoopStartFrame(0)

        , mSampleCount(0)

        , mChannels()

        , mUseOriginalData(false)
        , mOriginalData(nullptr)
        , mOriginalDataSize(0)
    {
        mChannels.allocBuffer(2);

        mItemType = ItemType::WaveFile;
    }

    ~WaveFile() override;

    void drawUI() override;

private:
    void doRead(const void* fileAddr) override;
    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override;

public:
    bool readWavFile(const sead::SafeString& path, Encoding encoding);
    bool writeWavFile(const sead::SafeString& path, s32 channelIdx = -1);

    bool isOriginalLoopAvailable() const
    {
        return mVersion >= 0x00010200;
    }

    sead::Endian::Types getDataEndian() const
    {
        return mDataEndian;
    }

    Encoding getEncoding() const
    {
        return mEncoding;
    }

    bool getIsLoop() const
    {
        return mIsLoop;
    }

    u32 getSampleRate() const
    {
        return mSampleRate;
    }

    u32 getLoopStartFrame() const
    {
        return mLoopStartFrame;
    }

    u32 getLoopEndFrame() const
    {
        return mLoopEndFrame;
    }

    u32 getOriginalLoopStartFrame() const
    {
        if (isOriginalLoopAvailable())
            return mOriginalLoopStartFrame;

        return mLoopStartFrame;
    }

    const sead::ObjList<Channel>& getChannels() const
    {
        return mChannels;
    }

    u32 getSampleCount() const
    {
        return mSampleCount;
    }

private:
    void invalidateOriginalData_()
    {
        if (mOriginalData)
        {
            delete[] mOriginalData;
            mOriginalData = nullptr;
        }

        mUseOriginalData = false;
    }

    void convert_();

private:
    sead::Endian::Types mDataEndian; //? For when Encoding is Pcm16
    Encoding mEncoding;
    bool mIsLoop;
    u32 mSampleRate;
    u32 mLoopStartFrame;
    u32 mLoopEndFrame;
    u32 mOriginalLoopStartFrame;

    u32 mSampleCount;

    sead::ObjList<Channel> mChannels;

    bool mUseOriginalData;
    void* mOriginalData;
    u32 mOriginalDataSize;

    friend class BfwarFile;
};
