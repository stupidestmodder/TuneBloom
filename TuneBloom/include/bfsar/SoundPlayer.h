#pragma once

#include <snd/SequenceNoteOnCallback.h>
#include <snd/SequenceSoundPlayer.h>
#include <snd/StreamSoundPlayer.h>
#include <snd/WaveSoundPlayer.h>

class SoundPlayer
{
public:
    static const u32 cMaxTracks = 16;

public:
    SoundPlayer()
        : mVolume(1.0f)
        , mPitch(1.0f)
        , mPan(0.0f)
        , mLPF(0.0f)
        , mBiquadType((s8)snd::BiquadFilterType::Inherit)
        , mBiquadValue(0.0f)
        , mSeqTempoRatio(1.0f)

        , mPlayingSound(nullptr)
        , mLastPlayedSound(nullptr)
        , mPlayingWaveFile(nullptr)
        , mCurrentPlayer(nullptr)
        , mSampleRate(0)
        , mSampleCount(0)
    {
        for (u32 i = 0; i < cMaxTracks; i++)
        {
            mTrackVolume[i] = 1.0f;
        }
    }

    ~SoundPlayer()
    {
        // reset();
    }

    void update();

    void stopAllPlayers(bool stop);
    void stopAllPlayersWithoutLock(bool stop);
    void stopAllVoices();

    bool playSound(const Sound* sound, u32 startOffsetSample = 0);
    bool playLastSound();

    bool playSeqSound(const Sound* sound);
    bool playStrmSound(const Sound* sound);
    bool playWaveSound(const Sound* sound, u32 startOffsetSample = 0);

    bool playSeqFile(const SequenceFile& seqFile, const sead::SafeString& startLabel, const Bank** bankArray, u8 volume);
    bool playWaveFile(const WaveFile& wave, s32 channel = -1, const Sound* sound = nullptr, u32 startOffsetSample = 0);
    bool playBankNote(u8 key, u8 velocity, const BankFile::VelocityRegion& velocityRegion);

    void pause(bool isPause);
    bool seek(f32 progress);
    void reset();

    f32 getVolume() const { return mVolume; }
    void setVolume(f32 volume);

    const Sound* getPlayingSound() const { return mPlayingSound; }
    void resetPlayingSound() { mPlayingSound = nullptr; }
    const Sound* getLastPlayedSound() const { return mLastPlayedSound; }
    void resetLastPlayedSound() { mLastPlayedSound = nullptr; }
    const WaveFile* getPlayingWaveFile() const { return mPlayingWaveFile; }
    void resetPlayingWaveFile() { mPlayingWaveFile = nullptr; }

    void invalidateBankFile(const BankFile& bankFile);

    BasicSoundPlayer* getCurrentPlayer() { return mCurrentPlayer; }
    SequenceSoundPlayer& getSequencePlayer() { return mSequencePlayer; }
    StreamSoundPlayer& getStreamPlayer() { return mStreamPlayer; }
    WaveSoundPlayer& getWavePlayer() { return mWavePlayer; }

    u32 getSampleRate() const { return mSampleRate; }
    u32 getSampleCount() const { return mSampleCount; }
    s32 getPlaySamplePosition() const;

    bool isCurrentPlayer() const { return mCurrentPlayer != nullptr; }
    bool isCurrentPlayerSequence() const { return mCurrentPlayer == &mSequencePlayer; }
    bool isCurrentPlayerStream() const { return mCurrentPlayer == &mStreamPlayer; }
    bool isCurrentPlayerWave() const { return mCurrentPlayer == &mWavePlayer; }

    bool isPause() const
    {
        bool isPause = true;
        if (isActive())
        {
            isPause = mCurrentPlayer->isPause();
        }

        return isPause;
    }

    bool isActive() const
    {
        return isCurrentPlayer() && mCurrentPlayer->isActive();
    }

    void drawParameters();
    void drawSeqVars();

private:
    void initPlayerParam_();
    void initPlayerTrack_();
    void initSeqVars_();

    struct SeqVarInfo
    {
        SeqVarInfo()
            : enable(false), value(-1)
        {
        }

        bool enable;
        s16 value;
    };

private:
    f32 mVolume;
    f32 mPitch;
    f32 mPan;
    f32 mLPF;
    s32 mBiquadType;
    f32 mBiquadValue;
    f32 mTrackVolume[cMaxTracks];
    f32 mSeqTempoRatio;

    SeqVarInfo mGlobalVars[SequenceSoundPlayer::cGlobalVariableNum];
    SeqVarInfo mPlayerVars[SequenceSoundPlayer::cPlayerVariableNum];
    SeqVarInfo mTrackVars[SequenceSoundPlayer::cTrackNumPerPlayer][SequenceTrack::cTrackVariableNum];

    const Sound* mPlayingSound;
    const Sound* mLastPlayedSound;
    const WaveFile* mPlayingWaveFile;
    BasicSoundPlayer* mCurrentPlayer;
    u32 mSampleRate;
    u32 mSampleCount;

    SequenceSoundPlayer mSequencePlayer;
    StreamSoundPlayer mStreamPlayer;
    WaveSoundPlayer mWavePlayer;

    // SequenceNoteOnCallback mSequenceNoteOnCallback;
    SequenceNoteOnCallback2 mSequenceNoteOnCallback2;
};
