#include "snd/Voice.h"

#include "snd/VoiceImpl.h"

namespace snd { namespace internal {

VoiceMgr* Voice::sVoiceMgr = nullptr;

namespace {

const BiquadFilterCoefficients cBiquadFilterCoefZero = { 0 };

const OutputMix cDefaultMix =
{
    1.0f, 1.0f,  // mainBus
    0.0f, 0.0f,  // auxA
    0.0f, 0.0f,  // auxB
    0.0f, 0.0f   // auxC
};

}

void VoiceParam::initialize()
{
    volume = 1.0f;
    pitch = 1.0f;
    mix = cDefaultMix;

    monoFilterFlag = false;
    biquadFilterFlag = false;

    biquadFilterCoefficients = cBiquadFilterCoefZero;
    monoFilterCutoff = 0;
    interpolationType = 0;
}

Voice::Voice()
    : mVoiceImpl(nullptr)
{
}

Voice::~Voice()
{
}

bool Voice::allocVoice(u32 priority)
{
    if (sVoiceMgr)
    {
        mVoiceImpl = sVoiceMgr->allocVoice(priority, &VoiceImplDisposeCallback, this);

        if (!mVoiceImpl)
            return false;
    }

    this->initialize_(priority);

    return true;
}

void Voice::free()
{
    mState = VoiceState::Stop;

    if (mVoiceImpl)
    {
        mVoiceImpl->free();
        mVoiceImpl = nullptr;
    }
}

bool Voice::isAvailable() const
{
    if (mVoiceImpl)
        return mVoiceImpl->isAvailable();

    return false;
}

void Voice::setState(VoiceState state)
{
    mState = state;

    if (mVoiceImpl)
    {
        mVoiceImpl->setState(mState);

        if (mState == VoiceState::Play)
        {
            mVoiceImpl->setSampleRate(mSampleRate);
            mVoiceImpl->setSampleFormat(mSampleFormat);
            mVoiceImpl->setAdpcmParam(mAdpcmParam);
        }
        else if (mState == VoiceState::Stop)
        {
            mVoiceImpl->freeAllWaveBuffer();
        }
    }
}

void Voice::appendWaveBuffer(WaveBuffer* waveBuffer)
{
    if (mVoiceImpl)
        mVoiceImpl->appendWaveBuffer(waveBuffer);
}

void Voice::setPriority(u32 priority)
{
    mPriority = priority;

    if (mVoiceImpl)
        mVoiceImpl->setPriority(mPriority);
}

void Voice::setMonoFilter(bool enable, u16 cutoff)
{
    mVoiceParam.monoFilterFlag = enable;

    if (enable)
    {
        // MultiVoice::CalcLpf on the caller's side guaranteed to be less than 16000.
        SEAD_ASSERT(cutoff < 16000);

        mVoiceParam.monoFilterCutoff = cutoff;
    }
}

void Voice::setBiquadFilter(bool enable, const BiquadFilterCoefficients* coef)
{
    mVoiceParam.biquadFilterFlag = enable;

    if (enable)
    {
        // MultiVoice::CalcBiquadFilter on the caller's side guaranteed to be not nullptr.
        SEAD_ASSERT(coef != nullptr);

        mVoiceParam.biquadFilterCoefficients = *coef;
    }
}

void Voice::updateParam()
{
    if (mVoiceImpl)
        mVoiceImpl->setVoiceParam(mVoiceParam);
}



u32 Voice::getPlayPosition() const
{
    if (mVoiceImpl)
        return mVoiceImpl->getPlayPosition();

    return 0;
}

void Voice::detail_setVoiceMgr(VoiceMgr* voiceMgr)
{
    sVoiceMgr = voiceMgr;
}

void Voice::initialize_(u32 priority)
{
    mState = VoiceState::Stop;
    mPriority = priority;

    mVoiceParam.initialize();

    mSampleRate = 32000;
    mSampleFormat = SampleFormat::PcmS16;
}

void Voice::freeAllWaveBuffer_()
{
}

void Voice::VoiceImplDisposeCallback(VoiceImpl* voiceImpl, void* arg)
{
    Voice* voice = reinterpret_cast<Voice*>(arg);

    SEAD_ASSERT(voice != nullptr);
    SEAD_ASSERT(voice->mVoiceImpl == voiceImpl);

    voice->mVoiceImpl = nullptr;
}

} } // namespace snd::internal
