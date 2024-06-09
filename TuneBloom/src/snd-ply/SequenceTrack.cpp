#include "snd/SequenceTrack.h"

#include "snd/SequenceSoundPlayer.h"
#include "snd/MmlParser.h"
#include "snd/BankR.h"

#include <basis/seadWarning.h>
#include <math/seadMathCalcCommon.h>

SequenceTrack::SequenceTrack()
    : mOpenFlag(false)
    , mSequenceSoundPlayer(nullptr)
    , mChannelList(nullptr)
    , mParser(nullptr)
{
    initParam();
}

void SequenceTrack::setPlayerTrackNo(s32 playerTrackNo)
{
    SEAD_ASSERT(0 <= playerTrackNo && playerTrackNo < SequenceSoundPlayer::cTrackNumPerPlayer);
    mPlayerTrackNo = static_cast<u8>(playerTrackNo);
}

void SequenceTrack::setSeqData(const void* seqBase, s32 seqOffset)
{
    mParserTrackParam.baseAddr = static_cast<const u8*>(seqBase);
    mParserTrackParam.currentAddr = mParserTrackParam.baseAddr + seqOffset;
    mParserTrackParam.currentCmdAddr = mParserTrackParam.currentAddr; // Custom
}

void SequenceTrack::initParam()
{
    mExtVolume                  = 1.0f;
    mExtPitch                   = 1.0f;
    mPanRange                   = 1.0f;

    mParam.initialize();

    // Parser Param
    mParserTrackParam.baseAddr       = nullptr;
    mParserTrackParam.currentAddr    = nullptr;

    mParserTrackParam.currentCmdAddr = nullptr; // Custom

    mParserTrackParam.cmpFlag        = true;
    mParserTrackParam.noteWaitFlag   = true;
    mParserTrackParam.tieFlag        = false;
    mParserTrackParam.monophonicFlag = false;

    mParserTrackParam.callStackDepth = 0;

    mParserTrackParam.wait           = 0;

    mParserTrackParam.waitAmount     = 0; // Custom

    mParserTrackParam.muteFlag       = false;
    mParserTrackParam.silenceFlag    = false;
    mParserTrackParam.noteFinishWait = false;
    mParserTrackParam.portaFlag      = false;
    mParserTrackParam.damperFlag     = false;

    mParserTrackParam.bankIndex      = 0;
    mParserTrackParam.prgNo          = 0;

    for (s32 i = 0; i < snd::internal::driver::Channel::cModCount; i++)
    {
        mParserTrackParam.lfoParam[i].initialize();
        mParserTrackParam.lfoTarget[i] = (u8)snd::internal::driver::Channel::LfoTarget::Pitch;
    }

    mParserTrackParam.sweepPitch     = 0.0f;

    mParserTrackParam.volume.initValue(127);
    mParserTrackParam.volume2.initValue(127);
    mParserTrackParam.pan.initValue(0);
    mParserTrackParam.surroundPan.initValue(0);
    mParserTrackParam.velocityRange  = 127;
    mParserTrackParam.pitchBend.initValue(0);
    mParserTrackParam.bendRange      = cDefaultBendRange;
    mParserTrackParam.initPan        = 0;
    mParserTrackParam.transpose      = 0;
    mParserTrackParam.priority       = cDefaultPriority;
    mParserTrackParam.portaKey       = cDefaultPortaKey;
    mParserTrackParam.portaTime      = 0;

    mParserTrackParam.attack         = cInvalidEnvelope;
    mParserTrackParam.decay          = cInvalidEnvelope;
    mParserTrackParam.sustain        = cInvalidEnvelope;
    mParserTrackParam.release        = cInvalidEnvelope;
    mParserTrackParam.envHold        = cInvalidEnvelope;

    mParserTrackParam.mainSend       = 127;
    for (s32 i = 0; i < (u32)snd::AuxBus::Num; i++)
    {
        mParserTrackParam.fxSend[i] = 0;
    }

    mParserTrackParam.lpfFreq        = 0.0f;
    mParserTrackParam.biquadType     = 0;
    mParserTrackParam.biquadValue    = 0;
    mParserTrackParam.outputLine     = -1;

    for(s32 varNo = 0; varNo < cTrackVariableNum ; varNo++)
    {
        mTrackVariable[varNo] = SequenceSoundPlayer::cVariableDefaultValue;
    }
}

void SequenceTrack::open()
{
    mParserTrackParam.noteFinishWait = false;
    mParserTrackParam.callStackDepth = 0;
    mParserTrackParam.wait           = 0;

    mParserTrackParam.waitAmount     = 0; // Custom

    mOpenFlag = true;
}

void SequenceTrack::close(bool stop)
{
    releaseAllChannel(-1, stop);
    freeAllChannel();

    mOpenFlag = false;
}

void SequenceTrack::updateChannelLength()
{
    if (!mOpenFlag)
        return;

    snd::internal::driver::Channel* channel = mChannelList;
    while(channel)
    {
        if (channel->getLength() > 0)
            channel->setLength(channel->getLength() - 1);

        updateChannelRelease(channel);

        if (!channel->isAutoUpdateSweep())
            channel->updateSweep(1);

        channel = channel->getNextTrackChannel();
    }
}

void SequenceTrack::updateChannelRelease(snd::internal::driver::Channel* channel)
{
    if (channel->getLength() == 0 && !channel->isRelease())
    {
        if (!mParserTrackParam.damperFlag)
            channel->noteOff();
    }
}

s32 SequenceTrack::parseNextTick(bool doNoteOn)
{
    if (!mOpenFlag)
        return 0;

    // Update MoveValue
    mParserTrackParam.volume.update();
    mParserTrackParam.volume2.update();
    mParserTrackParam.pan.update();
    mParserTrackParam.surroundPan.update();
    mParserTrackParam.pitchBend.update();

    // Wait check
    if (mParserTrackParam.noteFinishWait)
    {
        if (mChannelList)
            return 1;

        mParserTrackParam.noteFinishWait = false;
    }

    if (mParserTrackParam.wait > 0)
    {
        mParserTrackParam.wait--;

        if (mParserTrackParam.wait > 0)
            return 1;
    }

    // Sequence operation
    if (mParserTrackParam.currentAddr)
    {
        s32 counter = 0;
        const s32 cCounterMax = 10000;

        while (mParserTrackParam.wait == 0 && !mParserTrackParam.noteFinishWait)
        {
            counter++;
            if (counter > cCounterMax)
            {
                SEAD_WARNING("cannot process %d SEQ command!\n", cCounterMax);
                return 1;
            }

            ParseResult result = parse(doNoteOn);
            if (result == PARSE_RESULT_FINISH)
                return -1;
        }
    }

    return 1;
}

void SequenceTrack::releaseAllChannel(s32 release, bool stop)
{
    updateChannelParam();

    snd::internal::driver::Channel* channel = mChannelList;
    while (channel)
    {
        if (channel->isActive())
        {
            if (release >= 0)
            {
                SEAD_ASSERT(0 <= release && release <= 127);
                channel->setRelease(static_cast<u8>(release));
            }

            channel->release();

            if (stop)
                channel->stop();
        }

        channel = channel->getNextTrackChannel();
    }
}

void SequenceTrack::addChannel(snd::internal::driver::Channel* channel)
{
    channel->setNextTrackChannel(mChannelList);
    mChannelList = channel;
}

void SequenceTrack::updateChannelParam()
{
    if (!mOpenFlag)
        return;

    if (!mChannelList)
        return;

    // volume
    f32 volume
        = mParserTrackParam.volume.getValue()
        * mParserTrackParam.volume2.getValue()
        * mSequenceSoundPlayer->getParserPlayerParam().volume.getValue()
        / (127.0f * 127.0f * 127.0f);
    volume = volume * volume * mExtVolume * mSequenceSoundPlayer->getVolume();

    f32 pitch = (mParserTrackParam.pitchBend.getValue() / 128.0f) * mParserTrackParam.bendRange;

    f32 pitchRatio = mSequenceSoundPlayer->getPitch() * mExtPitch;

    // lpf freq
    f32 lpfFreq
        = mParserTrackParam.lpfFreq
        + mSequenceSoundPlayer->getLpfFreq();

    // biquad filter
    s32 biquadType = mParserTrackParam.biquadType;
    f32 biquadValue = mParserTrackParam.biquadValue;
    if (static_cast<snd::BiquadFilterType>(mSequenceSoundPlayer->getBiquadFilterType()) != snd::BiquadFilterType::Inherit)
    {
        biquadType = mSequenceSoundPlayer->getBiquadFilterType();
        biquadValue = mSequenceSoundPlayer->getBiquadFilterValue();
    }

    // outputLine
    u32 outputLine = mSequenceSoundPlayer->getOutputLine();
    if (mParserTrackParam.outputLine != -1)
        outputLine = mParserTrackParam.outputLine;

    // Base data.
    f32 panBase =
        sead::Mathf::clamp2(-1.0f, static_cast<f32>(mParserTrackParam.pan.getValue()) / 63.0f, 1.0f)
        * mPanRange
        * mSequenceSoundPlayer->getPanRange();

    //f32 spanBase = sead::Mathf::clamp2(0.0f, static_cast<f32>(mParserTrackParam.surroundPan.getValue()) / 63.0f, 2.0f);

    f32 mainSendBase = static_cast<f32>(mParserTrackParam.mainSend) / 127.0f - 1.0f; // To -1.0 through 0.0
    f32 fxSendBase[(u32)snd::AuxBus::Num];
    for (s32 i = 0; i < (u32)snd::AuxBus::Num; i++)
    {
        fxSendBase[i] = static_cast<f32>(mParserTrackParam.fxSend[i]) / 127.0f;
    }

    // Parameters for output to TV
    snd::internal::OutputParam tvParam = mSequenceSoundPlayer->getParam();
    {
        tvParam.volume *= mParam.volume;
        for (s32 i = 0; i < snd::cWaveChannelMax; i++)
        {
            for (s32 j = 0; j < (s32)snd::ChannelIdx::Num; j++)
            {
                tvParam.mixParameter[i].ch[j] *= mParam.mixParameter[i].ch[j];
            }
        }

        tvParam.pan += panBase;
        tvParam.pan += mParam.pan;
        //tvParam.span += spanBase;
        //tvParam.span += mTvParam.span;
        tvParam.mainSend += mainSendBase;
        tvParam.mainSend += mParam.mainSend;

        for (s32 i = 0; i < (u32)snd::AuxBus::Num; i++)
        {
            tvParam.fxSend[i] += fxSendBase[i];
            tvParam.fxSend[i] += mParam.fxSend[i];
        }
    }

    snd::internal::driver::Channel* channel = mChannelList;
    while (channel)
    {
        channel->setUserVolume(volume);
        channel->setUserPitch(pitch);
        channel->setUserPitchRatio(pitchRatio);
        channel->setUserLpfFreq(lpfFreq);
        channel->setBiquadFilter(biquadType, biquadValue);
        channel->setOutputLine(outputLine);

        for (s32 i = 0; i < snd::internal::driver::Channel::cModCount; i++)
        {
            channel->setLfoParam(mParserTrackParam.lfoParam[i], i);
            channel->setLfoTarget(static_cast<snd::internal::driver::Channel::LfoTarget>(mParserTrackParam.lfoTarget[i]), i);
        }

        channel->setParam(tvParam);

        channel = channel->getNextTrackChannel();
    }
}

void SequenceTrack::freeAllChannel()
{
    snd::internal::driver::Channel* channel = mChannelList;

    while (channel)
    {
        snd::internal::driver::Channel::detachChannel(channel);
        channel = channel->getNextTrackChannel();
    }

    mChannelList = nullptr;
}

void SequenceTrack::pauseAllChannel(bool flag)
{
    snd::internal::driver::Channel* channel = mChannelList;

    while (channel)
    {
        if (channel->isActive() && channel->isPause() != flag)
            channel->pause(flag);

        channel = channel->getNextTrackChannel();
    }
}

snd::internal::driver::Channel* SequenceTrack::noteOn(s32 key, s32 velocity, s32 length, bool tieFlag)
{
    SEAD_ASSERT(0 <= key && key <= 127);
    SEAD_ASSERT(0 <= velocity && velocity <= 127);

    const SequenceSoundPlayer::ParserPlayerParam& playerParam = mSequenceSoundPlayer->getParserPlayerParam();

    snd::internal::driver::Channel* channel = nullptr;

    // apply the velocity range
    velocity = velocity * mParserTrackParam.velocityRange / 127;

    if (tieFlag)
    {
        // Thailand.
        channel = getLastChannel();
        if (channel)
        {
            channel->setKey(static_cast<u8>(key));
            channel->setVelocity(BankR::calcChannelVelocityVolume(static_cast<u8>(velocity)));
            // TODO: Update the pan too?
        }
    }

    if (getParserTrackParam().monophonicFlag)
    {
        // monophonic
        channel = getLastChannel();
        if (channel)
        {
            if (channel->isRelease())
            {
                channel->stop();
                channel->callChannelCallback(snd::internal::driver::Channel::ChannelCallbackStatus::Stopped);
                snd::internal::driver::Channel::freeChannel(channel);
                channel = nullptr;
            }
            else
            {
                channel->setKey(static_cast<u8>(key));
                channel->setVelocity(BankR::calcChannelVelocityVolume(static_cast<u8>(velocity)));
                channel->setLength(length);
            }
        }
    }

    if (!channel)
    {
        NoteOnInfo noteOnInfo = {
            mParserTrackParam.prgNo,
            key,
            velocity,
            tieFlag ? -1 : length,
            mParserTrackParam.initPan,
            playerParam.priority + mParserTrackParam.priority,
            SequenceTrack::channelCallbackFunc,
            this,
            mSequenceSoundPlayer->getUpdateType()
        };

        channel = mSequenceSoundPlayer->noteOn(
            mParserTrackParam.bankIndex,
            noteOnInfo
        );

        if (!channel)
            return nullptr;

        // Alternate assign processing
        if (channel->getKeyGroupId() > 0)
        {
            snd::internal::driver::Channel* itr = mChannelList;
            while (itr)
            {
                if (itr->getKeyGroupId() == channel->getKeyGroupId())
                {
                    itr->setRelease(126);
                    itr->release();
                }

                itr = itr->getNextTrackChannel();
            }
        }

        // Attach to channel list
        addChannel(channel);
    }

    // Initial parameter settings

    // Update the envelope.
    if (mParserTrackParam.attack <= SequenceTrack::cMaxEnvelopeValue)
    {
        channel->setAttack(mParserTrackParam.attack);
    }

    if (mParserTrackParam.decay <= SequenceTrack::cMaxEnvelopeValue)
    {
        channel->setDecay(mParserTrackParam.decay);
    }

    if (mParserTrackParam.sustain <= SequenceTrack::cMaxEnvelopeValue)
    {
        channel->setSustain(mParserTrackParam.sustain);
    }

    if (mParserTrackParam.release <= SequenceTrack::cMaxEnvelopeValue)
    {
        channel->setRelease(mParserTrackParam.release);
    }

    if (mParserTrackParam.envHold <= SequenceTrack::cMaxEnvelopeValue)
    {
        channel->setHold(mParserTrackParam.envHold);
    }

    // Sweep and portamento update
    f32 sweepPitch = mParserTrackParam.sweepPitch;

    if (mParserTrackParam.portaFlag)
    {
        sweepPitch += mParserTrackParam.portaKey - key;
    }

    if (mParserTrackParam.portaTime == 0)
    {
        if (!(length != 0))
            SEAD_WARNING("portatime zero is invalid.");

        channel->setSweepParam(sweepPitch, length, false);
    }
    else
    {
        s32 sweepTime = mParserTrackParam.portaTime;
        sweepTime *= sweepTime;
        sweepTime = static_cast<s32>(sweepTime * (sweepPitch >= 0 ? sweepPitch : -sweepPitch));
        sweepTime >>= 5;
        sweepTime *= 5; // In msec units

        channel->setSweepParam(sweepPitch, sweepTime, true);
    }

    // Update the portamento start key
    mParserTrackParam.portaKey = static_cast<u8>(key);

    // Other.
    channel->setSilence(mParserTrackParam.silenceFlag != 0, 0);
    channel->setReleasePriorityFix(mSequenceSoundPlayer->isReleasePriorityFix());
    channel->setPanMode(mSequenceSoundPlayer->getPanMode());
    channel->setPanCurve(mSequenceSoundPlayer->getPanCurve());
    //channel->setFrontBypass(mParserTrackParam.frontBypassFlag);
    //channel->setRemoteFilter(mSequenceSoundPlayer->getRemoteFilter());
    //channel->setVoiceRendererType(mSequenceSoundPlayer->getVoiceRendererType());

    return channel;
}

void SequenceTrack::channelCallbackFunc(snd::internal::driver::Channel* dropChannel, snd::internal::driver::Channel::ChannelCallbackStatus, void* userData)
{
    SequenceTrack* track = reinterpret_cast<SequenceTrack*>(userData);

    SEAD_ASSERT(dropChannel);
    SEAD_ASSERT(track);

    if (track->mSequenceSoundPlayer)
        track->mSequenceSoundPlayer->channelCallback(dropChannel);

    // Disconnect the channel reference
    if (track->mChannelList == dropChannel)
    {
        track->mChannelList = dropChannel->getNextTrackChannel();
        return;
    }

    snd::internal::driver::Channel* channel = track->mChannelList;
    SEAD_ASSERT(channel);

    while (channel->getNextTrackChannel())
    {
        if (channel->getNextTrackChannel() == dropChannel)
        {
            channel->setNextTrackChannel(dropChannel->getNextTrackChannel());
            return;
        }

        channel = channel->getNextTrackChannel();
    }

    SEAD_ASSERT(false);
}

SequenceTrack::ParseResult SequenceTrack::parse(bool doNoteOn)
{
    SEAD_ASSERT(mParser);

    return mParser->parse(
        this,
        doNoteOn
    );
}
