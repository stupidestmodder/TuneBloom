#include <bfsar/SoundPlayer.h>

#include <ui/PopupMgr.h>
#include <ui/UI.h>

#include <snd/MultiVoiceMgr.h>
#include <snd/SoundSystem.h>

#include <imgui/imgui.h>

void SoundPlayer::update()
{
    if (mPlayingSound && !isActive())
    {
        mPlayingSound = nullptr;
    }
}

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
    mPlayingSound = nullptr;

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
        Item* prev = sSelectedItem;
        bool ret = playSound(mLastPlayedSound);
        sSelectedItem = prev;

        return ret;
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
    mPlayingSound = sound;

    return true;
}

bool SoundPlayer::playStrmSound(const Sound* sound)
{
    if (sound->getStreamSoundInfo().getStreamType() != Sound::StreamSoundInfo::StreamType::NwStreamBinary)
    {
        PopupMgr::instance()->addPopup({ "Only BFSTM streams are supported", nullptr });
        return false;
    }

    const Sound::StreamSoundInfo& strmSoundInfo = sound->getStreamSoundInfo();

    if (strmSoundInfo.getTrackList().isEmpty())
    {
        PopupMgr::instance()->addPopup({ "Streams must have at least 1 Track", nullptr });
        return false;
    }

    if (strmSoundInfo.getTrackList().size() > 8)
    {
        PopupMgr::instance()->addPopup({ "Streams can only have up to 8 Tracks", nullptr });
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

    const WaveFile* mainWave = nullptr;
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

        if (waveFile.getChannels().isEmpty())
        {
            PopupMgr::instance()->addPopup({ sead::FormatFixedSafeString<64>("Track %u Wave File has no channels", i).cstr(), nullptr });
            return false;
        }

        const_cast<WaveFile&>(waveFile).updateLoop();

        if (i == 0)
        {
            mainWave = &waveFile;
            mainEncoding = waveFile.getEncoding();
            mainSampleRate = waveFile.getSampleRate();
        }
        else
        {
            if (mainEncoding != waveFile.getEncoding())
            {
                PopupMgr::instance()->addPopup({ "All Stream Tracks must have the same encoding", nullptr });
                return false;
            }

            if (mainSampleRate != waveFile.getSampleRate())
            {
                PopupMgr::instance()->addPopup({ "All Stream Tracks must have the same sample rate", nullptr });
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
        mCurrentPlayer = &mStreamPlayer;

        mStreamPlayer.setInitialVolume(static_cast<f32>(sound->getVolume()) / 127.0f);

        mStreamPlayer.setup(setupArg);
        mStreamPlayer.prepare(strmSoundInfo);
        initPlayerParam_();
        initPlayerTrack_();

        mSampleCount = mStreamPlayer.getSampleCount();
        mSampleRate = mStreamPlayer.getSampleRate();

        mPlayingWaveFile = mainWave;
    }

    sSelectedItem = const_cast<Sound*>(sound);
    mPlayingSound = sound;

    return true;
}

bool SoundPlayer::playWaveSound(const Sound* sound, u32 startOffsetSample)
{
    const Item* waveFile = sound->getWaveSoundInfo().getWaveFileRef().getItem();
    if (!waveFile)
    {
        PopupMgr::instance()->addPopup({ "No Wave File attached", nullptr });
        return false;
    }

    if (!playWaveFile(*static_cast<const WaveFile*>(waveFile), -1, sound, startOffsetSample))
    {
        return false;
    }

    sSelectedItem = const_cast<Sound*>(sound);
    mPlayingSound = sound;

    return true;
}

bool SoundPlayer::playSeqFile(const SequenceFile& seqFile, const sead::SafeString& startLabel, const Bank** bankArray, u8 volume)
{
    if (!seqFile.isValid())
    {
        PopupMgr::instance()->addPopup({ "Sequence File is not compiled", nullptr });
        return false;
    }

    u32 startOffset = seqFile.getLabelOffset(startLabel);
    if (startOffset == SequenceFile::cInvaldOffset)
    {
        PopupMgr::instance()->addPopup({ sead::FormatFixedSafeString<128>("Couldn't find start label in Sequence File\n'%s'", startLabel.cstr()).cstr(), nullptr });
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
        mCurrentPlayer = &mSequencePlayer;

        mSequencePlayer.setInitialVolume(static_cast<f32>(volume) / 127.0f);

        s32 origSeqOffset = seqFile.getLabelOffset(startLabel, false);
        if (origSeqOffset == startOffset)
        {
            origSeqOffset = -1;
        }

        mSequencePlayer.setup(allocTracks, &mSequenceNoteOnCallback2);
        mSequencePlayer.prepare(seqFile, startOffset, banks, origSeqOffset);
        initPlayerParam_();
        initPlayerTrack_();
        initSeqVars_();

        mSampleCount = 0;
        mSampleRate = 0;

        mPlayingSound = nullptr;
        mPlayingWaveFile = nullptr;
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

    const_cast<WaveFile&>(wave).updateLoop();

    {
        snd::internal::driver::SoundThreadLock lock;
        stopAllPlayersWithoutLock(false);

        mWavePlayer.init();
        mCurrentPlayer = &mWavePlayer;

        if (sound)
        {
            mWavePlayer.setInitialVolume(static_cast<f32>(sound->getVolume()) / 127.0f);
        }

        mWavePlayer.prepare(wave, channel, sound, startOffsetSample);
        initPlayerParam_();

        mSampleCount = mWavePlayer.getSampleCount();
        mSampleRate = mWavePlayer.getSampleRate();

        mPlayingSound = nullptr;
        mPlayingWaveFile = &wave;
    }

    sSelectedItem = const_cast<WaveFile*>(&wave);

    return true;
}

bool SoundPlayer::playBankNote(u8 key, u8 velocity, const BankFile::VelocityRegion& velocityRegion)
{
    const WaveFile* waveFile = static_cast<const WaveFile*>(velocityRegion.getWaveFileRef().getItem());
    if (!waveFile)
    {
        return false;
    }

    if (waveFile->getChannels().isEmpty())
    {
        PopupMgr::instance()->addPopup({ "Wave File has no channels", nullptr });
        return false;
    }

    const_cast<WaveFile*>(waveFile)->updateLoop();

    {
        snd::internal::driver::SoundThreadLock lock;
        stopAllPlayersWithoutLock(false);

        mWavePlayer.init();
        mCurrentPlayer = &mWavePlayer;

        mWavePlayer.prepare(*waveFile);
        initPlayerParam_();

        mWavePlayer.setBankNoteInfo(key, velocity, velocityRegion);

        mSampleCount = mWavePlayer.getSampleCount();
        mSampleRate = mWavePlayer.getSampleRate();

        mPlayingSound = nullptr;
        mPlayingWaveFile = waveFile;
    }

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
    resetPlayingWaveFile();

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

s32 SoundPlayer::getPlaySamplePosition(bool isOriginalSamplePosition) const
{
    s32 currentSample = 0;

    if (mWavePlayer.isActive())
    {
        currentSample = mWavePlayer.getPlaySamplePosition(isOriginalSamplePosition);
    }

    if (mStreamPlayer.isActive())
    {
        currentSample = mStreamPlayer.getPlaySamplePosition(isOriginalSamplePosition);
    }

    return currentSample;
}

void SoundPlayer::drawParameters()
{
    HelpMarker("Those are not reset when you play a Sound !");

    if (ImGui::BeginTabBar("Params"))
    {
        if (ImGui::BeginTabItem("Basic"))
        {
            bool hasPlayer = isCurrentPlayer();

            if (ImGui::Button(ICON_LC_LIST_RESTART))
            {
                mVolume = 1.0f;
                mPitch = 1.0f;
                mPan = 0.0f;
                mLPF = 0.0f;
                mBiquadType = (s8)snd::BiquadFilterType::Inherit;
                mBiquadValue = 0.0f;
                mSeqTempoRatio = 1.0f;
                initPlayerParam_();
            }

            if (ImGui::SliderFloat("Volume", &mVolume, 0.0f, 4.0f) && hasPlayer)
            {
                snd::internal::driver::SoundThreadLock lock;
                mCurrentPlayer->setVolume(mVolume);
            }

            if (ImGui::SliderFloat("Pitch", &mPitch, 0.01f, 4.0f) && hasPlayer)
            {
                snd::internal::driver::SoundThreadLock lock;
                mCurrentPlayer->setPitch(mPitch);
            }

            if (ImGui::SliderFloat("Pan", &mPan, -2.0f, 2.0f) && hasPlayer)
            {
                snd::internal::driver::SoundThreadLock lock;
                mCurrentPlayer->setPan(mPan);
            }

            if (ImGui::SliderFloat("LPF Frequency", &mLPF, -1.0f, 0.0f) && hasPlayer)
            {
                snd::internal::driver::SoundThreadLock lock;
                mCurrentPlayer->setLpfFreq(mLPF);
            }

            static const char* sBiquadTypes[] = {
                "Inherit",
                "None",
                "Lpf",
                "Hpf",
                "Bpf512",
                "Bpf1024",
                "Bpf2048"
            };

            s32 biquadType = mBiquadType + 1;
            if (ImGui::Combo("Biquad Filter Type", (s32*)&biquadType, sBiquadTypes, IM_ARRAYSIZE(sBiquadTypes)) && hasPlayer)
            {
                snd::internal::driver::SoundThreadLock lock;
                mCurrentPlayer->setBiquadFilter(biquadType - 1, mCurrentPlayer->getBiquadFilterValue());
            }
            mBiquadType = biquadType - 1;

            if (ImGui::SliderFloat("Biquad Filter Value", &mBiquadValue, 0.0f, 1.0f) && hasPlayer)
            {
                snd::internal::driver::SoundThreadLock lock;
                mCurrentPlayer->setBiquadFilter(mCurrentPlayer->getBiquadFilterType(), mBiquadValue);
            }

            ImGui::Separator();

            bool disable = isActive() && !isCurrentPlayerSequence();
            if (disable)
            {
                ImGui::BeginDisabled();
            }

            if (ImGui::DragFloat("Sequence Tempo Ratio", &mSeqTempoRatio, 0.01f, 0.0f, 1000.0f, nullptr, ImGuiSliderFlags_AlwaysClamp))
            {
                mSequencePlayer.setTempoRatio(mSeqTempoRatio);
            }

            if (disable)
            {
                ImGui::EndDisabled();
            }

            // TODO: More ?

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Track"))
        {
            if (ImGui::Button(ICON_LC_LIST_RESTART))
            {
                for (u32 i = 0; i < cMaxTracks; i++)
                {
                    mTrackVolume[i] = 1.0f;
                }

                initPlayerTrack_();
            }

            for (u32 i = 0; i < cMaxTracks; i++)
            {
                bool isStream = mStreamPlayer.isActive();
                bool isSequence = mSequencePlayer.isActive();
                bool isWave = mWavePlayer.isActive();
                bool disable = false;
                if (isStream)
                {
                    disable = i >= mStreamPlayer.getTrackCount();
                }
                else if (isSequence)
                {
                    snd::internal::driver::SoundThreadLock lock;
                    disable = mSequencePlayer.getPlayerTrack(i) == nullptr;
                }
                else if (isWave)
                {
                    disable = true;
                }

                if (disable)
                {
                    ImGui::BeginDisabled();
                }

                if (ImGui::SliderFloat(sead::FormatFixedSafeString<32>("Track %u", i).cstr(), &mTrackVolume[i], 0.0f, 2.0f))
                {
                    if (isStream)
                    {
                        snd::internal::driver::SoundThreadLock lock;
                        mStreamPlayer.setTrackVolume(1 << i, mTrackVolume[i]);
                    }
                    else if (isSequence)
                    {
                        snd::internal::driver::SoundThreadLock lock;
                        SequenceTrack* track = mSequencePlayer.getPlayerTrack(i);
                        if (track)
                        {
                            track->setVolume(mTrackVolume[i]);
                        }
                    }
                }

                if (disable)
                {
                    ImGui::EndDisabled();
                }

                if (isSequence)
                {
                    s32 wait = 0;
                    s32 waitAmount = 0;
                    f32 progress = 1.0f;
                    sead::FixedSafeString<32> str;

                    {
                        snd::internal::driver::SoundThreadLock lock;
                        const SequenceTrack* track = mSequencePlayer.getPlayerTrack(i);
                        if (track)
                        {
                            wait = track->getParserTrackParam().wait;
                            waitAmount = track->getParserTrackParam().waitAmount;
                        }
                        else
                        {
                            progress = 0.0f;
                        }
                    }
                    
                    if (progress == 1.0f)
                    {
                        str.format("%02X/%02X", wait, waitAmount);

                        if (waitAmount != 0)
                        {
                            progress = static_cast<f32>(wait) / static_cast<f32>(waitAmount);
                        }
                    }

                    ImGui::SameLine();
                    ImGui::ProgressBar(progress, ImVec2(60.0f, 0.0f), str.cstr());
                }
            }

            ImGui::EndTabItem();
        }

        //! Effects 💀
        {
            ImGui::BeginDisabled();
            if (ImGui::BeginTabItem("Send"))
            {
                ImGui::EndTabItem();
            }
            ImGui::EndDisabled();

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Not implemented");
            }
        }

        ImGui::EndTabBar();
    }
}

void SoundPlayer::drawSeqVars()
{
    HelpMarker("Those are not reset when you play a Sound !");

    static volatile s16* sCurrentGlobalVars = mSequencePlayer.getVariablePtr(16);
    static volatile s16* sCurrentPlayerVars = mSequencePlayer.getVariablePtr(0);
    static volatile s16* sCurrentTrackVars[SequenceSoundPlayer::cTrackNumPerPlayer] = { nullptr };

    static bool sInitVars = true;
    if (sInitVars)
    {
        sInitVars = false;

        for (s32 trackNo = 0; trackNo < SequenceSoundPlayer::cTrackNumPerPlayer; trackNo++)
        {
            sCurrentTrackVars[trackNo] = mSequencePlayer.getTrack_(trackNo).getVariablePtr(0);
        }
    }

    auto varUI = [](const char* name, u32 i, SeqVarInfo& varInfo, volatile s16& current)
    {
        static const ImS16 cStepS16 = 1;

        ImGui::Text("%2u", i);

        ImGui::SameLine();

        bool updateVar = false;

        bool enable = varInfo.enable;
        if (ImGui::Checkbox(sead::FormatFixedSafeString<32>("Enable##%s%u", name, i).cstr(), &enable))
        {
            varInfo.enable = enable;
            updateVar = true;
        }

        ImGui::SameLine();

        if (!enable)
        {
            ImGui::BeginDisabled();
        }

        ImGui::SetNextItemWidth(200.0f);

        s16 var = varInfo.value;
        if (ImGui::InputScalar(sead::FormatFixedSafeString<32>("###%s%u", name, i).cstr(), ImGuiDataType_S16, &var, &cStepS16))
        {
            varInfo.value = var;
            updateVar = true;
        }

        if (!enable)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (enable && updateVar)
        {
            current = var;
        }

        ImGui::Text("Current: %i", current);
    };

    if (ImGui::BeginTabBar("Vars"))
    {
        if (ImGui::BeginTabItem("Global"))
        {
            for (u32 i = 0; i < SequenceSoundPlayer::cGlobalVariableNum; i++)
            {
                varUI("Global", i, mGlobalVars[i], sCurrentGlobalVars[i]);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Player"))
        {
            for (u32 i = 0; i < SequenceSoundPlayer::cPlayerVariableNum; i++)
            {
                varUI("Local", i, mPlayerVars[i], sCurrentPlayerVars[i]);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Track"))
        {
            if (ImGui::BeginTabBar("Tracks"))
            {
                for (u32 trackNo = 0; trackNo < SequenceSoundPlayer::cTrackNumPerPlayer; trackNo++)
                {
                    if (ImGui::BeginTabItem(sead::FormatFixedSafeString<16>("%02u", trackNo).cstr()))
                    {
                        for (u32 i = 0; i < SequenceTrack::cTrackVariableNum; i++)
                        {
                            varUI(sead::FormatFixedSafeString<32>("Track_%u", trackNo).cstr(), i, mTrackVars[trackNo][i], sCurrentTrackVars[trackNo][i]);
                        }

                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void SoundPlayer::initPlayerParam_()
{
    if (isCurrentPlayer())
    {
        mCurrentPlayer->setVolume(mVolume);
        mCurrentPlayer->setPitch(mPitch);
        mCurrentPlayer->setPan(mPan);
        mCurrentPlayer->setLpfFreq(mLPF);
        mCurrentPlayer->setBiquadFilter(mBiquadType, mBiquadValue);
    }

    if (isCurrentPlayerSequence())
    {
        mSequencePlayer.setTempoRatio(mSeqTempoRatio);
    }
}

void SoundPlayer::initPlayerTrack_()
{
    if (isCurrentPlayerStream())
    {
        for (u32 i = 0; i < cStrmTrackNum; i++)
        {
            if (i < mStreamPlayer.getTrackCount())
            {
                mStreamPlayer.setTrackVolume(1 << i, mTrackVolume[i]);
            }
        }
    }
    else if (isCurrentPlayerSequence())
    {
        for (u32 i = 0; i < SequenceSoundPlayer::cTrackNumPerPlayer; i++)
        {
            SequenceTrack* track = mSequencePlayer.getPlayerTrack(i);
            if (track)
            {
                track->setVolume(mTrackVolume[i]);
            }
        }
    }
}

void SoundPlayer::initSeqVars_()
{
    if (!isCurrentPlayerSequence())
    {
        return;
    }

    for (s32 i = 0; i < SequenceSoundPlayer::cPlayerVariableNum; i++)
    {
        const SeqVarInfo& varInfo = mPlayerVars[i];
        if (varInfo.enable)
        {
            mSequencePlayer.setLocalVariable(i, varInfo.value);
        }
    }

    for (s32 trackNo = 0; trackNo < SequenceSoundPlayer::cTrackNumPerPlayer; trackNo++)
    {
        for (s32 i = 0; i < SequenceTrack::cTrackVariableNum; i++)
        {
            const SeqVarInfo& varInfo = mTrackVars[trackNo][i];
            if (varInfo.enable)
            {
                mSequencePlayer.getTrack_(trackNo).setTrackVariable(i, varInfo.value);
            }
        }
    }
}
