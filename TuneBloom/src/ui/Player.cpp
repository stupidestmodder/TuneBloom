#include <ui/UI.h>

#include <snd/ChannelMgr.h>
#include <snd/MultiVoiceMgr.h>
#include <snd/SoundSystem.h>

#include <snd/SequenceNoteOnCallback.h>
#include <snd/SequenceSoundPlayer.h>
#include <snd/StreamSoundPlayer.h>
#include <snd/WaveSoundPlayer.h>

#include <snd/snd_SequenceSoundFileReader.h>

#include <basis/seadWarning.h>
#include <filedevice/seadFileDeviceMgr.h>
#include <filedevice/seadPath.h>
#include <heap/seadExpHeap.h>

// Players

InstanciateItemCallback CreatePlayerFunc(bool clear)
{
    return CreateItemFunc(clear, []() -> Item* { return new Player(); }, nullptr);
}

void DrawPlayersUI()
{
    DrawAllItemsUI("Player", sBfsar.getPlayerList(), &CreatePlayerFunc);
}

void DrawPlayerPropertiesUI()
{
    Player* player = static_cast<Player*>(sSelectedItem);

    const ImU32 cStepU32 = 1;

    {
        u32 playableSoundMax = player->getPlayableSoundMax();
        if (ImGui::InputScalar("Playable Sound Max", ImGuiDataType_U32, &playableSoundMax, &cStepU32))
        {
            player->setPlayableSoundMax(playableSoundMax);
        }
    }

    bool enablePlayerHeapSize = player->isEnablePlayerHeapSize();
    if (ImGui::Checkbox("Enable Player Heap Size", &enablePlayerHeapSize))
    {
        player->setEnablePlayerHeapSize(enablePlayerHeapSize);
    }

    if (!enablePlayerHeapSize)
        ImGui::BeginDisabled();

    {
        u32 playerHeapSize = player->getPlayerHeapSize();
        if (ImGui::InputScalar("Player Heap Size", ImGuiDataType_U32, &playerHeapSize, &cStepU32))
        {
            player->setPlayerHeapSize(playerHeapSize);
        }
    }

    if (!enablePlayerHeapSize)
        ImGui::EndDisabled();
}

// Runtime Player

SequenceSoundPlayer sSequencePlayer;
StreamSoundPlayer sStreamPlayer;
WaveSoundPlayer sWavePlayer;

SequenceNoteOnCallback sSequenceNoteOnCallback;
SequenceNoteOnCallback2 sSequenceNoteOnCallback2;

BasicSoundPlayer* sCurrentSoundPlayer = nullptr;
bool sLoop = true;
static f32 sVolume = 0.5f;

const Sound* sLastPlayedSound = nullptr;

u32 sSampleRate = 0;
u32 sSampleCount = 0;

static s16 sGlobalVars[SequenceSoundPlayer::cGlobalVariableNum] = { -1 };
static s16 sPlayerVars[SequenceSoundPlayer::cPlayerVariableNum] = { -1 };
static s16 sTrackVars[SequenceSoundPlayer::cTrackNumPerPlayer][SequenceTrack::cTrackVariableNum] = { -1 };

void DrawPlayerUI()
{
    //return;

    if (false)
    {
        if (ImGui::Begin("Seq Var"))
        {
            for (u32 i = 0; i < SequenceSoundPlayer::cGlobalVariableNum; i++)
            {
                ImGui::DragScalar(sead::FormatFixedSafeString<32>("Global_%u", i).cstr(), ImGuiDataType_S16, &sGlobalVars[i]);
            }

            for (u32 i = 0; i < SequenceSoundPlayer::cPlayerVariableNum; i++)
            {
                ImGui::DragScalar(sead::FormatFixedSafeString<32>("Local_%u", i).cstr(), ImGuiDataType_S16, &sPlayerVars[i]);
            }

            for (u32 trackNo = 0; trackNo < SequenceSoundPlayer::cTrackNumPerPlayer; trackNo++)
            {
                for (u32 i = 0; i < SequenceTrack::cTrackVariableNum; i++)
                {
                    ImGui::DragScalar(sead::FormatFixedSafeString<32>("Track%u_%u", trackNo, i).cstr(), ImGuiDataType_S16, &sTrackVars[trackNo][i]);
                }
            }
        }
        ImGui::End();
    }

    // //if (false)
    // {
    //     if (ImGui::Begin("FSEQ"))
    //     {
    //         static bool sFollowSeq = true;
    //         static u32 sFollowTrack = 0;

    //         //if (sSequencePlayer.isActive())
    //         {
    //             // s32 wait = 0;
    //             // s32 waitAmount = 1;

    //             // const SequenceTrack* track = sSequencePlayer.getPlayerTrack(0);
    //             // track = &sSequencePlayer.getTrack_(0);
    //             // if (track)
    //             // {
    //             //     sTrack0Offset = intptr_t(track->getParserTrackParam().currentCmdAddr) - intptr_t(track->getParserTrackParam().baseAddr);
    //             //     wait = track->getParserTrackParam().wait;
    //             //     waitAmount = track->getParserTrackParam().waitAmount;
    //             // }

    //             //if (sOffsetToLine)
    //             //    line = (*sOffsetToLine)[sTrack0Offset];

    //             //f32 progress = static_cast<f32>(wait) / static_cast<f32>(waitAmount);

    //             //sead::FormatFixedSafeString<32> str("%04X/%04X", wait, waitAmount);

    //             //ImGui::Text("Wait:   %d", wait);
    //             //ImGui::SameLine();
    //             //ImGui::ProgressBar(progress, ImVec2(120.0f, 0.0f), str.cstr());
    //             //ImGui::Text("Offset: %d", sTrack0Offset);
    //             //ImGui::Text("Line:   %d", line);
    //             //ImGui::SameLine();

    //             ImGui::Checkbox("Follow", &sFollowSeq);

    //             ImGui::SameLine();

    //             static const char* sTrackNames[] = {
    //                 "Track 0",
    //                 "Track 1",
    //                 "Track 2",
    //                 "Track 3",
    //                 "Track 4",
    //                 "Track 5",
    //                 "Track 6",
    //                 "Track 7",
    //                 "Track 8",
    //                 "Track 9",
    //                 "Track 10",
    //                 "Track 11",
    //                 "Track 12",
    //                 "Track 13",
    //                 "Track 14",
    //                 "Track 15"
    //             };

    //             ImGui::Combo("Track", (s32*)&sFollowTrack, sTrackNames, IM_ARRAYSIZE(sTrackNames));
    //         }

    //         if (sTextEditor)
    //         {
    //             sSeqTextKeeper.update(sSequencePlayer, *sTextEditor);

    //             sTextEditor->SetReadOnly(true);
    //             sTextEditor->Render("Editor");

    //             if (sFollowSeq)
    //             {
    //                 const SeqTextKeeper::Track& track = sSeqTextKeeper.mTracks[sFollowTrack];
    //                 if (track.active && track.line > 0)
    //                 {
    //                     sTextEditor->SetCursorPosition({ track.line - 1, 0 });
    //                 }
    //             }
    //         }
    //     }
    //     ImGui::End();
    // }

    if (ImGui::Begin(ICON_LC_MUSIC " Player###PlayerWindow"))
    {
        if (false)
        {
            snd::internal::driver::SoundThreadLock lock;
            ImGui::Text("Channel Count: %d", snd::internal::driver::ChannelMgr::instance()->getChannelCount());
        }

        bool isPause = true;
        if (sCurrentSoundPlayer && sCurrentSoundPlayer->isActive())
        {
            isPause = sCurrentSoundPlayer->isPause();
        }

        if (ImGui::Button(isPause ? ICON_LC_PLAY : ICON_LC_PAUSE) && sCurrentSoundPlayer)
        {
            if (!sCurrentSoundPlayer->isActive())
            {
                if (sLastPlayedSound)
                {
                    PlaySound(sLastPlayedSound);
                }
            }
            else
            {
                snd::internal::driver::SoundThreadLock lock;
                sCurrentSoundPlayer->pause(!isPause);
            }
        }

        ImGui::SameLine();

        if (ImGui::ButtonEx(ICON_LC_SQUARE, ImVec2(0.0f, 0.0f), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle))
        {
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
            {
                StopAllSoundPlayers(true);
                StopAllVoices();
            }
            else
            {
                StopAllSoundPlayers(false);
            }
        }

        ImGui::SameLine();

        f32 progress = 0.0f;
        f32 playingSample = 0.0f;
        f32 seconds = 0.0f;

        u32 sampleCount = sSampleCount;
        u32 sampleRate = sSampleRate;
        s32 currentSample = 0;

        if (sStreamPlayer.isActive())
        {
            currentSample = sStreamPlayer.getPlaySamplePosition(true);
        }

        if (sWavePlayer.isActive())
        {
            currentSample = sWavePlayer.getPlaySamplePosition(true);
        }

        playingSample = static_cast<f32>(currentSample);
        progress = sampleCount != 0 ? playingSample / static_cast<f32>(sampleCount) : 0.0f;

        seconds = sampleRate != 0 ? playingSample / static_cast<f32>(sampleRate) : 0.0f;
        u32 minutes = static_cast<u32>(seconds) / 60;

        ImGui::Text("%02d:%02d.%03d", minutes, static_cast<u32>(seconds) % 60, static_cast<u32>(fmodf(seconds * 1000.0f, 1000.0f)));
        ImGui::SameLine();

        //f32 adjustSize = ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("00:00.000").x - 257.0f;
        f32 adjustSize = ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("00:00.000").x - 257.0f + 57.0f;
        if (adjustSize > 0.0f)
        {
            if (sCurrentSoundPlayer == &sSequencePlayer)
            {
                static f32 col = 0.0f;
                static f32 step = 0.003f;

                if (!sCurrentSoundPlayer->isPause())
                {
                    col += step;
                    if (col > 0.1f)
                        step = -step;
                    if (col < 0.0f)
                        step = -step;
                }

                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.5f - col, 0.5f - col, 0.5f - col, 1.0f));
                if (sSequencePlayer.isActive())
                    progress = 1.0f;
            }

            ImGui::ProgressBar(progress, ImVec2(adjustSize, 0.0f), "");

            if (sCurrentSoundPlayer == &sSequencePlayer)
                ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        seconds = sampleRate != 0 ? static_cast<f32>(sampleCount) / static_cast<f32>(sampleRate) : 0.0f;
        minutes = static_cast<u32>(seconds) / 60;

        ImGui::Text("%02d:%02d.%03d", minutes, static_cast<u32>(seconds) % 60, static_cast<u32>(fmodf(seconds * 1000.0f, 1000.0f)));
        //ImGui::SameLine();
        //ImGui::Checkbox(ICON_LC_REPEAT, &sLoop);
        ImGui::SameLine();

        {
            const char* icon = ICON_LC_VOLUME_2;
            if (sVolume <= 0.5f)
                icon = ICON_LC_VOLUME_1;
            if (sVolume <= 0.0f)
                icon = ICON_LC_VOLUME_X;

            if (ImGui::BeginPopup("volume"))
            {
                ImGui::Text(icon);

                sead::FormatFixedSafeString<8> str("%d", static_cast<s32>(sVolume * 100.0f));

                ImGui::SameLine();
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::SliderFloat("##", &sVolume, 0.0f, 1.0f, str.cstr(), ImGuiSliderFlags_NoInput))
                {
                    if (sCurrentSoundPlayer)
                    {
                        snd::internal::driver::SoundThreadLock lock;
                        sCurrentSoundPlayer->setVolume(sVolume);
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
            if (ImGui::Button(icon))
            {
                ImGui::OpenPopup("volume");
            }
            ImGui::PopStyleColor();
        }

        ImGui::SeparatorText("Temp");

        if (false)
        {
            f32* buf = snd::SoundSystem::getWave();
            f32* fft = snd::SoundSystem::calcFFT();

            ImGui::PlotLines("##Wave", buf, snd::SoundSystem::cSamplePerFrame, 0, "Wave", -1, 1, ImVec2(264, 80));
            ImGui::PlotHistogram("##FFT", fft, snd::SoundSystem::cSamplePerFrame / 2, 0, "FFT", 0, 10, ImVec2(264, 80), 8);
        }

        //if (false)
        {
            snd::internal::driver::SoundThreadLock lock;

            if (sCurrentSoundPlayer)
            {
                f32 pitch = sCurrentSoundPlayer->getPitch();
                if (ImGui::DragFloat("Pitch", &pitch, 0.01f, 0.01f, 4.0f))
                {
                    sCurrentSoundPlayer->setPitch(pitch);
                }

                f32 lpf = sCurrentSoundPlayer->getLpfFreq();
                if (ImGui::DragFloat("LPF", &lpf, 0.01f, -1.0f, 0.0f))
                {
                    sCurrentSoundPlayer->setLpfFreq(lpf);
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

                s32 biquadType = sCurrentSoundPlayer->getBiquadFilterType() + 1;
                if (ImGui::Combo("Biquad Type", (s32*)&biquadType, sBiquadTypes, IM_ARRAYSIZE(sBiquadTypes)))
                {
                    sCurrentSoundPlayer->setBiquadFilter(biquadType - 1, sCurrentSoundPlayer->getBiquadFilterValue());
                }

                f32 biquadValue = sCurrentSoundPlayer->getBiquadFilterValue();
                if (ImGui::DragFloat("Biquad Value", &biquadValue, 0.01f, 0.0f, 1.0f))
                {
                    sCurrentSoundPlayer->setBiquadFilter(sCurrentSoundPlayer->getBiquadFilterType(), biquadValue);
                }
            }

            if (sStreamPlayer.isActive())
            {
                for (u32 i = 0; i < sStreamPlayer.getTrackCount(); i++)
                {
                    f32 volume = sStreamPlayer.getPlayerTrack(i)->mVolume;
                    if (ImGui::DragFloat(sead::FormatFixedSafeString<32>("Track%d Volume", i).cstr(), &volume, 0.01f, 0.0f, 2.0f))
                    {
                        sStreamPlayer.setTrackVolume(1 << i, volume);
                    }
                }
            }

            if (sSequencePlayer.isActive())
            {
                f32 tempoRatio = sSequencePlayer.getTempoRatio();
                if (ImGui::DragFloat("TempoRatio", &tempoRatio, 0.01f, 0.0f, sead::Mathf::maxNumber()))
                {
                    sSequencePlayer.setTempoRatio(tempoRatio);
                }

                for (u32 i = 0; i < SequenceSoundPlayer::cTrackNumPerPlayer; i++)
                {
                    SequenceTrack* track = sSequencePlayer.getPlayerTrack(i);
                    if (!track)
                        continue;

                    f32 volume = track->getVolume();
                    if (ImGui::DragFloat(sead::FormatFixedSafeString<32>("Track%d Volume", i).cstr(), &volume, 0.01f, 0.0f, 2.0f))
                    {
                        track->setVolume(volume);
                    }

                    if (false)
                    {
                        for (u32 varNo = 0; varNo < SequenceTrack::cTrackVariableNum; varNo++)
                        {
                            track->setTrackVariable(varNo, sTrackVars[i][varNo]);
                        }
                    }
                }

                if (false)
                {
                    for (u32 i = 0; i < SequenceSoundPlayer::cGlobalVariableNum; i++)
                    {
                        sSequencePlayer.setGlobalVariable(i, sGlobalVars[i]);
                    }

                    for (u32 i = 0; i < SequenceSoundPlayer::cPlayerVariableNum; i++)
                    {
                        sSequencePlayer.setLocalVariable(i, sPlayerVars[i]);
                    }
                }
            }
        }
    }
    ImGui::End();
}

void PlaySeqSound(const Sound* sound)
{
    const Sound::SequenceSoundInfo& seqSoundInfo = sound->getSequenceSoundInfo();

    const Item* seqFileItem = seqSoundInfo.getSequenceFileRef().getItem();
    if (!seqFileItem)
    {
        SEAD_PRINT("No sequence file attached\n");
        return;
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

    if (!PlaySeqFile(seqFile, seqSoundInfo.getStartLabel(), banks, sound->getVolume()))
    {
        return;
    }

    sSelectedItem = const_cast<Sound*>(sound);
}

void PlayStrmSound(const Sound* sound)
{
    if (sound->getStreamSoundInfo().getStreamType() != Sound::StreamSoundInfo::StreamType::NwStreamBinary)
    {
        SEAD_PRINT("Only BFSTM sounds are supported atm\n");
        return;
    }

    const Sound::StreamSoundInfo& strmSoundInfo = sound->getStreamSoundInfo();

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

    u32 channelIdxStart = 0;
    for (u32 i = 0; i < strmSoundInfo.getTrackList().size(); i++)
    {
        nw::snd::SoundArchive::StreamTrackInfo& tmp = setupArg.tracks[i];

        const Item* trackItem = strmSoundInfo.getTrackList().nth(i)->val();
        const Sound::StreamSoundInfo::Track& trackInfo = *static_cast<const Sound::StreamSoundInfo::Track*>(trackItem);

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

        StopAllSoundPlayersWithoutLock();

        sStreamPlayer.init();

        sStreamPlayer.setInitialVolume(static_cast<f32>(sound->getVolume()) / 127.0f);
        sStreamPlayer.setVolume(sVolume);

        sStreamPlayer.setup(setupArg);
        sStreamPlayer.prepare(strmSoundInfo);

        sCurrentSoundPlayer = &sStreamPlayer;

        sSampleCount = sStreamPlayer.getSampleCount();
        sSampleRate = sStreamPlayer.getSampleRate();
    }

    sSelectedItem = const_cast<Sound*>(sound);
}

void PlayWaveSound(const Sound* sound)
{
    const Item* waveFile = sound->getWaveSoundInfo().getWaveFileRef().getItem();
    if (!waveFile)
    {
        SEAD_PRINT("No wave file attached\n");
        return;
    }

    PlayWaveFile(*static_cast<const WaveFile*>(waveFile), -1, sound);

    sSelectedItem = const_cast<Sound*>(sound);
}

void PlaySound(const Sound* sound)
{
    SEAD_ASSERT(sound);

    sLastPlayedSound = sound;

    switch (sound->getSoundType())
    {
        case Sound::SoundType::Seq:
            PlaySeqSound(sound);
            break;

        case Sound::SoundType::Strm:
            PlayStrmSound(sound);
            break;

        case Sound::SoundType::Wave:
            PlayWaveSound(sound);
            break;
    }
}

void PlayWaveFile(const WaveFile& wave, s32 channel, const Sound* sound)
{
    SEAD_ASSERT(wave.getItemType() == Item::ItemType::WaveFile);

    {
        snd::internal::driver::SoundThreadLock lock;

        StopAllSoundPlayersWithoutLock();

        sWavePlayer.init();

        if (sound)
        {
            sWavePlayer.setInitialVolume(static_cast<f32>(sound->getVolume()) / 127.0f);
        }

        sWavePlayer.setVolume(sVolume);

        sWavePlayer.prepare(wave, channel, sound);

        sCurrentSoundPlayer = &sWavePlayer;

        sSampleCount = sWavePlayer.getSampleCount();
        sSampleRate = sWavePlayer.getSampleRate();
    }

    sSelectedItem = const_cast<WaveFile*>(&wave);
}

void PlayBankNote(u8 key, u8 velocity, const BankFile::VelocityRegion& velocityRegion)
{
    const Item* waveFile = velocityRegion.getWaveFileRef().getItem();
    if (!waveFile)
    {
        return;
    }

    snd::internal::driver::SoundThreadLock lock;

    StopAllSoundPlayersWithoutLock();

    sWavePlayer.init();

    sWavePlayer.setVolume(sVolume);

    sWavePlayer.prepare(*static_cast<const WaveFile*>(waveFile));
    sWavePlayer.setBankNoteInfo(key, velocity, velocityRegion);

    sCurrentSoundPlayer = &sWavePlayer;

    sSampleCount = sWavePlayer.getSampleCount();
    sSampleRate = sWavePlayer.getSampleRate();
}

bool PlaySeqFile(const SequenceFile& seqFile, const sead::SafeString& startLabel, const Bank** bankArray, u8 volume)
{
    if (!seqFile.isValid())
    {
        SEAD_PRINT("SequenceFile is not valid\n");
        return false;
    }

    u32 startOffset = seqFile.getLabelOffset(startLabel);
    if (startOffset == SequenceFile::cInvaldOffset)
    {
        SEAD_PRINT("Couldn't find label '%s'\n", startLabel.cstr());
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

        StopAllSoundPlayersWithoutLock();

        sSequencePlayer.init();

        sSequencePlayer.setInitialVolume(static_cast<f32>(volume) / 127.0f);
        sSequencePlayer.setVolume(sVolume);

        sSequencePlayer.setup(allocTracks, &sSequenceNoteOnCallback2);
        sSequencePlayer.prepare(seqFile, startOffset, banks);

        sCurrentSoundPlayer = &sSequencePlayer;

        sSampleCount = 0;
        sSampleRate = 0;
    }

    return true;
}

void StopAllSoundPlayers(bool stop)
{
    snd::internal::driver::SoundThreadLock lock;

    StopAllSoundPlayersWithoutLock(stop);
}

void StopAllSoundPlayersWithoutLock(bool stop)
{
    sSequencePlayer.deinit(stop);
    sStreamPlayer.deinit();
    sWavePlayer.deinit(stop);
}

void StopAllVoices()
{
    snd::internal::driver::SoundThreadLock lock;

    snd::internal::driver::MultiVoiceMgr::instance()->stopAllVoices();
}
