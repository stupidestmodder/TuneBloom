#include "snd/Channel.h"

#include "snd/DisposeCallbackMgr.h"
#include "snd/Util.h"
#include "snd/ChannelMgr.h"
#include "snd/MultiVoiceMgr.h"

#include <basis/seadWarning.h>

namespace snd { namespace internal { namespace driver {

namespace {

u8 getSndInterpolationTypeFromHardwareMgr()
{
    u8 result = 0;  // 0: 4-point interpolation, 1: Linear interpolation, 2: No interpolation.
                    // (Same as the value stored in BankFile::VelocityRegion.)

    switch (HardwareMgr::instance()->getSrcType())
    {
        case SrcType::Type_None:
            result = 2;
            break;

        case SrcType::Type_Linear:
            result = 1;
            break;

        case SrcType::Type_4Tap:
            // Remains as 0.
            break;
    }

    return result;
}

} // anonymous namespace

Channel::Channel()
    : mPauseFlag(0)
    , mActiveFlag(false)
    , mAllocFlag(false)
    , mVoice(nullptr)
{
    mDisposer.initialize(this);

    DisposeCallbackMgr::instance()->registerDisposeCallback(&mDisposer);
}

Channel::~Channel()
{
    DisposeCallbackMgr::instance()->unregisterDisposeCallback(&mDisposer);
}

void Channel::update(bool doPeriodicProc)
{
    if (!mActiveFlag)
        return;

    if (mPauseFlag)
        doPeriodicProc = false;

    // volume
    mSilenceVolume.update();
    f32 volume = mVelocity * mVelocity * mInstrumentVolume * mUserVolume * mSilenceVolume.getValue() * cSilenceVolumeMaxR;

    // Attenuation completion check
    if (mCurveAdshr.getStatus() == CurveAdshr::Status::Release && mCurveAdshr.getValue() < -90.4f)
    {
        this->stop();
        return;
    }

    // pitch
    f32 cent = mKey - mOriginalKey + mUserPitch + this->getSweepValue();

    for (u32 i = 0; i < cModCount; i++)
    {
        if (mLfoTarget[i] == (u8)LfoTarget::Pitch)
            cent += mLfo[i].getValue();
    }

    f32 pitch;

    if (cent == mCent)
    {
        pitch = mCentPitch;
    }
    else
    {
        pitch = Util::calcPitchRatio(static_cast<s32>(cent * (1 << Util::cPitchDivisionBit)));
        mCent = cent;
        mCentPitch = pitch;
    }

    pitch = pitch * mTune * mUserPitchRatio;

    OutputParam param = mParam;
    {
        param.pan += mInitPan;

        for (u32 i = 0; i < cModCount; i++)
        {
            if (mLfoTarget[i] == (u8)LfoTarget::Pan)
                param.pan += mLfo[i].getValue();
        }

        //param.span += mInitSurroundPan;
    }

    // Update counter
    if (doPeriodicProc)
    {
        if (mAutoSweep)
        {
            this->updateSweep(HardwareMgr::cSoundFrameIntervalMSEC);
        }

        for (u32 i = 0; i < cModCount; i++)
        {
            mLfo[i].update(HardwareMgr::cSoundFrameIntervalMSEC);
        }

        mCurveAdshr.update(HardwareMgr::cSoundFrameIntervalMSEC);
    }

    // lpf
    f32 lpfFreq = 1.0f + mUserLpfFreq;

    // Update the volume value.
    volume *= Util::calcVolumeRatio(mCurveAdshr.getValue());

    for (u32 i = 0; i < cModCount; i++)
    {
        if (mLfoTarget[i] == (u8)LfoTarget::Volume)
            volume *= Util::calcVolumeRatio(mLfo[i].getValue() * 6.0f);
    }

    if (mVoice)
    {
        mVoice->setVolume(volume);
        mVoice->setPitch(pitch);

        mVoice->setPanMode(mPanMode);
        mVoice->setPanCurve(mPanCurve);

        mVoice->setLpfFreq(lpfFreq);
        mVoice->setBiquadFilter(mBiquadType, mBiquadValue);

        mVoice->setOutputLine(mOutputLineFlag);
        mVoice->setParam(param);
    }
}

void Channel::callChannelCallback(ChannelCallbackStatus status)
{
    if (mCallback)
    {
        mCallback(this, status, mCallbackData);

        mCallback = nullptr;
        mCallbackData = nullptr;
    }
}

void Channel::start(const WaveInfo& waveInfo, s32 length, u32 startOffsetSamples)
{
    // Channel Param
    mLength = length;
    for (u32 i = 0; i < cModCount; i++)
    {
        mLfo[i].reset();
    }

    mCurveAdshr.reset();
    mSweepCounter = 0;

    mVoice->setSampleFormat(waveInfo.sampleFormat);
    mVoice->setSampleRate(waveInfo.sampleRate);
    mVoice->setInterpolationType(mInterpolationType);

    this->appendWaveBuffer(waveInfo, startOffsetSamples);

    mVoice->start();
    mActiveFlag = true;
}

void Channel::stop()
{
    if (!mVoice)
        return;

    mVoice->stop();
    mVoice->free();
    mVoice = nullptr;

    mPauseFlag = false;
    mActiveFlag = false;
}

void Channel::pause(bool flag)
{
    mPauseFlag = flag;
    mVoice->pause(flag);
}

void Channel::noteOff()
{
    if (mIsIgnoreNoteOff)
        return;

    this->release();
}

void Channel::release()
{
    if (mCurveAdshr.getStatus() != CurveAdshr::Status::Release)
    {
        if (mVoice)
        {
            if (!mReleasePriorityFixFlag)
                mVoice->setPriority(cPriorityRelease);
        }

        mCurveAdshr.setStatus(CurveAdshr::Status::Release);
    }

    mPauseFlag = false;
}

void Channel::setPriority(u32 priority)
{
    mVoice->setPriority(priority);
}

void Channel::updateSweep(s32 count)
{
    mSweepCounter += count;

    if (mSweepCounter > mSweepLength)
        mSweepCounter = mSweepLength;
}

u32 Channel::getCurrentPlayingSample(bool isOriginalSamplePosition) const
{
    if (!mActiveFlag)
        return 0;

    SEAD_ASSERT(mVoice != nullptr);

    u32 playSamplePosition = 0;

    if (mLoopFlag && mWaveBuffer[0][0].status == WaveBuffer::Status::Done)
        playSamplePosition = mLoopStartFrame + mVoice->getCurrentPlayingSample();
    else
        playSamplePosition = mStartOffsetSamples + mVoice->getCurrentPlayingSample();

    // Calculates the sample position before correcting the loop location.
    if (isOriginalSamplePosition && mLoopFlag)
    {
        u32 loopEnd = mStartOffsetSamples + mWaveBuffer[0][0].sampleLength;
        u32 originalLoopEnd = loopEnd - (mLoopStartFrame - mOriginalLoopStartFrame);
        
        if (playSamplePosition > originalLoopEnd)
            playSamplePosition = mOriginalLoopStartFrame + (playSamplePosition - originalLoopEnd);
    }
    
    return playSamplePosition;
}

void Channel::setUpdateType(UpdateType updateType)
{
    mVoice->setUpdateType(updateType);
}

UpdateType Channel::getUpdateType() const
{
    return mVoice->getUpdateType();
}

f32 Channel::getSweepValue() const
{
    if (mSweepPitch == 0.0f)
        return 0.0f;

    if (mSweepCounter >= mSweepLength)
        return 0.0f;

    f32 sweep = mSweepPitch;
    sweep *= mSweepLength - mSweepCounter;

    SEAD_ASSERT(mSweepLength != 0);
    sweep /= mSweepLength;

    return sweep;
}

void Channel::initParam(ChannelCallback callback, void* callbackData)
{
    mNextLink = nullptr; // User region

    mCallback = callback;
    mCallbackData = callbackData;

    // msync_flag = 0; // NOTE: must not be cleared!
    mPauseFlag = false;
    mAutoSweep = true;
    mReleasePriorityFixFlag = false;
    mIsIgnoreNoteOff = false;

    mLoopFlag = false;
    mLoopStartFrame = 0;
    mOriginalLoopStartFrame = 0;
    mStartOffsetSamples = 0;

    mLength = 0;
    mKey = cKeyInit;
    mOriginalKey = cOriginalKeyInit;
    mInitPan = 0.0f;
    //mInitSurroundPan = 0.0f;
    mTune = 1.0f;

    mCent = 0.0f;
    mCentPitch = 1.0f;

    mUserVolume = 1.0f;
    mUserPitch = 0.0f;
    mUserPitchRatio = 1.0f;
    mUserLpfFreq = 0.0f;
    mBiquadType = (u8)BiquadFilterType::None;
    mBiquadValue = 0.0f;

    mOutputLineFlag = (u32)OutputLine::Main;

    mParam.initialize();

    mSilenceVolume.initValue(cSilenceVolumeMax);

    mSweepPitch = 0.0f;
    mSweepLength = 0;
    mSweepCounter = 0;

    mCurveAdshr.initialize();

    for (u32 i = 0; i < cModCount; i++)
    {
        mLfo[i].getParam().initialize();
        mLfoTarget[i] = (u8)LfoTarget::Pitch;
    }

    mPanMode = PanMode::Dual;
    mPanCurve = PanCurve::Sqrt;

    mKeyGroupId = 0;
    mInterpolationType = getSndInterpolationTypeFromHardwareMgr();
        // WSD reflects this value.
        // SEQ reflects the value set by Channel::SetInterpolationType (the value written to the instrument's velocity region).
    mInstrumentVolume = 1.0f;
    mVelocity = 1.0f;

    //mUseContextInfo = false;
}

void Channel::appendWaveBuffer(const WaveInfo& waveInfo, u32 startOffsetSamples)
{
    mLoopFlag = waveInfo.loopFlag;
    mLoopStartFrame = waveInfo.loopStartFrame;
    mOriginalLoopStartFrame = waveInfo.originalLoopStartFrame;

    // Round down to the 8-byte boundary (because DSP ADPCM is encoded in 8-byte, 14-sample units).
    if (startOffsetSamples > 0 && waveInfo.sampleFormat == snd::SampleFormat::DspAdpcm)
        startOffsetSamples = (startOffsetSamples / 14) * 14;

    mStartOffsetSamples = startOffsetSamples;
    SEAD_ASSERT(waveInfo.loopEndFrame > startOffsetSamples);

    u32 loopStartByte = MultiVoice::frameToByte(mLoopStartFrame, waveInfo.sampleFormat);

    const u32 sdkVoiceCount = mVoice->getSdkVoiceCount();

    for (u32 ch = 0; ch < sdkVoiceCount; ch++)
    {
        const void* originalDataAddress = waveInfo.channelParam[ch].dataAddress;
        const void* adjustDataAddress = originalDataAddress;

        if (startOffsetSamples > 0)
            adjustDataAddress = sead::PtrUtil::addOffset(originalDataAddress, MultiVoice::frameToByte(startOffsetSamples, waveInfo.sampleFormat));

        AdpcmContext& adpcmContext = mAdpcmContext[ch];
        AdpcmContext& adpcmLoopContext = mAdpcmLoopContext[ch];
        AdpcmContext& offsetAdpcmContext = mContextInfo.contexts[ch];

        // For ADPCM, generate a context.
        if (waveInfo.sampleFormat == snd::SampleFormat::DspAdpcm)
        {
            const DspAdpcmParam* pParam = &waveInfo.channelParam[ch].adpcmParam;

            // Coefficient setting (u16 coef[16]).
            snd::AdpcmParam param;
            for (s32 i = 0; i < 8; i++)
            {
                param.coef[i][0] = pParam->coef[i][0];
                param.coef[i][1] = pParam->coef[i][1];
            }

            // Context settings (maintained in the SDK user side until nw::snd::DspVoice is used).
            if (startOffsetSamples == 0)
            {
                adpcmContext.pred_scale = pParam->predScale;
                adpcmContext.yn1 = pParam->yn1;
                adpcmContext.yn2 = pParam->yn2;
            }
            else // startOffsetSamples > 0
            {
                if (mUseContextInfo)
                {
                    adpcmContext = offsetAdpcmContext; // Structure copying
                }
                else
                {
                    // pred_scale goes into the first byte of the 8-byte boundary.
                    // pred_scale is type u16, but because the most significant 8 bits are not referenced, it should be OK to assign *reinterpret_cast<const u16*>( adjustDataAddress ) for pred_scale.
                    // 
                    adpcmContext.pred_scale = pParam->predScale;
                    adpcmContext.yn1 = pParam->yn1;
                    adpcmContext.yn2 = pParam->yn2;

                    // Decode partway on own effort (because normally it is the DSP doing the decoding, for this portion, the CPU takes the load).
                    MultiVoice::calcOffsetAdpcmParam(
                        &adpcmContext,
                        param,
                        startOffsetSamples,
                        originalDataAddress
                    );
                }
            }

            if (waveInfo.loopFlag)
            {
                const DspAdpcmLoopParam* pLoopParam = &waveInfo.channelParam[ch].adpcmLoopParam;
                adpcmLoopContext.pred_scale = pLoopParam->loopPredScale;
                adpcmLoopContext.yn1 = pLoopParam->loopYn1;
                adpcmLoopContext.yn2 = pLoopParam->loopYn2;
            }

            mVoice->setAdpcmParam(ch, param);
        }

        {
            // A waveform such as |------------------|<---------- loop --------->| would be used as |<-------------- pBuffer0 -------------------->|, |<--------- pBuffer1 ------>|.

            // WaveBuffer registration.
            WaveBuffer* pBuffer0 = &mWaveBuffer[ch][0];
            WaveBuffer* pBuffer1 = &mWaveBuffer[ch][1];

            pBuffer0->bufferAddress = adjustDataAddress;
            pBuffer0->sampleLength = waveInfo.loopEndFrame - startOffsetSamples;
            pBuffer0->loopFlag = false;

            if (waveInfo.sampleFormat == snd::SampleFormat::DspAdpcm)
                pBuffer0->adpcmContext = &adpcmContext;
            else
                pBuffer0->adpcmContext = nullptr;

            pBuffer0->endian = waveInfo.endian; // Custom

            if (waveInfo.loopFlag)
            {
                pBuffer1->bufferAddress = sead::PtrUtil::addOffset(originalDataAddress, loopStartByte);
                pBuffer1->sampleLength = waveInfo.loopEndFrame - mLoopStartFrame;
                pBuffer1->loopFlag = true;

                if (waveInfo.sampleFormat == snd::SampleFormat::DspAdpcm)
                    pBuffer1->adpcmContext = &adpcmLoopContext;
                else
                    pBuffer1->adpcmContext = nullptr;

                pBuffer1->endian = waveInfo.endian; // Custom

                mVoice->appendWaveBuffer(ch, pBuffer0, false);
                mVoice->appendWaveBuffer(ch, pBuffer1, true);
            }
            else
            {
                mVoice->appendWaveBuffer(ch, pBuffer0, true);
            }
        }
    }
}

Channel* Channel::allocChannel(s32 voiceChannelCount, u32 priority, Channel::ChannelCallback callback, void* callbackData)
{
    SEAD_ASSERT(priority >= Voice::cPriorityMin && priority <= Voice::cPriorityMax);

    Channel* channel = ChannelMgr::instance()->alloc();

    if (!channel)
    {
        //Util::WarningLogger::GetInstance().Log(Util::WarningLogger::LOG_ID_CHANNEL_ALLOCATION_FAILED);
        SEAD_WARNING("snd::internal::driver::Channel allocation failed");
        return nullptr;
    }

    channel->mAllocFlag = true;

    // Voice acquisition
    MultiVoice* voice = MultiVoiceMgr::instance()->allocVoice(voiceChannelCount, priority, &Channel::VoiceCallbackFunc, channel);

    if (!voice)
    {
        ChannelMgr::instance()->free(channel);
        return nullptr;
    }

    channel->mVoice = voice;
    channel->initParam(callback, callbackData);

    return channel;
}

void Channel::freeChannel(Channel* channel)
{
    SEAD_ASSERT(channel != nullptr);

    ChannelMgr::instance()->free(channel);
}

void Channel::detachChannel(Channel* channel)
{
    SEAD_ASSERT(channel != nullptr);

    channel->mCallback = nullptr;
    channel->mCallbackData = nullptr;
}

void Channel::VoiceCallbackFunc(MultiVoice* voice, MultiVoice::VoiceCallbackStatus status, void* arg)
{
    SEAD_ASSERT(arg != nullptr);

    ChannelCallbackStatus chStatus = ChannelCallbackStatus::Finish; // Appropriate initialization.

    switch (status)
    {
        case MultiVoice::VoiceCallbackStatus::FinishWave:
            chStatus = ChannelCallbackStatus::Finish;
            voice->free();
            break;

        case MultiVoice::VoiceCallbackStatus::Cancel:
            chStatus = ChannelCallbackStatus::Cancel;
            voice->free();
            break;

        case MultiVoice::VoiceCallbackStatus::DropVoice:
            chStatus = ChannelCallbackStatus::Drop;
            break;

        case MultiVoice::VoiceCallbackStatus::DropDsp:
            chStatus = ChannelCallbackStatus::Drop;
            break;
    }

    Channel* channel = static_cast<Channel*>(arg);

    channel->callChannelCallback(chStatus);

    channel->mVoice = nullptr;
    channel->mPauseFlag = false;
    channel->mActiveFlag = false;

    channel->mAllocFlag = false;
}

void Channel::Disposer::invalidateData(const void* start, const void* end)
{
    if (!mChannel->mVoice)
        return;

    // Stop voice(s) if there is any waveform data included between 'start' and 'end'
    bool disposeFlag = false;
    s32 sdkVoiceCount = mChannel->mVoice->getSdkVoiceCount();

    for (s32 channelIndex = 0; channelIndex < sdkVoiceCount; channelIndex++)
    {
        for (s32 waveBufferIndex = 0; waveBufferIndex < cWaveBufferMax; waveBufferIndex++)
        {
            const WaveBuffer& waveBuffer = mChannel->mWaveBuffer[channelIndex][waveBufferIndex];

            if (waveBuffer.status == WaveBuffer::Status::Done)
                continue;

            const void* bufferEnd = sead::PtrUtil::addOffset(waveBuffer.bufferAddress, Util::getByteBySample(waveBuffer.sampleLength, mChannel->mVoice->getFormat()));

            if (start <= bufferEnd && end >= waveBuffer.bufferAddress)
            {
                disposeFlag = true;
                break;
            }
        }
    }

    if (disposeFlag)
    {
        mChannel->callChannelCallback(ChannelCallbackStatus::Cancel);

        mChannel->stop();

        Channel::freeChannel(mChannel);
    }
}

} } } // namespace snd::internal::driver
