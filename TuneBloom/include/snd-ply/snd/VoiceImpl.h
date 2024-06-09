#pragma once

#include "HardwareMgr.h"

#include <thread/seadAtomic.h>
#include <container/seadOffsetList.h>

#include "LFRingBuffer.h"

namespace snd { namespace internal {

class Voice;

struct MixWaveBuffer
{
    f32* left;
    f32* right;

    u32 sampleCount;
    u32 channelCount;
};

struct VoiceSynthesizeBuffer
{
    enum
    {
        Flag_L = 1 << 0,
        Flag_R = 1 << 1
    };

    static const u32 cSamplePerFrame = driver::HardwareMgr::cSamplePerFrame;
    static const u32 cChannelCount = driver::HardwareMgr::cChannelCount;

    VoiceSynthesizeBuffer();

    void initialize(sead::Heap* heap);
    void finalize();

    u8* buffer;

    u32 flag;
    u32 sampleCount;

    MixWaveBuffer mix;

    //Event event;
    sead::AtomicU32 ready;

    static u32 getSynthesizeBufferSize()
    {
        return cSamplePerFrame * cChannelCount * sizeof(f32);
    }
};

class VoiceMgr;

class VoiceImpl
{
    SEAD_NO_COPY(VoiceImpl);

public:
    typedef void (*DisposeCallbackFunc)(VoiceImpl* voice, void* arg);

    static const u32 cSampleRate = driver::HardwareMgr::cSampleRate;
    static const u32 cSamplePerFrame = driver::HardwareMgr::cSamplePerFrame;

public:
    VoiceImpl();
    ~VoiceImpl();

    void free();
    void freeAllWaveBuffer();

    bool isAvailable() const
    {
        return mIsAppendedToList;
    }

    void appendWaveBuffer(WaveBuffer* waveBuffer);

    void setPriority(u32 priority);

    u32 getPlayPosition() const
    {
        return mPlayPosition;
    }

    void setState(VoiceState state)
    {
        mState = state;
    }

    void setSampleFormat(SampleFormat format)
    {
        mSampleFormat = format;
    }

    void setSampleRate(u32 sampleRate)
    {
        mSampleRate = sampleRate;
    }

    void setAdpcmParam(const AdpcmParam& param)
    {
        mAdpcmParam = param;
    }

    void setVoiceParam(const VoiceParam& param)
    {
        mVoiceParam = param;
    }

    void updateVoiceInfo(VoiceInfo* voiceInfo) const;

private:
    static const u32 cHistoryWaveSamples = 4;
    static const u32 cDecodeWaveSamples = 2048;

    struct VeParam
    {
        u16 vol;
        s16 volDelta;
    };

    struct Lpf
    {
        bool on;
        u16 a0;
        u16 b0;
        u16 yn1;
    };

    struct Biquad
    {
        bool on;
        s16 b0;
        s16 b1;
        s16 b2;
        s16 a1;
        s16 a2;
        s16 xn1;
        s16 xn2;
        s16 yn1;
        s16 yn2;
    };

    void initialize(DisposeCallbackFunc func, void* arg);
    void finalize();

    void updateState(const OutputMode* outputMode);

    void setupVoice(const VoiceParam& voiceParam, const OutputMode* outputMode);
    void updateVoice(const VoiceParam& voiceParam, const OutputMode* outputMode);

    void setupMix(const VoiceParam& voiceParam, OutputMode outputMode);
    void updateMix(const VoiceParam& voiceParam, OutputMode outputMode);

    void updateVe(const VoiceParam& voiceParam, bool initialFlag);

    void updateMonoFilter(const VoiceParam& voiceParam, bool initialFlag);
    void updateBiquadFilter(const VoiceParam& voiceParam, bool initialFlag);

    void calcSetupMix(VeParam mix[], const OutputMix& src/*, bool sorroundFlag*/);
    bool calcUpdateMix(VeParam mix[], const OutputMix& src/*, bool sorroundFlag*/);

    u32 decodePcm16(s16* buffer, u32 samples);
    u32 decodePcm8(s16* buffer, u32 samples);
    u32 decodeAdpcm(s16* buffer, u32 samples);

    void synthesize(VoiceSynthesizeBuffer* buffer, f32* workBuffer, u32 samples, f32 sampleRate);
    bool mixSample(const f32* srcData, f32* mixBus, u32 samples, const VeParam& ve);

    bool updateNextWaveBuffer();

    static bool updateDelta(s32 target, VeParam* ve);

    static const u32 cBasePitch = 0x00010000;

    enum SrcType
    {
        SrcType_2_4,
        SrcType_3_4,
        SrcType_4_4,
        SrcType_Count
    };

    static const u32 cChannelCount = driver::HardwareMgr::cChannelCount;

    static const u32 cSrcCoefTapCount = 4;
    static const u32 cSrcCoefElementCount = 128;
    static const s16 cSrcCoef[SrcType_Count][cSrcCoefTapCount * cSrcCoefElementCount];

    static s16 sDecodeWaveBuffer[cDecodeWaveSamples + cHistoryWaveSamples];

private:
    friend class VoiceMgr;

    DisposeCallbackFunc mDisposeCallbackFunc;
    void* mDisposeCallbackArg;

    VoiceMgr* mVoiceMgr;

    WaveBuffer* mWaveBufferListBegin;
    WaveBuffer* mWaveBufferListEnd;

    VeParam mVe;
    VeParam mMix[cChannelCount];

    Lpf mLpf;
    Biquad mBiquad;

    AdpcmContext mAdpcmContext;

    u32 mCurrentAddressFraction;
    s16 mLastSamples[cHistoryWaveSamples];
    u32 mPlayPosition;

    SampleFormat mSampleFormat;
    u32 mSampleRate;
    AdpcmParam mAdpcmParam;

    VoiceState mState;
    VoiceParam mVoiceParam;

    u32 mPriority;

    u32 mCurrentOffset;
    u32 mLoopCount;
    bool mPauseFlag;

    bool mIsAppendedToList;

    sead::ListNode mVoiceListNode;
    Voice* mVoice;
};

class VoiceMgr
{
    SEAD_NO_COPY(VoiceMgr);

public:
    static const u32 cVoiceCountMax = driver::HardwareMgr::cMaxVoiceCount;

public:
    VoiceMgr();
    ~VoiceMgr();

    void initialize();
    void finalize();

    VoiceImpl* allocVoice(u32 priority, VoiceImpl::DisposeCallbackFunc func, void* arg);
    void freeVoice(VoiceImpl* voice);

    void updatePriorityOrder(VoiceImpl* voice);

    u32 getActiveVoiceCount() const;

    void detail_updateAllVoices();
    void detail_synthesize(VoiceSynthesizeBuffer* buffer, u32 samples, f32 sampleRate);
    const VoiceImpl* detail_getVoiceById(u32 id);

private:
    void appendVoiceList(VoiceImpl* voice);
    void eraseVoiceList(VoiceImpl* voice);

private:
    typedef sead::OffsetList<VoiceImpl> VoiceList;

    VoiceList mVoiceList;
    VoiceList mFreeVoiceList;
    VoiceImpl mVoiceArray[cVoiceCountMax];
};

class VoiceRenderer : public FinalMixCallback
{
    SEAD_NO_COPY(VoiceRenderer);

public:
    enum class SampleRateType
    {
        Rate_48000,
        Rate_32000
    };

    static const u32 cSynthesizeBufferCountMax = 64;

public:
    VoiceRenderer();
    ~VoiceRenderer() override;

    void initialize(u32 synthesizeBufferCount, sead::Heap* heap);
    void finalize();

    void updateAllVoices();
    u32 synthesize();

    void setSampleRate(SampleRateType type);

private:
    void mixSamples(const f32* src, f32* dst, u32 samples);

    void onFinalMix(const FinalMixData* data) override;

private:
    VoiceMgr mVoiceMgr;

    u32 mSampleCount;
    f32 mSampleRate;

    //MessageQueue mSynthesizeMsgQueue;
    LFRingBuffer<void*, cSynthesizeBufferCountMax> mSynthesizeMsgRingBuffer;

    VoiceSynthesizeBuffer* mSynthesizeBuffer;
    u32 mSynthesizeBufferCount;
};

} } // namespace snd::internal
