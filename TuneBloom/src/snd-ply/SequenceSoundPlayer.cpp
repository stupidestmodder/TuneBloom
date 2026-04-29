#include "snd/SequenceSoundPlayer.h"

#include "snd/DisposeCallbackMgr.h"
#include "snd/HardwareMgr.h"
#include "snd/MmlParser.h"

#include "snd/snd_SequenceSoundFileReader.h"

#include <bfsar/SequenceFile.h>

MmlParser sMmlParser;

static const u32 INTERVAL_MSEC_DENOMINATOR = 0x10000;
static const u32 INTERVAL_MSEC_NUMERATOR   = snd::internal::driver::HardwareMgr::cSoundFrameIntervalMSEC * INTERVAL_MSEC_DENOMINATOR;

volatile s16 SequenceSoundPlayer::sGlobalVariable[cGlobalVariableNum];
volatile s32 SequenceSoundPlayer::sSkipIntervalTickPerFrame = cDefaultSkipIntervalTick;

SequenceSoundPlayer::SequenceSoundPlayer()
    : mReleasePriorityFixFlag(false)
    //, mFinishFlag(false)
    , mSkipTickCounter(0)
    , mSkipTimeCounter(0.0f)
    , mIsRegisterPlayerCallback(false)
{
    for (s32 varNo = 0; varNo < cPlayerVariableNum; varNo++)
    {
        mLocalVariable[varNo] = cVariableDefaultValue;
    }

    for (s32 i = 0; i < cTrackNumPerPlayer; i++)
    {
        mTracks[i] = nullptr;
    }
}

void SequenceSoundPlayer::init()
{
    BasicSoundPlayer::init();

    //mStartedFlag                  = false;
    mReleasePriorityFixFlag       = false;

    mTempoRatio                   = 1.0f;
    mTickFraction                 = 0.0f;
    mSkipTickCounter              = 0;
    mSkipTimeCounter              = 0.0f;
    //mDelayCount                   = 0;
    mPanRange                     = 1.0f;
    mTickCounter                  = 0;

    mParserParam.tempo            = cDefaultTempo;
    mParserParam.timebase         = cDefaultTimebase;
    mParserParam.volume.initValue(127);
    mParserParam.priority         = 64;
    mParserParam.callback         = nullptr;

    for (s32 varNo = 0; varNo < cPlayerVariableNum; varNo++)
    {
        mLocalVariable[varNo] = cVariableDefaultValue;
    }

    for (s32 trackNo = 0; trackNo < cTrackNumPerPlayer; trackNo++)
    {
        mTracks[trackNo] = nullptr;

        mTrackInstances[trackNo].initParam();
    }

    mIsRegisterPlayerCallback = false;
}

void SequenceSoundPlayer::deinit(bool stop)
{
    finishPlayer(stop);
}

void SequenceSoundPlayer::setup(u32 allocTracks, NoteOnCallback* callback)
{
    mParserParam.callback = callback;

    u32 trackBitMask = allocTracks;
    for (u32 trackNo = 0; trackNo < cTrackNumPerPlayer && trackBitMask != 0; trackNo++, trackBitMask >>= 1)
    {
        if ((trackBitMask & 1) == 0)
            continue;

        SequenceTrack* track = &mTrackInstances[trackNo];
        track->initParam();
        track->setMmlParser(&sMmlParser);
        track->setSequenceSoundPlayer(this);

        setPlayerTrack(trackNo, track);
    }
}

void SequenceSoundPlayer::prepare(const void* seqFile, s32 seqOffset, const void** bankFiles, const void** warcFiles, const bool* warcIsIndividuals, snd::UpdateType updateType)
{
    if (mActiveFlag)
    {
        finishPlayer();
    }

    SequenceTrack* seqTrack = getPlayerTrack(0);
    if (!seqTrack)
    {
        //finalize();
        return;
    }

    {
        SEAD_ASSERT(seqFile);
        nw::snd::internal::SequenceSoundFileReader reader(seqFile);
        const void* seqData = reader.GetSequenceData(); // Data block body.
        seqTrack->setSeqData(seqData, seqOffset, -1);
        seqTrack->open();
    }

    for (s32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
    {
        mBankFileReader[i].Initialize(bankFiles[i]);
        mWarcFileReader[i].Initialize(warcFiles[i], warcIsIndividuals[i]);
    }

    snd::internal::driver::DisposeCallbackMgr::instance()->registerDisposeCallback(this);
    snd::internal::driver::SoundThread::instance()->registerPlayerCallback(this);
    mIsRegisterPlayerCallback = true;
    mActiveFlag = true;

    mUpdateType = updateType;
}

void SequenceSoundPlayer::prepare(const SequenceFile& seqFile, s32 seqOffset, const BankFile** bankFiles, s32 origSeqOffset, snd::UpdateType updateType)
{
    if (mActiveFlag)
    {
        finishPlayer();
    }

    if (!seqFile.isValid())
    {
        finishPlayer();
        return;
    }

    SequenceTrack* seqTrack = getPlayerTrack(0);
    if (!seqTrack)
    {
        //finalize();
        return;
    }

    {
        const void* seqData = seqFile.getSeqBytes();
        SEAD_ASSERT(seqData);

        seqTrack->setSeqData(seqData, seqOffset, origSeqOffset);
        seqTrack->open();
    }

    for (s32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
    {
        mBankFile[i] = bankFiles[i];
    }

    snd::internal::driver::DisposeCallbackMgr::instance()->registerDisposeCallback(this);
    snd::internal::driver::SoundThread::instance()->registerPlayerCallback(this);
    mIsRegisterPlayerCallback = true;
    mActiveFlag = true;

    mUpdateType = updateType;

    mPlayingFile = &seqFile;
}

void SequenceSoundPlayer::pause(bool flag)
{
    mPauseFlag = flag;

    for (s32 trackNo = 0; trackNo < cTrackNumPerPlayer; trackNo++)
    {
        SequenceTrack* track = getPlayerTrack(trackNo);
        if (!track)
            continue;

        track->pauseAllChannel(flag);
    }
}

SequenceTrack* SequenceSoundPlayer::getPlayerTrack(s32 trackNo)
{
    if (trackNo > cTrackNumPerPlayer - 1 )
        return nullptr;

    return mTracks[trackNo];
}

const SequenceTrack* SequenceSoundPlayer::getPlayerTrack(s32 trackNo) const
{
    if (trackNo > cTrackNumPerPlayer - 1 )
        return nullptr;

    return mTracks[trackNo];
}

void SequenceSoundPlayer::closeTrack(s32 trackNo, bool stop)
{
    SEAD_ASSERT(0 <= trackNo && trackNo < cTrackNumPerPlayer);

    SequenceTrack* track = getPlayerTrack(trackNo);
    if (!track)
        return;

    track->close(stop);
    mTracks[trackNo] = nullptr;
}

void SequenceSoundPlayer::setPlayerTrack(s32 trackNo, SequenceTrack* track)
{
    if (trackNo > cTrackNumPerPlayer - 1)
        return;

    mTracks[trackNo] = track;
    track->setPlayerTrackNo(trackNo);
}

void SequenceSoundPlayer::finishPlayer(bool stop)
{
    if (mIsRegisterPlayerCallback)
    {
        snd::internal::driver::SoundThread::instance()->unregisterPlayerCallback(this);
        snd::internal::driver::DisposeCallbackMgr::instance()->unregisterDisposeCallback(this);
        mIsRegisterPlayerCallback = false;
    }

    //if (mStartedFlag)
    {
        //mStartedFlag = false;
    }

    // Release all tracks
    for (s32 trackNo = 0; trackNo < cTrackNumPerPlayer; trackNo++)
    {
        closeTrack(trackNo, stop);
    }

    mActiveFlag = false;
}

void SequenceSoundPlayer::updateChannelParam()
{
    for (s32 trackNo = 0; trackNo < cTrackNumPerPlayer; trackNo++)
    {
        SequenceTrack* track = getPlayerTrack(trackNo);
        if (!track)
            continue;

        track->updateChannelParam();
    }
}

s32 SequenceSoundPlayer::parseNextTick(bool doNoteOn)
{
    mParserParam.volume.update();

    bool activeFlag = false;

    for (s32 trackNo = 0; trackNo < cTrackNumPerPlayer ; trackNo++)
    {
        SequenceTrack* track = getPlayerTrack(trackNo);
        if (!track)
            continue;

        track->updateChannelLength();

        if (track->parseNextTick(doNoteOn) < 0)
            closeTrack(trackNo);

        if (track->isOpened())
            activeFlag = true;
    }

    if (!activeFlag)
        return 1;

    return 0;
}

void SequenceSoundPlayer::update()
{
    if (!mActiveFlag)
        return;

    if (mSkipTickCounter > 0 || mSkipTimeCounter > 0.0f)
        skipTick();
    else if (!mPauseFlag)
        updateTick();

    updateChannelParam();
}

void SequenceSoundPlayer::updateTick()
{
    f32 tickPerMsec = calcTickPerMsec();
    if (tickPerMsec == 0.0f)
        return;

    u64 restMsec = INTERVAL_MSEC_NUMERATOR;
    u64 nextMsec = static_cast<u64>(INTERVAL_MSEC_DENOMINATOR * mTickFraction / tickPerMsec);

    while (nextMsec < restMsec)
    {
        restMsec -= nextMsec;

        bool result = parseNextTick(true) != 0;

        if (result)
        {
            finishPlayer();
            //mFinishFlag = true;
            return;
        }

        mTickCounter++;

        tickPerMsec = calcTickPerMsec();
        if (tickPerMsec == 0.0f)
            return;

        nextMsec = static_cast<u64>(INTERVAL_MSEC_DENOMINATOR / tickPerMsec);
    }

    nextMsec -= restMsec;
    mTickFraction = nextMsec * tickPerMsec / INTERVAL_MSEC_DENOMINATOR;
}

void SequenceSoundPlayer::skipTick()
{
    for (s32 trackNo = 0; trackNo < cTrackNumPerPlayer; trackNo++)
    {
        SequenceTrack* track = getPlayerTrack(trackNo);
        if (!track)
            continue;

        track->releaseAllChannel(SequenceTrack::cPauseReleaseValue);
        track->freeAllChannel();
    }

    s32 skipCount = 0;
    while (mSkipTickCounter > 0 || mSkipTimeCounter * calcTickPerMsec() >= 1.0f)
    {
        if (skipCount >= sSkipIntervalTickPerFrame)
            return;

        if (mSkipTickCounter > 0)
        {
            mSkipTickCounter--;
        }
        else
        {
            f32 tickPerMsec = calcTickPerMsec();
            SEAD_ASSERT(tickPerMsec > 0.0f); // do not skip for values of 0.0f
            f32 msecPerTick = 1.0f / tickPerMsec;
            mSkipTimeCounter -= msecPerTick;
        }

        if (parseNextTick(false) != 0)
        {
            finishPlayer();
            //mFinishFlag = true;
            return;
        }

        skipCount++;
        mTickCounter++;
    }

    mSkipTimeCounter = 0.0f;
}

snd::internal::driver::Channel* SequenceSoundPlayer::noteOn(u8 bankIndex, const NoteOnInfo& noteOnInfo)
{
    snd::internal::driver::Channel* channel = mParserParam.callback->noteOn(
        this,
        bankIndex,
        noteOnInfo
    );

    return channel;
}

void SequenceSoundPlayer::invalidateData(const void* start, const void* end)
{
    if (mActiveFlag)
    {
        for (s32 trackNo = 0; trackNo < cTrackNumPerPlayer; trackNo++)
        {
            SequenceTrack* track = getPlayerTrack(trackNo);
            if (!track)
            {
                continue;
            }

            const u8* cur = track->getParserTrackParam().baseAddr;
            if (start <= cur && cur <= end)
            {
                finishPlayer();
                //finalize();
                break;
            }
        }

        for (s32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
        {
            const void* cur = mBankFileReader[i].GetBankFileAddress();
            if (start <= cur && cur <= end)
            {
                mBankFileReader[i].Finalize();
            }
        }
    }
}

void SequenceSoundPlayer::initSequenceSoundPlayer()
{
    for (s32 variableNo = 0; variableNo < cGlobalVariableNum; variableNo++)
    {
        sGlobalVariable[variableNo] = cVariableDefaultValue;
    }
}
