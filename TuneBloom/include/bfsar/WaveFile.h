#pragma once

#include <bfsar/Item.h>
#include <bfsar/InnerFile.h>

#include <snd/Global.h>

#include <container/seadObjList.h>

#include <dsp/dsp.h>

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
        struct SeekInfo
        {
            SeekInfo(s16 s1 = 0, s16 s2 = 0)
                : yn1(s1), yn2(s2)
            {
            }

            s16 yn1;
            s16 yn2;
        };

    public:
        Channel()
            : mOwnsData(false)
            , mData(nullptr)
            , mDataSize(0)
            , mOriginalDataOffset(0)
            , mSeekInfo(nullptr)
            , mSeekInfoBlocks(0)
        {
            sead::MemUtil::fillZero(&mAdpcmParam, sizeof(mAdpcmParam));
            sead::MemUtil::fillZero(&mAdpcmLoopParam, sizeof(mAdpcmLoopParam));
            sead::MemUtil::fillZero(&mAdpcmParamStream, sizeof(mAdpcmParamStream));
            sead::MemUtil::fillZero(&mAdpcmLoopParamStream, sizeof(mAdpcmLoopParamStream));
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

        const snd::DspAdpcmParam& getAdpcmParam(bool forStream = false) const
        {
            return forStream ? mAdpcmParamStream : mAdpcmParam;
        }

        const snd::internal::DspAdpcmLoopParam& getAdpcmLoopParam(bool forStream = false) const
        {
            return forStream ? mAdpcmLoopParamStream : mAdpcmLoopParam;

        const SeekInfo& getSeekInfo(u32 blockNo) const
        {
            if (blockNo < mSeekInfoBlocks)
                return mSeekInfo[blockNo];

        }

        {
            return mSeekInfoBlocks;
        }

    private:
        void dispose_();
        void freeSeekInfo_();

    private:
        bool mOwnsData;
        const void* mData;
        u32 mDataSize;
        s32 mOriginalDataOffset;

        snd::DspAdpcmParam mAdpcmParam;
        snd::internal::DspAdpcmLoopParam mAdpcmLoopParam;

        snd::DspAdpcmParam mAdpcmParamStream;
        snd::internal::DspAdpcmLoopParam mAdpcmLoopParamStream;

        SeekInfo* mSeekInfo;
        u32 mSeekInfoBlocks;

        friend class Bfsar;
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
        , mOriginalLoopEndFrame(0)

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

    const Item* validate(sead::BufferedSafeString& error) const override;

    void drawUI() override;

    void setup(sead::Endian::Types endian, u32 version) const
    {
        mEndian = endian;
        mVersion = version;
    }

private:
    void doRead(const void* fileAddr) override;
    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override;

public:
    bool readWavFile(const sead::SafeString& path, Encoding encoding);
    bool writeWavFile(const sead::SafeString& path, s32 channelIdx = -1);

    static bool isOriginalLoopAvailable(u32 version)
    {
        return version >= 0x00010200;
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

    u32 getOriginalLoopEndFrame() const
    {
        if (isOriginalLoopAvailable())
            return mOriginalLoopEndFrame;

        return mLoopEndFrame;
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

    void buildSeekTable_(const void* samples, u32 sampleCount, snd::SampleFormat sampleFormat, Channel& channel);

    void updateLoopInfo_(bool skipStream);

    static void* convert_(
        void* data, sead::Endian::Types dataEndian, u32 samples,
        Encoding from, Encoding to, u32* size,
        ADPCMINFO* adpcmInfo, ADPCMINFO* adpcmInfoStream,
        u32 loopSample, u32 loopSampleStream);

private:
    sead::Endian::Types mDataEndian; //? For when Encoding is Pcm16
    Encoding mEncoding;
    bool mIsLoop;
    u32 mSampleRate;
    u32 mLoopStartFrame;
    u32 mLoopEndFrame;
    u32 mOriginalLoopStartFrame;
    u32 mOriginalLoopEndFrame; //? Only used for streams

    u32 mSampleCount;

    sead::ObjList<Channel> mChannels;

    bool mUseOriginalData;
    void* mOriginalData;
    u32 mOriginalDataSize;

    friend class Bfsar;
    friend class BfwarFile;
};
