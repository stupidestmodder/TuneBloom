#include <bfsar/SoundPlayer.h>

#include <ui/PopupMgr.h>
#include <ui/UI.h>

#include <snd/MultiVoiceMgr.h>
#include <snd/SoundSystem.h>

void SoundPlayer::stopAllPlayers(bool stop)
{
    snd::internal::driver::SoundThreadLock lock;
    stopAllPlayersWithoutLock(stop);
}

void SoundPlayer::stopAllPlayersWithoutLock(bool stop)
{
    mSequencePlayer.deinit(stop);
    mStreamPlayer.deinit();
    mWavePlayer.deinit(stop);
}

void SoundPlayer::stopAllVoices()
{
    snd::internal::driver::SoundThreadLock lock;
    snd::internal::driver::MultiVoiceMgr::instance()->stopAllVoices();
}

bool SoundPlayer::playSound(const Sound* sound, u32 startOffsetSample)
{
    SEAD_ASSERT(sound);
    mLastPlayedSound = sound;

    switch (sound->getSoundType())
    {
        case Sound::SoundType::Seq:
            return playSeqSound(sound);

        case Sound::SoundType::Strm:
            return playStrmSound(sound);

        case Sound::SoundType::Wave:
            return playWaveSound(sound, startOffsetSample);
    }

    return false;
}

bool SoundPlayer::playLastSound()
{
    if (mLastPlayedSound)
    {
        return playSound(mLastPlayedSound);
    }

    return false;
}

bool SoundPlayer::playSeqSound(const Sound* sound)
{
    const Sound::SequenceSoundInfo& seqSoundInfo = sound->getSequenceSoundInfo();

    const Item* seqFileItem = seqSoundInfo.getSequenceFileRef().getItem();
    if (!seqFileItem)
    {
        PopupMgr::instance()->addPopup({ "No Sequence File attached", nullptr });
        //SEAD_PRINT("No sequence file attached\n");
        return false;
    }

    SEAD_ASSERT(seqFileItem->getItemType() == Item::ItemType::SequenceFile);

    const SequenceFile& seqFile = *static_cast<const SequenceFile*>(seqFileItem);

    const Bank* banks[nw::snd::SoundArchive::SEQ_BANK_MAX];
    for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
    {
        banks[i] = nullptr;
    }

    for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
    {
        const Item* item = seqSoundInfo.getBankRef(i).getItem();
        if (!item)
        {
            continue;
        }

        SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);
        const Bank* bank = static_cast<const Bank*>(item);

        banks[i] = bank;
    }

    if (!playSeqFile(seqFile, seqSoundInfo.getStartLabel(), banks, sound->getVolume()))
    {
        return false;
    }

    sSelectedItem = const_cast<Sound*>(sound);

    return true;
}

bool SoundPlayer::playStrmSound(const Sound* sound)
{
    if (sound->getStreamSoundInfo().getStreamType() != Sound::StreamSoundInfo::StreamType::NwStreamBinary)
    {
        PopupMgr::instance()->addPopup({ "Only BFSTM streams are supported", nullptr });
        //SEAD_PRINT("Only BFSTM sounds are supported atm\n");
        return false;
    }

    const Sound::StreamSoundInfo& strmSoundInfo = sound->getStreamSoundInfo();

    if (strmSoundInfo.getTrackList().isEmpty())
    {
        PopupMgr::instance()->addPopup({ "Streams must have at least 1 track", nullptr });
        return false;
    }

    if (strmSoundInfo.getTrackList().size() > 8)
    {
        PopupMgr::instance()->addPopup({ "Streams can only have up to 8 tracks", nullptr });
        return false;
    }

    StreamSoundPlayer::SetupArg setupArg;
    setupArg.allocChannelCount = strmSoundInfo.getAllocateChannelCount();
    setupArg.allocTrackFlag = strmSoundInfo.getAllocateTrackFlags();

    if (sBfsar.isStreamSendAvailable())
    {
        setupArg.pitch = strmSoundInfo.getPitch();
        setupArg.mainSend = strmSoundInfo.getMainSend();
        for (u32 i = 0; i < nw::snd::AUX_BUS_NUM; i++)
        {
            setupArg.fxSend[i] = strmSoundInfo.getFxSend(i);
        }
    }
    else
    {
        setupArg.pitch = 1.0f;
        setupArg.mainSend = 127;
        for (u32 i = 0; i < nw::snd::AUX_BUS_NUM; i++)
        {
            setupArg.fxSend[i] = 0;
        }
    }

    WaveFile::Encoding mainEncoding = WaveFile::Encoding::DspAdpcm;
    u32 mainSampleRate = 0;

    u32 channelIdxStart = 0;
    for (u32 i = 0; i < strmSoundInfo.getTrackList().size(); i++)
    {
        nw::snd::SoundArchive::StreamTrackInfo& tmp = setupArg.tracks[i];

        const Item* trackItem = strmSoundInfo.getTrackList().nth(i)->val();
        const Sound::StreamSoundInfo::Track& trackInfo = *static_cast<const Sound::StreamSoundInfo::Track*>(trackItem);

        if (!trackInfo.getWaveFileRef().isAttached())
        {
            PopupMgr::instance()->addPopup({ sead::FormatFixedSafeString<64>("Track %u has no Wave File attached", i).cstr(), nullptr });
            return false;
        }

        const WaveFile& waveFile = *static_cast<const WaveFile*>(trackInfo.getWaveFileRef().getItem());

        if (i == 0)
        {
            mainEncoding = waveFile.getEncoding();
            mainSampleRate = waveFile.getSampleRate();
        }
        else
        {
            if (mainEncoding != waveFile.getEncoding())
            {
                PopupMgr::instance()->addPopup({ "All stream tracks must have the same encoding", nullptr });
                return false;
            }

            if (mainSampleRate != waveFile.getSampleRate())
            {
                PopupMgr::instance()->addPopup({ "All stream tracks must have the same sample rate", nullptr });
                return false;
            }
        }

        tmp.volume = trackInfo.getVolume();
        tmp.pan = trackInfo.getPan();
        tmp.span = trackInfo.getSPan();
        tmp.flags = trackInfo.getFlags();
        tmp.channelCount = static_cast<u8>(trackInfo.getChannelCount());

        if (sBfsar.isStreamSendAvailable())
        {
            tmp.mainSend = trackInfo.getMainSend();
            for (u32 j = 0; j < nw::snd::AUX_BUS_NUM; j++)
            {
                tmp.fxSend[j] = trackInfo.getFxSend(j);
            }
        }
        else
        {
            tmp.mainSend = 127;
            for (u32 j = 0; j < nw::snd::AUX_BUS_NUM; j++)
            {
                tmp.fxSend[j] = 0;
            }
        }

        if (sBfsar.isFilterSupportedVersion())
        {
            tmp.lpfFreq = trackInfo.getLpfFreq();
            tmp.biquadType = trackInfo.getBiquadType();
            tmp.biquadValue = trackInfo.getBiquadValue();
        }
        else
        {
            tmp.lpfFreq = 64;
            tmp.biquadType = 0;
            tmp.biquadValue = 0;
        }

        u32 count = sead::Mathu::min(static_cast<u32>(tmp.channelCount), nw::snd::WAVE_CHANNEL_MAX);
        for (u32 ch = 0; ch < count; ch++)
        {
            tmp.globalChannelIndex[ch] = channelIdxStart + ch;
        }

        channelIdxStart += count;
    }

    {
        snd::internal::driver::SoundThreadLock lock;
        stopAllPlayersWithoutLock(false);

        mStreamPlayer.init();

        mStreamPlayer.setInitialVolume(static_cast<f32>(sound->getVolume()) / 127.0f);
        mStreamPlayer.setVolume(mVolume);

        mStreamPlayer.setup(setupArg);
        mStreamPlayer.prepare(strmSoundInfo);

        mCurrentPlayer = &mStreamPlayer;

        mSampleCount = mStreamPlayer.getSampleCount();
        mSampleRate = mStreamPlayer.getSampleRate();
    }

    sSelectedItem = const_cast<Sound*>(sound);

    return true;
}

bool SoundPlayer::playWaveSound(const Sound* sound, u32 startOffsetSample)
{
    const Item* waveFile = sound->getWaveSoundInfo().getWaveFileRef().getItem();
    if (!waveFile)
    {
        PopupMgr::instance()->addPopup({ "No Wave File attached", nullptr });
        //SEAD_PRINT("No wave file attached\n");
        return false;
    }

    playWaveFile(*static_cast<const WaveFile*>(waveFile), -1, sound, startOffsetSample);

    sSelectedItem = const_cast<Sound*>(sound);

    return true;
}

bool SoundPlayer::playSeqFile(const SequenceFile& seqFile, const sead::SafeString& startLabel, const Bank** bankArray, u8 volume)
{
    if (!seqFile.isValid())
    {
        PopupMgr::instance()->addPopup({ "Sequence File is not compiled", nullptr });
        //SEAD_PRINT("SequenceFile is not valid\n");
        return false;
    }

    u32 startOffset = seqFile.getLabelOffset(startLabel);
    if (startOffset == SequenceFile::cInvaldOffset)
    {
        PopupMgr::instance()->addPopup({ sead::FormatFixedSafeString<64>("Couldn't find start label '%s' in Sequence File", startLabel.cstr()).cstr(), nullptr });
        //SEAD_PRINT("Couldn't find start label '%s' in Sequence File\n", startLabel.cstr());
        return false;
    }

    u32 allocTracks = seqFile.getLabelAllocTracks(startLabel);

    const BankFile* banks[nw::snd::SoundArchive::SEQ_BANK_MAX];
    for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
    {
        banks[i] = nullptr;
    }

    for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
    {
        const Item* item = bankArray[i];
        if (!item)
        {
            continue;
        }

        SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);
        const Bank* bank = static_cast<const Bank*>(item);

        item = bank->getFileRef().getItem();
        if (!item)
        {
            continue;
        }

        SEAD_ASSERT(item->getItemType() == Item::ItemType::BankFile);
        const BankFile* bankFile = static_cast<const BankFile*>(item);

        banks[i] = bankFile;
    }

    {
        snd::internal::driver::SoundThreadLock lock;
        stopAllPlayersWithoutLock(false);

        mSequencePlayer.init();

        mSequencePlayer.setInitialVolume(static_cast<f32>(volume) / 127.0f);
        mSequencePlayer.setVolume(mVolume);

        mSequencePlayer.setup(allocTracks, &mSequenceNoteOnCallback2);
        mSequencePlayer.prepare(seqFile, startOffset, banks);

        mCurrentPlayer = &mSequencePlayer;

        mSampleCount = 0;
        mSampleRate = 0;

        // //? Init vars
        // for (s32 i = 0; i < SequenceSoundPlayer::cPlayerVariableNum; i++)
        // {
        //     const SeqVarInfo& varInfo = sPlayerVars[i];
        //     if (varInfo.enable)
        //     {
        //         sSequencePlayer.setLocalVariable(i, varInfo.value);
        //     }
        // }

        // for (s32 trackNo = 0; trackNo < SequenceSoundPlayer::cTrackNumPerPlayer; trackNo++)
        // {
        //     for (s32 i = 0; i < SequenceTrack::cTrackVariableNum; i++)
        //     {
        //         const SeqVarInfo& varInfo = sTrackVars[trackNo][i];
        //         if (varInfo.enable)
        //         {
        //             sSequencePlayer.getTrack_(trackNo).setTrackVariable(i, varInfo.value);
        //         }
        //     }
        // }
    }

    return true;
}

bool SoundPlayer::playWaveFile(const WaveFile& wave, s32 channel, const Sound* sound, u32 startOffsetSample)
{
    SEAD_ASSERT(wave.getItemType() == Item::ItemType::WaveFile);

    if (wave.getChannels().isEmpty())
    {
        PopupMgr::instance()->addPopup({ "Wave File has no channels", nullptr });
        return false;
    }

    {
        snd::internal::driver::SoundThreadLock lock;
        stopAllPlayersWithoutLock(false);

        mWavePlayer.init();

        if (sound)
        {
            mWavePlayer.setInitialVolume(static_cast<f32>(sound->getVolume()) / 127.0f);
        }

        mWavePlayer.setVolume(mVolume);

        mWavePlayer.prepare(wave, channel, sound, startOffsetSample);

        mCurrentPlayer = &mWavePlayer;

        mSampleCount = mWavePlayer.getSampleCount();
        mSampleRate = mWavePlayer.getSampleRate();
    }

    sSelectedItem = const_cast<WaveFile*>(&wave);

    return true;
}

bool SoundPlayer::playBankNote(u8 key, u8 velocity, const BankFile::VelocityRegion& velocityRegion)
{
    const Item* waveFile = velocityRegion.getWaveFileRef().getItem();
    if (!waveFile)
    {
        return false;
    }

    snd::internal::driver::SoundThreadLock lock;
    stopAllPlayersWithoutLock(false);

    mWavePlayer.init();

    mWavePlayer.setVolume(mVolume);

    mWavePlayer.prepare(*static_cast<const WaveFile*>(waveFile));
    mWavePlayer.setBankNoteInfo(key, velocity, velocityRegion);

    mCurrentPlayer = &mWavePlayer;

    mSampleCount = mWavePlayer.getSampleCount();
    mSampleRate = mWavePlayer.getSampleRate();

    return true;
}

void SoundPlayer::pause(bool isPause)
{
    if (isCurrentPlayer())
    {
        snd::internal::driver::SoundThreadLock lock;
        mCurrentPlayer->pause(isPause);
    }
}

bool SoundPlayer::seek(f32 progress)
{
    progress = sead::Mathf::clamp2(0.0f, progress, 1.0f);

    Item* selectedItem = sSelectedItem;

    if (mWavePlayer.isActive())
    {
        snd::internal::driver::SoundThreadLock lock;
        mWavePlayer.seek(mSampleCount * progress);
    }

    if (mStreamPlayer.isActive())
    {
        snd::internal::driver::SoundThreadLock lock;
        mStreamPlayer.seek(mSampleCount * progress);
    }

    sSelectedItem = selectedItem;

    return true;
}

void SoundPlayer::reset()
{
    resetLastPlayedSound();

    mSampleRate = 0;
    mSampleCount = 0;

    stopAllPlayers(true);
    stopAllVoices();
}

void SoundPlayer::setVolume(f32 volume)
{
    mVolume = volume;

    if (isCurrentPlayer())
    {
        snd::internal::driver::SoundThreadLock lock;
        mCurrentPlayer->setVolume(volume);
    }
}

void SoundPlayer::invalidateBankFile(const BankFile& bankFile)
{
    snd::internal::driver::SoundThreadLock lock;
    mSequencePlayer.invalidateBankFile(bankFile);
}

s32 SoundPlayer::getPlaySamplePosition() const
{
    s32 currentSample = 0;

    if (mWavePlayer.isActive())
    {
        currentSample = mWavePlayer.getPlaySamplePosition(true);
    }

    if (mStreamPlayer.isActive())
    {
        currentSample = mStreamPlayer.getPlaySamplePosition(true);
    }

    return currentSample;
}
