#pragma once

#include "BasicSoundPlayer.h"
#include "Channel.h"
#include "SoundThread.h"
#include "SequenceTrack.h"
#include "MoveValue.h"

#include "snd/snd_WaveArchiveFileReader.h"
#include "snd/snd_BankFileReader.h"
#include "snd/snd_SoundArchive.h"

#include <bfsar/BankFile.h>

class SequenceFile;
class SequenceSoundPlayer;

struct NoteOnInfo
{
    s32 prgNo;
    s32 key;
    s32 velocity;
    s32 length;
    s32 initPan;
    s32 priority;
    snd::internal::driver::Channel::ChannelCallback channelCallback;
    void* channelCallbackData;
    snd::UpdateType updateType;
};

class NoteOnCallback
{
    SEAD_NO_COPY(NoteOnCallback);

public:
    NoteOnCallback()
    {
    }

    virtual ~NoteOnCallback()
    {
    }

    virtual snd::internal::driver::Channel* noteOn(SequenceSoundPlayer* sequenceSoundPlayer, u8 bankIndex, const NoteOnInfo& noteOnInfo) = 0;
};

class SequenceSoundPlayer : public BasicSoundPlayer, public snd::internal::driver::SoundThread::PlayerCallback
{
public:
    struct ParserPlayerParam
    {
        u8 priority;
        u8 timebase;
        u16 tempo;
        snd::internal::MoveValue<u8, s16> volume;

        NoteOnCallback* callback;

        ParserPlayerParam()
            : priority(64)
            , timebase(cDefaultTimebase)
            , tempo(cDefaultTempo)
            , callback(nullptr)
        {
            volume.initValue(127);
        }
    };

    static const s32 cPlayerVariableNum = 16;
    static const s32 cGlobalVariableNum = 16;
    static const s32 cTrackNumPerPlayer = 16;
    static const s32 cVariableDefaultValue = -1;

    static const s32 cDefaultTimebase = 48;
    static const s32 cDefaultTempo = 120;

    static const s32 cDefaultSkipIntervalTick = 48 * 4 * 4;

public:
    SequenceSoundPlayer();

    void onUpdateFrameSoundThread() override
    {
        update();
    }

    void onUpdateFrameSoundThreadWithAudioFrameFrequency() override
    {
        if (mUpdateType == snd::UpdateType::AudioFrame)
            update();
    }

    void init();
    void deinit(bool stop = false);
    void setup(u32 allocTracks, NoteOnCallback* callback);
    void prepare(const void* seqFile, s32 seqOffset, const void** bankFiles, const void** warcFiles, const bool* warcIsIndividuals, snd::UpdateType updateType = snd::UpdateType::AudioFrame);
    void prepare(const void* seqFile, s32 seqOffset, const BankFile** bankFiles, snd::UpdateType updateType = snd::UpdateType::AudioFrame);
    void prepare(const SequenceFile& seqFile, s32 seqOffset, const BankFile** bankFiles, snd::UpdateType updateType = snd::UpdateType::AudioFrame);

    void pause(bool flag) override;

    virtual void channelCallback(snd::internal::driver::Channel* channel) { SEAD_UNUSED(channel); }

    const ParserPlayerParam& getParserPlayerParam() const { return mParserParam; }
    ParserPlayerParam& getParserPlayerParam() { return mParserParam; }

    u32 getTickCounter() const { return mTickCounter; }

    snd::UpdateType getUpdateType() const
    {
        return mUpdateType;
    }

    bool isPlayingFile(const SequenceFile& file) const
    {
        return &file == mPlayingFile;
    }

    SequenceTrack* getPlayerTrack(s32 trackNo);
    const SequenceTrack* getPlayerTrack(s32 trackNo) const;
    void closeTrack(s32 trackNo, bool stop = false);
    void setPlayerTrack(s32 trackNo, SequenceTrack* track);

    void finishPlayer(bool stop = false);

    void updateChannelParam();
    s32 parseNextTick(bool doNoteOn);

    void update();
    void updateTick();
    void skipTick();

    snd::internal::driver::Channel* noteOn(u8 bankIndex, const NoteOnInfo& noteOnInfo);

    void setTempoRatio(f32 tempoRatio)
    {
        SEAD_ASSERT(tempoRatio >= 0.0f);
        mTempoRatio = tempoRatio;
    }

    f32 getTempoRatio() const
    {
        return mTempoRatio;
    }

    void setTrackVolume(u32 trackBitFlag, f32 volume);

    bool isReleasePriorityFix() const { return mReleasePriorityFixFlag; }

    f32 getPanRange() const { return mPanRange; }

    s16 getLocalVariable(s32 varNo) const
    {
        SEAD_ASSERT(0 <= varNo && varNo < cPlayerVariableNum);

        return mLocalVariable[varNo];
    }

    s16 getGlobalVariable(s32 varNo)
    {
        SEAD_ASSERT(0 <= varNo && varNo < cGlobalVariableNum);

        return sGlobalVariable[varNo];
    }

    void setLocalVariable(s32 varNo, s16 var)
    {
        SEAD_ASSERT(0 <= varNo && varNo < cPlayerVariableNum);

        mLocalVariable[varNo] = var;
    }

    void setGlobalVariable(s32 varNo, s16 var)
    {
        SEAD_ASSERT(0 <= varNo && varNo < cGlobalVariableNum);

        sGlobalVariable[varNo] = var;
    }

    volatile s16* getVariablePtr(s32 varNo)
    {
        SEAD_ASSERT(0 <= varNo && varNo < cPlayerVariableNum + cGlobalVariableNum);

        if (varNo < cPlayerVariableNum)
            return &mLocalVariable[varNo];
        else if (varNo < cPlayerVariableNum + cGlobalVariableNum)
            return &sGlobalVariable[varNo - cPlayerVariableNum];
        else
            return nullptr;
    }

    const nw::snd::internal::BankFileReader& getBankFileReader(u8 bankIndex) const
    {
        return mBankFileReader[bankIndex];
    }

    const nw::snd::internal::WaveArchiveFileReader& getWaveArchiveFileReader(u8 bankIndex) const
    {
        // Waveform archives have a 1:1 correspondence with banks.
        return mWarcFileReader[bankIndex];
    }

    const BankFile* getBankFile(u8 bankIndex)
    {
        return mBankFile[bankIndex];
    }

    f32 calcTickPerMinute() const { return mParserParam.timebase * mParserParam.tempo * mTempoRatio; }
    f32 calcTickPerMsec() const { return calcTickPerMinute() / (60.0f * 1000.0f); }

    static volatile s16 sGlobalVariable[cGlobalVariableNum];
    static volatile s32 sSkipIntervalTickPerFrame;

    //? Get track reference even if it's not active
    const SequenceTrack& getTrack_(s32 trackNo) const
    {
        SEAD_ASSERT(0 <= trackNo && trackNo < cTrackNumPerPlayer);
        return mTrackInstances[trackNo];
    }

protected:
    //bool mFinishFlag;

    SequenceTrack mTrackInstances[cTrackNumPerPlayer];
    SequenceTrack* mTracks[cTrackNumPerPlayer];

    bool mReleasePriorityFixFlag;

    f32 mPanRange;
    f32 mTempoRatio;
    f32 mTickFraction;
    u32 mSkipTickCounter;
    f32 mSkipTimeCounter;

    ParserPlayerParam mParserParam;

    nw::snd::internal::WaveArchiveFileReader mWarcFileReader[nw::snd::SoundArchive::SEQ_BANK_MAX];
    nw::snd::internal::BankFileReader mBankFileReader[nw::snd::SoundArchive::SEQ_BANK_MAX];

    const BankFile* mBankFile[nw::snd::SoundArchive::SEQ_BANK_MAX];

    volatile s16 mLocalVariable[cPlayerVariableNum];
    volatile u32 mTickCounter;

    bool mIsRegisterPlayerCallback;

    snd::UpdateType mUpdateType;

    const SequenceFile* mPlayingFile;
};
