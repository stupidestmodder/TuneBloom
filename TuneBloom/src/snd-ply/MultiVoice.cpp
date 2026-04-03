#include "snd/MultiVoice.h"

#include "snd/DecodeAdpcm.h"
#include "snd/Util.h"
#include "snd/MultiVoiceMgr.h"
#include "snd/HardwareMgr.h"
#include "snd/BiquadFilterCallback.h"

#include <math/seadMathCalcCommon.h>

namespace snd { namespace internal { namespace driver {

namespace {

u32 DspAdpcmFrameToNibbleAddress(u32 frame)
{
    u32 mod = frame % 14;
    u32 nibbleAddress = (frame / 14) * 16;
    if (mod != 0)
        nibbleAddress += (mod + 2);

    return nibbleAddress;
}

inline void panCurveToPanInfo(Util::PanInfo& panInfo, PanCurve curve)
{
    switch (curve)
    {
        case PanCurve::Sqrt:
            panInfo.curve = Util::PanCurve::Sqrt;
            break;

        case PanCurve::Sqrt0Db:
            panInfo.curve = Util::PanCurve::Sqrt;
            panInfo.centerZeroFlag = true;
            break;

        case PanCurve::Sqrt0DbClamp:
            panInfo.curve = Util::PanCurve::Sqrt;
            panInfo.centerZeroFlag = true;
            panInfo.zeroClampFlag = true;
            break;

        case PanCurve::SinCos:
            panInfo.curve = Util::PanCurve::SinCos;
            break;

        case PanCurve::SinCos0Db:
            panInfo.curve = Util::PanCurve::SinCos;
            panInfo.centerZeroFlag = true;
            break;

        case PanCurve::SinCos0DbClamp:
            panInfo.curve = Util::PanCurve::SinCos;
            panInfo.centerZeroFlag = true;
            panInfo.zeroClampFlag = true;
            break;

        case PanCurve::Linear:
            panInfo.curve = Util::PanCurve::Linear;
            break;

        case PanCurve::Linear0Db:
            panInfo.curve = Util::PanCurve::Linear;
            panInfo.centerZeroFlag = true;
            break;

        case PanCurve::Linear0DbClamp:
            panInfo.curve = Util::PanCurve::Linear;
            panInfo.centerZeroFlag = true;
            panInfo.zeroClampFlag = true;
            break;

        default:
            panInfo.curve = Util::PanCurve::Sqrt;
            break;
    }
}

// Calculate pan for monaural balance or dual.
inline void calcPanForMono(f32* left, f32* right, const Util::PanInfo& panInfo)
{
    f32 ratio = Util::calcPanRatio(MultiVoice::cPanCenter, panInfo, OutputMode::Mono);

    *left = ratio;
    *right = ratio;
}

// Calculate pan for stereo or surround balance.
inline void calcBarancePanForStereo(f32* left, f32* right, const f32& pan, s32 channelIndex, const Util::PanInfo& panInfo, OutputMode mode)
{
    if (channelIndex == 0)
    {
        *left = Util::calcPanRatio(pan, panInfo, mode);
        *right = 0.0f;
    }
    else if (channelIndex == 1)
    {
        *left = 0.0f;
        *right = Util::calcPanRatio(MultiVoice::cPanCenter - pan, panInfo, mode);
    }
}

// Calculate dual pan for stereo or surround.
inline void calcDualPanForStereo(f32* left, f32* right, const f32& pan, const Util::PanInfo& panInfo, OutputMode mode)
{
    *left = Util::calcPanRatio(pan, panInfo, mode);
    *right = Util::calcPanRatio(MultiVoice::cPanCenter - pan, panInfo, mode);
}

// Calculate surround pan for monaural or stereo.
inline void calcSurroundPanForMono(f32* front, f32* rear, const Util::PanInfo& panInfo)
{
    *front = Util::calcSurroundPanRatio(MultiVoice::cSpanFront, panInfo);
    *rear = Util::calcSurroundPanRatio(MultiVoice::cSpanRear, panInfo);
}

} // anonymous namespace

/*
inline u16 calcMixVolume(f32 volume)
{
    if (volume <= 0.0f)
        return 0UL;

    return static_cast<u16>(Mathu::min(0x0000FFFFUL, static_cast<u32>(volume * 0x8000)));
}
*/

MultiVoice::MultiVoice()
    : mCallback(nullptr)
    , mIsActive(false)
    , mIsStart(false)
    , mIsStarted(false)
    , mIsPause(false)
    , mSyncFlag(0)
    , mChannelCount(0)
    , mUpdateType(UpdateType::AudioFrame)
{
}

MultiVoice::~MultiVoice()
{
    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        mVoice[channelIndex].free();
    }
}

bool MultiVoice::alloc(s32 channelCount, u32 priority, VoiceCallback callback, void* callbackData)
{
    SEAD_ASSERT(channelCount >= 1 && channelCount <= cWaveChannelMax);

    channelCount = sead::Mathi::clamp2(1, channelCount, static_cast<s32>(cWaveChannelMax));

    mChannelCount = 0;

    SEAD_ASSERT(!mIsActive);

    // Voice acquisition
    s32 allocCount = 0;

    for (s32 i = 0; i < channelCount; i++)
    {
        if (!mVoice[i].allocVoice(priority))
            break;

        allocCount++;
    }

    if (allocCount != channelCount)
    {
        for (s32 i = 0; i < allocCount; i++)
        {
            mVoice[i].free();
        }

        return false;
    }

    mChannelCount = channelCount;

    // Initialize parameters
    this->initParam(callback, callbackData);

    mIsActive = true;

    return true;
}

void MultiVoice::free()
{
    if (!mIsActive)
        return;

    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
            mVoice[channelIndex].free();
    }

    mChannelCount = 0;

    MultiVoiceMgr::instance()->freeVoice(this);

    mIsActive = false;
}

void MultiVoice::start()
{
    mIsStart = true;
    mIsPause = false;

    mSyncFlag |= cUpdateStart;
}

void MultiVoice::stop()
{
    if (mIsStarted)
    {
        this->stopAllSdkVoice();

        mIsStarted = false;
    }

    mIsPausing = false;
    mIsPause = false;
    mIsStart = false;
}

void MultiVoice::updateVoiceStatus()
{
    if (!mIsActive)
        return;

    // Playback stop check
    if (!mIsStarted)
        return;

    if (this->isPlayFinished())
    {
        // Waveform stop
        if (mCallback)
            mCallback(this, VoiceCallbackStatus::FinishWave, mCallbackData);

        mIsStarted = false;
        mIsStart = false;
    }
    else
    {
        // Voice drop check.
        bool dropFlag = false;

        for (s32 ch = 0; ch < mChannelCount; ch++)
        {
            if (!mVoice[ch].isAvailable())
            {
                dropFlag = true;
                break;
            }
        }

        if (dropFlag)
        {
            for (s32 ch = 0; ch < mChannelCount; ch++)
            {
                mVoice[ch].free();
            }

            mChannelCount = 0;
            mIsActive = false;

            // TODO: Is the timing for freeVoice appropriate?
            MultiVoiceMgr::instance()->freeVoice(this);

            if (mCallback)
                mCallback(this, VoiceCallbackStatus::DropDsp, mCallbackData);

            mIsStarted = false;
            mIsStart = false;
        }
    }
}

void MultiVoice::pause(bool flag)
{
    if (mIsPause == flag)
        return;

    mIsPause = flag;
    mSyncFlag |= cUpdatePause;
}

void MultiVoice::calc()
{
    if (!mIsStart)
        return;

    // Playback rate
    if (mSyncFlag & cUpdateSrc)
    {
        this->calcSrc(false);
        mSyncFlag &= ~cUpdateSrc;
    }

    // Volume.
    if (mSyncFlag & cUpdateVe)
    {
        this->calcVe();
        mSyncFlag &= ~cUpdateVe;
    }

    // Mix
    if (mSyncFlag & cUpdateMix)
    {
        this->calcMix();
        mSyncFlag &= ~cUpdateMix;
    }

    // Low-pass filter
    if (mSyncFlag & cUpdateLpf)
    {
        this->calcLpf();
        mSyncFlag &= ~cUpdateLpf;
    }

    // Biquad filter
    if (mSyncFlag & cUpdateBiquad)
    {
        this->calcBiquadFilter();
        mSyncFlag &= ~cUpdateBiquad;
    }
}

void MultiVoice::update()
{
    if (!mIsActive)
        return;

    // Flags for playing/stopping voices together at a later time
    enum { None, Run, Stop, Pause } runFlag = None;

    // Start playback.
    if (mSyncFlag & cUpdateStart)
    {
        if (mIsStart && !mIsStarted)
        {
            this->calcSrc(true);
            this->calcMix();
            this->calcVe();   // Apply master volume.

            runFlag = Run;
            mIsStarted = true;

            mSyncFlag &= ~cUpdateStart;
            mSyncFlag &= ~cUpdateSrc;
            mSyncFlag &= ~cUpdateMix;
        }
    }

    if (mIsStarted)
    {
        // Pause
        if (mSyncFlag & cUpdatePause)
        {
            if (mIsStart)
            {
                if (mIsPause)
                {
                    runFlag = Pause;
                    mIsPausing = true;
                }
                else
                {
                    runFlag = Run;
                    mIsPausing = false;
                }

                mSyncFlag &= ~cUpdatePause;
            }
        }
    }

    for (s32 i = 0; i < mChannelCount; i++)
    {
        mVoice[i].updateParam();
    }

    // Collect together the voice playback and stop operations and run them.
    // Do this because depop will be generated if you perform the play and stop operations on a voice that has not been played yet in the same frame.
    switch (runFlag)
    {
        case Run:
            this->runAllSdkVoice();
            break;

        case Stop:
            this->stopAllSdkVoice();
            break;

        case Pause:
            this->pauseAllSdkVoice();
            break;

        case None:
            break;

        default:
            break;
    }
}

bool MultiVoice::isRun() const
{
    if (this->isActive())
        return mVoice[0].getState() == VoiceState::Play;

    return false;
}

bool MultiVoice::isPlayFinished() const
{
    if (!this->isActive())
        return false;

    if (!mLastWaveBuffer)
        return false;

    if (mLastWaveBuffer->status == WaveBuffer::Status::Wait || mLastWaveBuffer->status == WaveBuffer::Status::Play)
        return false;

    return true;
}

void MultiVoice::setSampleFormat(SampleFormat format)
{
    mFormat = format;

    for (s32 i = 0; i < mChannelCount; i++)
    {
        mVoice[i].setSampleFormat(mFormat);
    }
}

void MultiVoice::setSampleRate(u32 sampleRate)
{
    for (s32 i = 0; i < mChannelCount; i++)
    {
        mVoice[i].setSampleRate(sampleRate);
    }
}

void MultiVoice::setVolume(f32 volume)
{
    if (volume < cVolumeMin)
        volume = cVolumeMin;

    if (volume != mVolume)
    {
        mVolume = volume;
        mSyncFlag |= cUpdateVe;
    }
}

void MultiVoice::setPitch(f32 pitch)
{
    if (pitch != mPitch)
    {
        mPitch = pitch;
        mSyncFlag |= cUpdateSrc;
    }
}

void MultiVoice::setPanMode(PanMode panMode)
{
    if (panMode != mPanMode)
    {
        mPanMode = panMode;
        mSyncFlag |= cUpdateMix;
    }
}

void MultiVoice::setPanCurve(PanCurve panCurve)
{
    if (panCurve != mPanCurve)
    {
        mPanCurve = panCurve;
        mSyncFlag |= cUpdateMix;
    }
}

void MultiVoice::setLpfFreq(f32 lpfFreq)
{
    if (lpfFreq != mLpfFreq)
    {
        mLpfFreq = lpfFreq;
        mSyncFlag |= cUpdateLpf;
    }
}

void MultiVoice::setBiquadFilter(s32 type, f32 value)
{
    // To reach here, the INHERIT process should be done, so BiquadFilterType::DataMin should be minimized.
    SEAD_ASSERT(type >= (s32)BiquadFilterType::DataMin && type <= (s32)BiquadFilterType::Max);

    value = sead::Mathf::clamp2(cBiquadValueMin, value, cBiquadValueMax);

    bool isUpdate = false;

    if (type != mBiquadType)
    {
        mBiquadType = static_cast<u8>(type);
        isUpdate = true;
    }

    if (value != mBiquadValue)
    {
        mBiquadValue = value;
        isUpdate = true;
    }

    if (isUpdate)
        mSyncFlag |= cUpdateBiquad;
}

void MultiVoice::setPriority(u32 priority)
{
    mPriority = priority;
    MultiVoiceMgr::instance()->changeVoicePriority(this);

    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        mVoice[channelIndex].setPriority(mPriority);
    }
}

/*
void MultiVoice::setFrontBypass(bool isFrontBypass)
{
    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        mVoice[channelIndex].setFrontBypass(isFrontBypass);
    }

    mIsEnableFrontBypass = isFrontBypass;
}
*/

void MultiVoice::setInterpolationType(u8 interpolationType)
{
    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        mVoice[channelIndex].setInterpolationType(interpolationType);
    }
}

const Voice& MultiVoice::detail_getSdkVoice(s32 channelIndex ) const
{
    SEAD_ASSERT(channelIndex < cWaveChannelMax);
    return mVoice[channelIndex];
}

void MultiVoice::setOutputLine(u32 lineFlag)
{
    if (lineFlag != mOutputLineFlag)
    {
        mOutputLineFlag = lineFlag;
        mSyncFlag |= cUpdateMix;
    }
}

void MultiVoice::setParam(const OutputParam& param)
{
    this->setOutputParamImpl(param, mParam);
}

u32 MultiVoice::getCurrentPlayingSample() const
{
    if (!this->isActive())
        return 0;

    return mVoice[0].getPlayPosition();
}

SampleFormat MultiVoice::getFormat() const
{
    SEAD_ASSERT(this->isActive());
    return mFormat;
}

u32 MultiVoice::frameToByte(u32 sample, SampleFormat format)
{
    u32 result = 0;

    switch (format)
    {
        case SampleFormat::DspAdpcm:
            result = DspAdpcmFrameToNibbleAddress(sample) >> 1;
            break;

        case SampleFormat::PcmS8:
            result = sample;
            break;

        case SampleFormat::PcmS16:
            result = sample << 1;
            break;

        default:
            SEAD_ASSERT_MSG(false, "Invalid format");
    }

    return result;
}

void MultiVoice::calcOffsetAdpcmParam(AdpcmContext* context, const AdpcmParam& param, u32 offsetSamples, const void* dataAddress)
{
    u32 currentSample = 0;
    const u8* inputAddress = reinterpret_cast<const u8*>(dataAddress);

    const u32 cDecodeSampleCount = 14; // Decode in 14 sample units.
    SEAD_ASSERT(offsetSamples % cDecodeSampleCount == 0);

    s16 output[cDecodeSampleCount] = { 0 };
    while (currentSample < offsetSamples)
    {
        DecodeDspAdpcm(currentSample, *context, param, inputAddress, cDecodeSampleCount, output);

        currentSample += cDecodeSampleCount;
    }
}

void MultiVoice::appendWaveBuffer(s32 channelIndex, WaveBuffer* buffer, bool lastFlag)
{
    SEAD_ASSERT(buffer != nullptr);
    SEAD_ASSERT(channelIndex >= 0 && channelIndex < cWaveChannelMax);

    mVoice[channelIndex].appendWaveBuffer(buffer);

    if (lastFlag)
        mLastWaveBuffer = buffer;
}

void MultiVoice::setAdpcmParam(s32 channelIndex, const AdpcmParam& param)
{
    SEAD_ASSERT(channelIndex >= 0 && channelIndex < cWaveChannelMax);

    mVoice[channelIndex].setAdpcmParam(param);
}

void MultiVoice::initParam(VoiceCallback callback, void* callbackData)
{
    // Init Flag
    mSyncFlag  = 0;
    mIsPause = false;
    mIsPausing = false;
    mIsStart = false;
    mIsStarted = false;

    // Init Param
    mLastWaveBuffer = nullptr;
    mCallback = callback;
    mCallbackData = callbackData;
    mVolume = cVolumeDefault;
    mPitch = 1.0f;
    mPanMode = PanMode::Dual;
    mPanCurve = PanCurve::Sqrt;
    mLpfFreq = 1.0f;
    mBiquadType = (u8)BiquadFilterType::None;
    mBiquadValue = 0.0f;
    //mIsEnableFrontBypass = false;

    mOutputLineFlag = (u32)OutputLine::Main;

    mParam.initialize();
}

void MultiVoice::calcSrc(bool initialUpdate)
{
    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        mVoice[channelIndex].setPitch(mPitch);
    }
}

void MultiVoice::calcVe()
{
    f32 volume = mVolume;
    volume *= HardwareMgr::instance()->getOutputVolume();

    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        mVoice[channelIndex].setVolume(volume);
    }
}

void MultiVoice::calcMix()
{
    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        Voice& voice = mVoice[channelIndex];

        PreMixVolume preMix;
        this->calcPreMixVolume(&preMix, mParam, channelIndex);

        OutputMix mix;
        this->calcMix(&mix, preMix);

        voice.setMix(mix);
    }
}

void MultiVoice::calcLpf()
{
    u16 freq = Util::calcLpfFreq(mLpfFreq);

    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        Voice& voice = mVoice[channelIndex];

        if (freq >= 16000)
            voice.setMonoFilter(false);
        else
            voice.setMonoFilter(true, freq);
    }
}

void MultiVoice::calcBiquadFilter()
{
    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        Voice& voice = mVoice[channelIndex];

        const BiquadFilterCallback* cb = HardwareMgr::instance()->getBiquadFilterCallback(mBiquadType);

        if (cb)
        {
            if (mBiquadValue <= 0.0f)
            {
                voice.setBiquadFilter(false);
            }
            else
            {
                BiquadFilterCallback::Coefficients coef;
                cb->getCoefficients(mBiquadType, mBiquadValue, &coef);

                voice.setBiquadFilter(true, &coef);
            }
        }
        else
        {
            voice.setBiquadFilter(false);
        }
    }
}

void MultiVoice::calcPreMixVolume(PreMixVolume* mix, const OutputParam& param, s32 channelIndex)
{
    SEAD_ASSERT(mix != nullptr);

    // Calculate the mix parameter corresponding to NW OutputMode.
    const OutputMode nwMode = HardwareMgr::instance()->getOutputMode();

    f32 fL = 0.0f;
    f32 fR = 0.0f;
    f32 lrMixed = 0.0f;

    if (param.mixMode == (u32)MixMode::Pan)
    {
        f32 left = 0.0f;
        f32 right = 0.0f;
        f32 front = 0.0f;
        f32 rear = 0.0f;

        Util::PanInfo panInfo;
        panCurveToPanInfo(panInfo, mPanCurve);

        // Calculate left and right.
        switch (nwMode)
        {
            case OutputMode::Mono:
                calcPanForMono(&left, &right, panInfo);
                break;

            case OutputMode::Stereo:
                if (mChannelCount > 1 && mPanMode == PanMode::Balance)
                {
                    calcBarancePanForStereo(&left, &right, param.pan, channelIndex, panInfo, nwMode);
                }
                else
                {
                    f32 voicePan = param.pan;
                    if (mChannelCount == 2)
                    {
                        if (channelIndex == 0)
                            voicePan -= 1.0f;

                        if (channelIndex == 1)
                            voicePan += 1.0f;
                    }

                    calcDualPanForStereo(&left, &right, voicePan, panInfo, nwMode);
                }
                break;

            default:
                SEAD_ASSERT_MSG(false, "[%s] nwMode(%d) is invalid", __FUNCTION__, (s32)nwMode);
                break;
        }

        // Calculate front and rear.
        switch (nwMode)
        {
            case OutputMode::Mono:
            case OutputMode::Stereo:
                calcSurroundPanForMono(&front, &rear, panInfo);
                break;

            default:
                break;
        }

        fL = front * left;
        fR = front * right;
        lrMixed = (left + right) * 0.5f;
    }
    else
    {
        fL = param.mixParameter[channelIndex].ch[(u32)ChannelIdx::Left];
        fR = param.mixParameter[channelIndex].ch[(u32)ChannelIdx::Right];
        lrMixed = (fL + fR) * 0.5f;
    }

    const OutputMode endUserMode = HardwareMgr::instance()->getEndUserOutputMode();

    switch (endUserMode)
    {
        case OutputMode::Mono:
            switch (nwMode)
            {
                // case OutputMode::Mono:
                //     // do nothing
                //     break;

                case OutputMode::Stereo:
                    {
                        f32 mono = (fL + fR) * 0.5f;
                        fL = mono;
                        fR = mono;
                    }
                    break;

                default:
                    break;
            }
            break;

        case OutputMode::Stereo:
            switch (nwMode)
            {
                // case OutputMode::Mono:
                //     // do nothing
                //     break;

                // case OutputMode::Stereo:
                //     // do nothing
                //     break;

                default:
                    break;
            }
            break;

        default:
            SEAD_ASSERT_MSG(false, "[%s] endUserMode(%d) is invalid\n", __FUNCTION__, (s32)endUserMode);
            break;
    }

    // Value storage.
    {
        mix->volume[(u32)ChannelIdx::Left] = fL;
        mix->volume[(u32)ChannelIdx::Right] = fR;
        mix->lrMixedVolume = lrMixed;
    }
}

void MultiVoice::calcMix(OutputMix* mix, const PreMixVolume& preMix)
{
    this->calcMixImpl(mix, mParam, preMix);
}

void MultiVoice::calcMixImpl(OutputMix* mix, const OutputParam& param, const PreMixVolume& pre)
{
    f32 main = 0.0f;
    f32 aux[(u32)AuxBus::Num] = { 0.0f };

    u32 outputDeviceFlag = 0;

    for (s32 i = 0; i < (s32)OutputLineIdx::Max; i++)
    {
        if ((mOutputLineFlag >> i) & 1)
            outputDeviceFlag |= HardwareMgr::instance()->getOutputDeviceFlag(i);
    }

    if (outputDeviceFlag & 1)
    {
        main = sead::Mathf::clamp2(0.0f, param.mainSend + 1.0f, 1.0f);

        for (s32 i = 0; i < (u32)AuxBus::Num; i++)
        {
            aux[i] = sead::Mathf::clamp2(0.0f, param.fxSend[i], 1.0f);
        }
    }

    for (s32 i = 0; i < (u32)ChannelIdx::Num; i++)
    {
        f32 channelVolume = pre.volume[i] * param.volume;
        mix->mainBus[i] = main * channelVolume;

        for (s32 j = 0; j < (u32)AuxBus::Num; j++)
        {
            mix->auxBus[j][i] = aux[j] * channelVolume;
        }
    }
}

void MultiVoice::runAllSdkVoice()
{
    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        mVoice[channelIndex].setState(VoiceState::Play);
    }
}

void MultiVoice::stopAllSdkVoice()
{
    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        mVoice[channelIndex].setState(VoiceState::Stop);
    }
}

void MultiVoice::pauseAllSdkVoice()
{
    for (s32 channelIndex = 0; channelIndex < mChannelCount; channelIndex++)
    {
        mVoice[channelIndex].setState(VoiceState::Pause);
    }
}

void MultiVoice::setOutputParamImpl(const OutputParam& in, OutputParam& out)
{
    // volume
    {
        f32 volume = in.volume;
        if (volume < cVolumeMin)
            volume = cVolumeMin;

        if (volume != out.volume)
        {
            out.volume = volume;
            mSyncFlag |= cUpdateMix;
        }
    }

    // mixMode
    if (in.mixMode != out.mixMode)
    {
        out.mixMode = in.mixMode;
        mSyncFlag |= cUpdateMix;
    }

    // mixParameter
    for (s32 i = 0; i < cWaveChannelMax; i++)
    {
        for (s32 j = 0; j < (s32)ChannelIdx::Num; j++)
        {
            if (in.mixParameter[i].ch[j] != out.mixParameter[i].ch[j])
            {
                out.mixParameter[i].ch[j] = in.mixParameter[i].ch[j];
                mSyncFlag |= cUpdateMix;
            }
        }
    }

    // Pan
    if (in.pan != out.pan)
    {
        out.pan = in.pan;
        mSyncFlag |= cUpdateMix;
    }

    // span
    //if (in.span != out.span)
    //{
    //    out.span = in.span;
    //    mSyncFlag |= cUpdateMix;
    //}

    // mainSend
    if (in.mainSend != out.mainSend)
    {
        out.mainSend = in.mainSend;
        mSyncFlag |= cUpdateMix;
    }

    // fxSend
    for (s32 i = 0; i < (u32)AuxBus::Num; i++)
    {
        if (in.fxSend[i] != out.fxSend[i])
        {
            out.fxSend[i] = in.fxSend[i];
            mSyncFlag |= cUpdateMix;
        }
    }
}

} } } // namespace snd::internal::driver
