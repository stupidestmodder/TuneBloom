#pragma once

#include <bfsar/Item.h>
#include <bfsar/SequenceFile.h>

#include <snd/Global.h>

#include <container/seadObjList.h>

class Sound : public Item
{
public:
    enum class SoundType
    {
        Invalid = 0,

        Seq,
        Strm,
        Wave,
    };

    class Sound3DInfo
    {
    public:
        enum Flags
        {
            Volume   = 1 << 0,
            Priority = 1 << 1,
            Pan      = 1 << 2,
            SPan     = 1 << 3,
            Filter   = 1 << 4
        };

        enum DecayCurve
        {
            Logarithmic = 1,
            Linear
        };

    public:
        Sound3DInfo()
            : mFlags(Volume | Priority | Pan | SPan)
            , mDecayRatio(0.5f)
            , mDecayCurve(DecayCurve::Logarithmic)
            , mDopplerFactor(0)
        {
        }

        u32 getFlags() const
        {
            return mFlags;
        }

        void setFlags(u32 flags)
        {
            mFlags = flags;
        }

        f32 getDecayRatio() const
        {
            return mDecayRatio;
        }

        void setDecayRatio(f32 decayRatio)
        {
            decayRatio = sead::Mathf::clamp2(0.0f, decayRatio, 1.0f);
            mDecayRatio = decayRatio;
        }

        u8 getDecayCurve() const
        {
            return mDecayCurve;
        }

        void setDecayCurve(u8 decayCurve)
        {
            mDecayCurve = decayCurve;
        }

        u8 getDopplerFactor() const
        {
            return mDopplerFactor;
        }

        void setDopplerFactor(u8 dopplerFactor)
        {
            mDopplerFactor = dopplerFactor;
        }

    private:
        u32 mFlags;
        f32 mDecayRatio;
        u8 mDecayCurve;
        u8 mDopplerFactor;

        friend class Bfsar;
    };

    class SequenceSoundInfo
    {
    public:
        SequenceSoundInfo(Sound* owner)
            : mSequenceFileRef(owner)
            , mBankRefs()
            //, mAllocateTrackFlags(0)
            , mEnableStartOffset(true)
            //, mStartOffset(0)
            , mStartLabel()
            , mEnablePriority(true)
            , mChannelPriority(64)
            , mIsReleasePriorityFix(false)
        {
            for (u32 i = 0; i < 4; i++)
            {
                mBankRefs[i] = new ItemReference(owner);
            }
        }

        ~SequenceSoundInfo()
        {
            for (u32 i = 0; i < 4; i++)
            {
                delete mBankRefs[i];
                mBankRefs[i] = nullptr;
            }
        }

        const ItemReference& getSequenceFileRef() const
        {
            return mSequenceFileRef;
        }

        ItemReference& getSequenceFileRef()
        {
            return mSequenceFileRef;
        }

        u32 getBankId(u32 idx) const
        {
            SEAD_ASSERT(idx < 4);
            return (*mBankRefs[idx]).getItemId();
        }

        const ItemReference& getBankRef(u32 idx) const
        {
            SEAD_ASSERT(idx < 4);
            return *mBankRefs[idx];
        }

        ItemReference& getBankRef(u32 idx)
        {
            SEAD_ASSERT(idx < 4);
            return *mBankRefs[idx];
        }

        u32 getAllocateTrackFlags() const
        {
            const Item* item = mSequenceFileRef.getItem();
            if (!item || item->getItemType() != ItemType::SequenceFile)
                return 0;

            const SequenceFile* seqFile = static_cast<const SequenceFile*>(item);
            return seqFile->getLabelAllocTracks(mStartLabel);
        }

        // void setAllocateTrackFlags(u32 flags)
        // {
        //     mAllocateTrackFlags = flags;
        // }

        bool isEnableStartOffset() const
        {
            return mEnableStartOffset;
        }

        void setEnableStartOffset(bool enable)
        {
            mEnableStartOffset = enable;
        }

        u32 getStartOffset() const
        {
            if (!mEnableStartOffset)
                return 0;

            const Item* item = mSequenceFileRef.getItem();
            if (!item || item->getItemType() != ItemType::SequenceFile)
                return 0;

            const SequenceFile* seqFile = static_cast<const SequenceFile*>(item);
            return seqFile->getLabelOffset(mStartLabel);
        }

        // void setStartOffset(u32 startOffset)
        // {
        //     mStartOffset = startOffset;
        // }

        sead::SafeString getStartLabel() const
        {
            if (mEnableStartOffset)
                return mStartLabel;

            return "";
        }

        sead::FixedSafeString<128>& getStartLabel()
        {
            return mStartLabel;
        }

        bool isEnablePriority() const
        {
            return mEnablePriority;
        }

        void setEnablePriority(bool enable)
        {
            mEnablePriority = enable;
        }

        u8 getChannelPriority() const
        {
            if (mEnablePriority)
                return mChannelPriority;

            return 64;
        }

        void setChannelPriority(u8 channelPriority)
        {
            channelPriority = sead::MathCalcCommon<u8>::clampMax(channelPriority, 127);
            mChannelPriority = channelPriority;
        }

        bool getIsReleasePriorityFix() const
        {
            if (mEnablePriority)
                return mIsReleasePriorityFix;

            return true;
        }

        void setIsReleasePriorityFix(bool isReleasePriorityFix)
        {
            mIsReleasePriorityFix = isReleasePriorityFix;
        }

    private:
        ItemReference mSequenceFileRef;

        ItemReference* mBankRefs[4];
        //u32 mAllocateTrackFlags;
        bool mEnableStartOffset;
        //u32 mStartOffset;
        sead::FixedSafeString<128> mStartLabel; // TODO: How big ?
        bool mEnablePriority;
        u8 mChannelPriority;
        bool mIsReleasePriorityFix;

        friend class Bfsar;
    };

    class StreamSoundInfo
    {
    public:
        class Track : public Item
        {
        public:
            Track()
                : Item()
                , mVolume(127)
                , mPan(64)
                , mSPan(0)
                , mFlags(0)
                , mChannels()
                , mMainSend(127)
                , mLpfFreq(64)
                , mBiquadType(0)
                , mBiquadValue(0)
            {
                mItemType = ItemType::StreamTrack;

                mChannels.allocBuffer(2);

                for (u32 i = 0; i < 3; i++)
                {
                    mFxSend[i] = 0;
                }
            }

            ~Track()
            {
                mChannels.freeBuffer();
            }

            void drawUI();

            u8 getVolume() const
            {
                return mVolume;
            }

            void setVolume(u8 volume)
            {
                mVolume = volume;
            }

            u8 getPan() const
            {
                return mPan;
            }

            void setPan(u8 pan)
            {
                mPan = pan;
            }

            u8 getSPan() const
            {
                return mSPan;
            }

            void setSPan(u8 span)
            {
                mSPan = span;
            }

            u8 getFlags() const
            {
                return mFlags;
            }

            void setFlags(u8 flags)
            {
                mFlags = flags;
            }

            const sead::ObjList<u8>& getChannels() const
            {
                return mChannels;
            }

            sead::ObjList<u8>& getChannels()
            {
                return mChannels;
            }

            u8 getMainSend() const
            {
                return mMainSend;
            }

            void setMainSend(u8 mainSend)
            {
                mainSend = sead::MathCalcCommon<u8>::clampMax(mainSend, 127);
                mMainSend = mainSend;
            }

            u8 getFxSend(u32 idx) const
            {
                SEAD_ASSERT(idx < 3);
                return mFxSend[idx];
            }

            void setFxSend(u32 idx, u8 fxSend)
            {
                SEAD_ASSERT(idx < 3);

                fxSend = sead::MathCalcCommon<u8>::clampMax(fxSend, 127);
                mFxSend[idx] = fxSend;
            }

            u8 getLpfFreq() const
            {
                return mLpfFreq;
            }

            void setLpfFreq(u8 lpfFreq)
            {
                mLpfFreq = lpfFreq;
            }

            u8 getBiquadType() const
            {
                return mBiquadType;
            }

            void setBiquadType(u8 biquadType)
            {
                mBiquadType = biquadType;
            }

            u8 getBiquadValue() const
            {
                return mBiquadValue;
            }

            void setBiquadValue(u8 biquadValue)
            {
                mBiquadValue = biquadValue;
            }

        private:
            u8 mVolume;
            u8 mPan;
            u8 mSPan;
            u8 mFlags;
            sead::ObjList<u8> mChannels;
            u8 mMainSend;
            u8 mFxSend[3];
            u8 mLpfFreq;
            u8 mBiquadType;
            u8 mBiquadValue;

            friend class Bfsar;
        };

        enum StreamType
        {
            Invalid = 0,

            NwStreamBinary,
            Adts
        };

    public:
        StreamSoundInfo(Sound* owner)
            : mPath()

            , mAllocateTrackFlags(0)
            , mAllocateChannelCount(0)
            , mTrackList()
            , mPitch(1.0f)
            , mMainSend(127)

            , mEnableStreamSoundExtension(false)
            , mStreamType(StreamType::NwStreamBinary)
            , mIsLoop(false)
            , mLoopStartFrame(0)
            , mLoopEndFrame(0)

            , mPrefetchFileRef(owner)
        {
            for (u32 i = 0; i < 3; i++)
            {
                mFxSend[i] = 0;
            }
        }

        ~StreamSoundInfo()
        {
        }

        const sead::SafeString& getPath() const
        {
            return mPath;
        }

        sead::FixedSafeString<512>& getPath()
        {
            return mPath;
        }

        u16 getAllocateTrackFlags() const
        {
            return mAllocateTrackFlags;
        }

        void setAllocateTrackFlags(u16 flags)
        {
            mAllocateTrackFlags = flags;
        }

        u16 getAllocateChannelCount() const
        {
            return mAllocateChannelCount;
        }

        void setAllocateChannelCount(u16 channelCount)
        {
            channelCount = sead::MathCalcCommon<u16>::clampMax(channelCount, 16);
            mAllocateChannelCount = channelCount;
        }

        const Track::List& getTrackList() const
        {
            return mTrackList;
        }

        Track::List& getTrackList()
        {
            return mTrackList;
        }

        f32 getPitch() const
        {
            return mPitch;
        }

        void setPitch(f32 pitch)
        {
            pitch = sead::Mathf::clamp2(0.0f, pitch, 8.0f);
            mPitch = pitch;
        }

        u8 getMainSend() const
        {
            return mMainSend;
        }

        void setMainSend(u8 mainSend)
        {
            mainSend = sead::MathCalcCommon<u8>::clampMax(mainSend, 127);
            mMainSend = mainSend;
        }

        u8 getFxSend(u32 idx) const
        {
            SEAD_ASSERT(idx < 3);
            return mFxSend[idx];
        }

        void setFxSend(u32 idx, u8 fxSend)
        {
            SEAD_ASSERT(idx < 3);

            fxSend = sead::MathCalcCommon<u8>::clampMax(fxSend, 127);
            mFxSend[idx] = fxSend;
        }

        bool isEnableStreamSoundExtension() const
        {
            return mEnableStreamSoundExtension;
        }

        void setEnableStreamSoundExtension(bool enable)
        {
            mEnableStreamSoundExtension = enable;
        }

        StreamType getStreamType() const
        {
            if (mEnableStreamSoundExtension)
                return mStreamType;

            return StreamType::NwStreamBinary;
        }

        void setStreamType(StreamType type)
        {
            mStreamType = type;
        }

        bool getIsLoop() const
        {
            if (mEnableStreamSoundExtension)
                return mIsLoop;

            return false;
        }

        void setIsLoop(bool isLoop)
        {
            mIsLoop = isLoop;
        }

        u32 getLoopStartFrame() const
        {
            if (mEnableStreamSoundExtension)
                return mLoopStartFrame;

            return 0;
        }

        void setLoopStartFrame(u32 loopStartFrame)
        {
            mLoopStartFrame = loopStartFrame;
        }

        u32 getLoopEndFrame() const
        {
            if (mEnableStreamSoundExtension)
                return mLoopEndFrame;

            return 0;
        }

        void setLoopEndFrame(u32 loopEndFrame)
        {
            mLoopEndFrame = loopEndFrame;
        }

        u32 getPrefetchFileId() const
        {
            return mPrefetchFileRef.getItemId();
        }

        ItemReference& getPrefetchFileRef()
        {
            return mPrefetchFileRef;
        }

    private:
        sead::FixedSafeString<512> mPath; //? Temp solution, will probably not leave like this

        u16 mAllocateTrackFlags;
        u16 mAllocateChannelCount;
        Track::List mTrackList;
        f32 mPitch;
        u8 mMainSend;
        u8 mFxSend[3];

        bool mEnableStreamSoundExtension;
        StreamType mStreamType;
        bool mIsLoop;
        u32 mLoopStartFrame;
        u32 mLoopEndFrame;

        ItemReference mPrefetchFileRef;

        friend class Bfsar;
    };

    class WaveSoundInfo
    {
    public:
        WaveSoundInfo(Sound* owner)
            : mWaveFileRef(owner)

            , mAllocateTrackCount(1)
            , mEnablePriority(true)
            , mChannelPriority(64)
            , mIsReleasePriorityFix(false)

            , mEnablePan(true)
            , mPan(64)
            , mSurroundPan(0)
            , mEnablePitch(true)
            , mPitch(1.0f)
            , mEnableSend(true)
            , mMainSend(127)
            , mEnableEnvelope(true)
            , mAdshrCurve(127, 127, 127, 127, 127)
            , mEnableFilter(true)
            , mLpfFreq(64)
            , mBiquadType(0)
            , mBiquadValue(0)
        {
            for (u32 i = 0; i < 3; i++)
            {
                mFxSend[i] = 0;
            }
        }

        const ItemReference& getWaveFileRef() const
        {
            return mWaveFileRef;
        }

        ItemReference& getWaveFileRef()
        {
            return mWaveFileRef;
        }

        u32 getAllocateTrackCount() const
        {
            return mAllocateTrackCount;
        }

        void setAllocateTrackCount(u32 trackCount)
        {
            mAllocateTrackCount = trackCount;
        }

        bool isEnablePriority() const
        {
            return mEnablePriority;
        }

        void setEnablePriority(bool enable)
        {
            mEnablePriority = enable;
        }

        u8 getChannelPriority() const
        {
            if (mEnablePriority)
                return mChannelPriority;

            return 64;
        }

        void setChannelPriority(u8 channelPriority)
        {
            channelPriority = sead::MathCalcCommon<u8>::clampMax(channelPriority, 127);
            mChannelPriority = channelPriority;
        }

        bool getIsReleasePriorityFix() const
        {
            if (mEnablePriority)
                return mIsReleasePriorityFix;

            return false;
        }

        void setIsReleasePriorityFix(bool isReleasePriorityFix)
        {
            mIsReleasePriorityFix = isReleasePriorityFix;
        }

        bool isEnablePan() const
        {
            return mEnablePan;
        }

        void setEnablePan(bool enable)
        {
            mEnablePan = enable;
        }

        u8 getPan() const
        {
            if (mEnablePan)
                return mPan;

            return 64;
        }

        void setPan(u8 pan)
        {
            pan = sead::MathCalcCommon<u8>::clampMax(pan, 127);
            mPan = pan;
        }

        s8 getSurroundPan() const
        {
            if (mEnablePan)
                return mSurroundPan;

            return 0;
        }

        void setSurroundPan(s8 span)
        {
            span = sead::MathCalcCommon<s8>::clamp2(0, span, 127);
            mSurroundPan = span;
        }

        bool isEnablePitch() const
        {
            return mEnablePitch;
        }

        void setEnablePitch(bool enable)
        {
            mEnablePitch = enable;
        }

        f32 getPitch() const
        {
            if (mEnablePitch)
                return mPitch;

            return 1.0f;
        }

        void setPitch(f32 pitch)
        {
            pitch = sead::Mathf::clamp2(0.0f, pitch, 8.0f);
            mPitch = pitch;
        }

        bool isEnableSend() const
        {
            return mEnableSend;
        }

        void setEnableSend(bool enable)
        {
            mEnableSend = enable;
        }

        u8 getMainSend() const
        {
            if (mEnableSend)
                return mMainSend;

            return 127;
        }

        void setMainSend(u8 mainSend)
        {
            mainSend = sead::MathCalcCommon<u8>::clampMax(mainSend, 127);
            mMainSend = mainSend;
        }

        u8 getFxSend(u32 idx) const
        {
            SEAD_ASSERT(idx < 3);

            if (mEnableSend)
                return mFxSend[idx];

            return 0;
        }

        void setFxSend(u32 idx, u8 fxSend)
        {
            SEAD_ASSERT(idx < 3);

            fxSend = sead::MathCalcCommon<u8>::clampMax(fxSend, 127);
            mFxSend[idx] = fxSend;
        }

        bool isEnableEnvelope() const
        {
            return mEnableEnvelope;
        }

        void setEnableEnvelope(bool enable)
        {
            mEnableEnvelope = enable;
        }

        const snd::AdshrCurve& getAdshrCurve() const
        {
            if (mEnableEnvelope)
                return mAdshrCurve;

            static const snd::AdshrCurve cNullAdshrCurve(127, 127, 127, 127, 127);
            return cNullAdshrCurve;
        }

        void setAdshrCurve(const snd::AdshrCurve& curve)
        {
            mAdshrCurve = curve;

            mAdshrCurve.attack = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.attack, 127);
            mAdshrCurve.decay = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.decay, 127);
            mAdshrCurve.sustain = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.sustain, 127);
            mAdshrCurve.hold = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.hold, 127);
            mAdshrCurve.release = sead::MathCalcCommon<u8>::clampMax(mAdshrCurve.release, 127);
        }

        bool isEnableFilter() const
        {
            return mEnableFilter;
        }

        void setEnableFilter(bool enable)
        {
            mEnableFilter = enable;
        }

        u8 getLpfFreq() const
        {
            if (mEnableFilter)
                return mLpfFreq;

            return 64;
        }

        void setLpfFreq(u8 freq)
        {
            freq = sead::MathCalcCommon<u8>::clampMax(freq, 64);
            mLpfFreq = freq;
        }

        u8 getBiquadType() const
        {
            if (mEnableFilter)
                return mBiquadType;

            return 0;
        }

        void setBiquadType(u8 biquadType)
        {
            mBiquadType = biquadType;
        }

        u8 getBiquadValue() const
        {
            if (mEnableFilter)
                return mBiquadValue;

            return 0;
        }

        void setBiquadValue(u8 biquadValue)
        {
            biquadValue = sead::MathCalcCommon<u8>::clampMax(biquadValue, 127);
            mBiquadValue = biquadValue;
        }

    private:
        ItemReference mWaveFileRef;

        u32 mAllocateTrackCount;
        bool mEnablePriority;
        u8 mChannelPriority;
        bool mIsReleasePriorityFix;

        bool mEnablePan;
        u8 mPan;
        s8 mSurroundPan;
        bool mEnablePitch;
        f32 mPitch;
        bool mEnableSend;
        u8 mMainSend;
        u8 mFxSend[3];
        bool mEnableEnvelope;
        snd::AdshrCurve mAdshrCurve;
        bool mEnableFilter;
        u8 mLpfFreq;
        u8 mBiquadType;
        u8 mBiquadValue;

        friend class Bfsar;
    };

public:
    Sound()
        : Item()
        , mPlayerRef(this)
        , mVolume(96)
        , mRemoteFilter(0)
        , mSoundType(SoundType::Wave)
        , mEnablePanParam(true)
        , mPanMode(snd::PanMode::Dual)
        , mPanCurve(snd::PanCurve::Sqrt)
        , mEnablePlayerParam(true)
        , mPlayerPriority(64)
        , mActorPlayerId(0)

        , mEnableIsFrontBypass(true)
        , mIsFrontBypass(false)
        , mEnableSound3DInfo(true)
        , mSound3DInfo()

        , mSequenceSoundInfo(this)
        , mStreamSoundInfo(this)
        , mWaveSoundInfo(this)
    {
        mItemType = ItemType::Sound;

        for (u32 i = 0; i < 4; i++)
        {
            mEnableUserParam[i] = false;
            mUserParam[i] = 0;
        }

        mEnableUserParam[0] = true;
    }

    u32 getPlayerId() const
    {
        return mPlayerRef.getItemId();
    }

    ItemReference& getPlayerRef()
    {
        return mPlayerRef;
    }

    u8 getVolume() const
    {
        return mVolume;
    }

    void setVolume(u8 volume)
    {
        mVolume = volume;
    }

    u8 getRemoteFilter() const
    {
        return mRemoteFilter;
    }

    void setRemoteFilter(u8 filter)
    {
        filter = sead::MathCalcCommon<u8>::clampMax(filter, 127);
        mRemoteFilter = filter;
    }

    SoundType getSoundType() const
    {
        return mSoundType;
    }

    void setSoundType(SoundType type)
    {
        mSoundType = type;
    }

    bool isEnablePanParam() const
    {
        return mEnablePanParam;
    }

    void setEnablePanParam(bool enable)
    {
        mEnablePanParam = enable;
    }

    snd::PanMode getPanMode() const
    {
        if (mEnablePanParam)
            return mPanMode;

        return snd::PanMode::Dual;
    }

    void setPanMode(snd::PanMode mode)
    {
        mPanMode = mode;
    }

    snd::PanCurve getPanCurve() const
    {
        if (mEnablePanParam)
            return mPanCurve;

        return snd::PanCurve::Sqrt;
    }

    void setPanCurve(snd::PanCurve curve)
    {
        mPanCurve = curve;
    }

    bool isEnablePlayerParam() const
    {
        return mEnablePlayerParam;
    }

    void setEnablePlayerParam(bool enable)
    {
        mEnablePlayerParam = enable;
    }

    u8 getPlayerPriority() const
    {
        if (mEnablePlayerParam)
            return mPlayerPriority;

        return 64;
    }

    void setPlayerPriority(u8 playerPriority)
    {
        playerPriority = sead::MathCalcCommon<u8>::clampMax(playerPriority, 127);
        mPlayerPriority = playerPriority;
    }

    u8 getActorPlayerId() const
    {
        if (mEnablePlayerParam)
            return mActorPlayerId;

        return 0;
    }

    void setActorPlayerId(u8 actorPlayerId)
    {
        actorPlayerId = sead::MathCalcCommon<u8>::clampMax(actorPlayerId, 3);
        mActorPlayerId = actorPlayerId;
    }

    bool isEnableUserParam(u32 idx) const
    {
        SEAD_ASSERT(idx < 4);
        return mEnableUserParam[idx];
    }

    void setEnableUserParam(u32 idx, bool enable)
    {
        SEAD_ASSERT(idx < 4);
        mEnableUserParam[idx] = enable;
    }

    u32 getUserParam(u32 idx) const
    {
        SEAD_ASSERT(idx < 4);

        if (mEnableUserParam[idx])
            return mUserParam[idx];

        return 0xFFFFFFFF;
    }

    void setUserParam(u32 idx, u32 userParam)
    {
        SEAD_ASSERT(idx < 4);
        mUserParam[idx] = userParam;
    }

    bool isEnableIsFrontBypass() const
    {
        return mEnableIsFrontBypass;
    }

    void setEnableIsFrontBypass(bool enable)
    {
        mEnableIsFrontBypass = enable;
    }

    bool getIsFrontBypass() const
    {
        if (mEnableIsFrontBypass)
            return mIsFrontBypass;

        return false;
    }

    void setIsFrontBypass(bool isFrontBypass)
    {
        mIsFrontBypass = isFrontBypass;
    }

    bool isEnableSound3DInfo() const
    {
        return mEnableSound3DInfo;
    }

    void setEnableSound3DInfo(bool enable)
    {
        mEnableSound3DInfo = enable;
    }

    const Sound3DInfo& getSound3DInfo() const
    {
        return mSound3DInfo;
    }

    Sound3DInfo& getSound3DInfo()
    {
        return mSound3DInfo;
    }

    const SequenceSoundInfo& getSequenceSoundInfo() const
    {
        return mSequenceSoundInfo;
    }

    SequenceSoundInfo& getSequenceSoundInfo()
    {
        return mSequenceSoundInfo;
    }

    const StreamSoundInfo& getStreamSoundInfo() const
    {
        return mStreamSoundInfo;
    }

    StreamSoundInfo& getStreamSoundInfo()
    {
        return mStreamSoundInfo;
    }

    const WaveSoundInfo& getWaveSoundInfo() const
    {
        return mWaveSoundInfo;
    }

    WaveSoundInfo& getWaveSoundInfo()
    {
        return mWaveSoundInfo;
    }

private:
    ItemReference mPlayerRef;
    u8 mVolume;
    u8 mRemoteFilter;
    SoundType mSoundType;
    bool mEnablePanParam;
    snd::PanMode mPanMode;
    snd::PanCurve mPanCurve;
    bool mEnablePlayerParam;
    u8 mPlayerPriority;
    u8 mActorPlayerId;
    bool mEnableUserParam[4];
    u32 mUserParam[4];
    bool mEnableIsFrontBypass;
    bool mIsFrontBypass;
    bool mEnableSound3DInfo;
    Sound3DInfo mSound3DInfo;

    SequenceSoundInfo mSequenceSoundInfo;

    StreamSoundInfo mStreamSoundInfo;

    WaveSoundInfo mWaveSoundInfo;

    friend class Bfsar;
};
