#pragma once

#include <bfsar/Item.h>
#include <bfsar/InnerFile.h>

#include <snd/Global.h>

#include <container/seadObjList.h>

#include <dsp/dsp.h>

class Sound;

void FillAdpcmInfo(ADPCMINFO* adpcmInfo, const snd::DspAdpcmParam& param, const snd::internal::DspAdpcmLoopParam& loopParam);
void FillAdpcmParam(snd::DspAdpcmParam* param, snd::internal::DspAdpcmLoopParam* loopParam, const ADPCMINFO& adpcmInfo);
void FillAdpcmParam(snd::AdpcmParam* param, const ADPCMINFO& adpcmInfo);

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
            , mFullData(nullptr)
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
        }

        const SeekInfo& getSeekInfo(u32 blockNo) const
        {
            if (blockNo < mSeekInfoBlocks)
                return mSeekInfo[blockNo];

            static const SeekInfo cNullSeekInfo;
            return cNullSeekInfo;
        }

        u32 getSeekInfoBlocks() const
        {
            return mSeekInfoBlocks;
        }

        const void* getFullData_() const
        {
            return mFullData;
        }

        void setFullData_(const void* fullData)
        {
            mFullData = fullData;
        }

    private:
        void dispose_();
        void freeSeekInfo_();

    private:
        bool mOwnsData;
        const void* mData;
        u32 mDataSize;
        s32 mOriginalDataOffset;

        const void* mFullData; // For when mLoopEndFrame < mSampleCount, to keep the full data alive for loop editing (Not spooled !)

        snd::DspAdpcmParam mAdpcmParam;
        snd::internal::DspAdpcmLoopParam mAdpcmLoopParam;

        snd::DspAdpcmParam mAdpcmParamStream;
        snd::internal::DspAdpcmLoopParam mAdpcmLoopParamStream;

        SeekInfo* mSeekInfo;
        u32 mSeekInfoBlocks;

        friend class Bfsar;
        friend class WaveFile;

        friend bool ReadStreamWaves(Sound* sound, const void* strmFile);
    };

    struct RiffWaveInfo
    {
        RiffWaveInfo()
        {
            clear();
        }

        void clear()
        {
            isValid = false;
            path.clear();
            endian = sead::Endian::eLittle;
            fileSize = 0;
            chunkSize = 0;
            format = 0;
            numChannels = 0;
            sampleRate = 0;
            byteRate = 0;
            blockAlign = 0;
            bitsPerSample = 0;
            sampleBytes = 0;
            sampleCount = 0;
            sampleFormat = snd::SampleFormat::PcmS16;
            dataStart = 0;
            isLoop = false;
            loopStartFrame = 0;
            loopEndFrame = 0;
        }

        bool isValid;

        // In
        sead::FixedSafeString<512> path;

        // Out
        sead::Endian::Types endian;
        u32 fileSize;
        u32 chunkSize;
        u16 format;
        u16 numChannels;
        u32 sampleRate;
        u32 byteRate;
        u16 blockAlign;
        u16 bitsPerSample;
        u32 sampleBytes;
        u32 sampleCount;
        snd::SampleFormat sampleFormat;
        u32 dataStart;
        bool isLoop;
        u32 loopStartFrame;
        u32 loopEndFrame;
    };

    static const char* sEncodingTypes[3];

    static const u32 cSamplesPerFrame = 14;
    static const u32 cStreamLoopStartAlignmentFramesAdpcm = 0x3800;
    static const u32 cStreamMinimumLoopFrames = 0x480;

public:
    WaveFile()
        : Item()
        , InnerFile()
        , mDataEndian(sead::Endian::eBig)
        , mEncoding(Encoding::DspAdpcm)
        , mIsLoop(false)
        , mIsLoopDirty(false)
        , mSampleRate(0)
        , mLoopStartFrame(0)
        , mLoopEndFrame(0)

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
    bool doRead(const void* fileAddr) override;
    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override;

public:
    static bool readRiffWavInfo(RiffWaveInfo* out);

    bool readWavFile(const RiffWaveInfo& info, Encoding encoding);
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

    bool getIsLoopDirty() const
    {
        return mIsLoopDirty;
    }

    u32 getSampleRate() const
    {
        return mSampleRate;
    }

    u32 getLoopStartFrame(bool forStream) const
    {
        return getOriginalLoopStartFrame() + getSpoolFrames(getLoopStartAlignmentFrames(forStream));
    }

    u32 getLoopEndFrame(bool forStream) const
    {
        u32 loopFrames = getLoopFrames();
        u32 endFrame = getLoopStartFrame(forStream) + loopFrames;
        if (forStream && cStreamMinimumLoopFrames > loopFrames && loopFrames != 0)
            endFrame += loopFrames * (cStreamMinimumLoopFrames / loopFrames);

        return endFrame;
    }

    // u32 getMaxRealFrame(bool forStream) const
    // {
    //     u32 loopFrames = getSampleCount() - getOriginalLoopStartFrame();
    //     u32 endFrame = getLoopStartFrame(forStream) + loopFrames;
    //     if (forStream && cStreamMinimumLoopFrames > loopFrames && loopFrames != 0)
    //         endFrame += loopFrames * (cStreamMinimumLoopFrames / loopFrames);
    //
    //     return endFrame;
    // }

    u32 getOriginalLoopStartFrame() const
    {
        if (getIsLoop())
            return mLoopStartFrame;

        return 0;
    }

    u32 getOriginalLoopEndFrame() const
    {
        return mLoopEndFrame;
    }

    u32 getLoopFrames() const
    {
        return getOriginalLoopEndFrame() - getOriginalLoopStartFrame();
    }

    u32 getSpoolFrames(u32 loopStartAlignmentFrames) const
    {
        if (loopStartAlignmentFrames > 0)
        {
            u32 skipped = getOriginalLoopStartFrame() % loopStartAlignmentFrames;
            if (skipped > 0)
                return loopStartAlignmentFrames - skipped;
        }
        
        return 0;
    }

    u32 getLoopStartAlignmentFrames(bool forStream) const
    {
        switch (mEncoding)
        {
            case Encoding::DspAdpcm:
                return forStream ? cStreamLoopStartAlignmentFramesAdpcm : cSamplesPerFrame;
            case Encoding::Pcm16:
                return forStream ? 8192 / sizeof(s16) : 0;
            case Encoding::Pcm8:
                return forStream ? 8192 / sizeof(u8) : 0;
        }

        return 0;
    }

    const sead::ObjList<Channel>& getChannels() const
    {
        return mChannels;
    }

    u32 getSampleCount() const
    {
        return mSampleCount;
    }

    void updateLoop();

    static void buildSeekTable_(const void* samples, u32 sampleCount, snd::SampleFormat sampleFormat, Channel& channel);

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

    void disposeChannels_();
    void updateLoopInfo_(bool update, bool updateStream);
    void rebuildSpooledData_();
    void* convertChannel_(Channel& channel, const void* data, sead::Endian::Types dataEndian, Encoding from, Encoding to, u32* size);

private:
    sead::Endian::Types mDataEndian; //? For when Encoding is Pcm16
    Encoding mEncoding;
    bool mIsLoop;
    bool mIsLoopDirty;
    u32 mSampleRate;
    u32 mLoopStartFrame;
    u32 mLoopEndFrame;

    u32 mSampleCount;

    sead::ObjList<Channel> mChannels;

    bool mUseOriginalData;
    void* mOriginalData;
    u32 mOriginalDataSize;

    friend class Bfsar;
    friend class BfwarFile;

    friend bool ReadStreamWaves(Sound* sound, const void* strmFile);
};
