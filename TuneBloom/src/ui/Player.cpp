#include <ui/UI.h>

#include <ui/PopupMgr.h>

#include <snd/ChannelMgr.h>
#include <snd/MultiVoiceMgr.h>
#include <snd/Voice.h>
#include <snd/VoiceImpl.h>
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

const Item* Player::validate(sead::BufferedSafeString& error) const
{
    if (!Item::validateName(error))
    {
        return this;
    }

    return nullptr;
}

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

static bool SeekPlayer(f32 progress);

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

struct SeqVarInfo
{
    SeqVarInfo()
        : enable(false), value(-1)
    {
    }

    bool enable;
    s16 value;
};

static SeqVarInfo sGlobalVars[SequenceSoundPlayer::cGlobalVariableNum];
static SeqVarInfo sPlayerVars[SequenceSoundPlayer::cPlayerVariableNum];
static SeqVarInfo sTrackVars[SequenceSoundPlayer::cTrackNumPerPlayer][SequenceTrack::cTrackVariableNum];

void DrawPlayerUI()
{
    if (false)
    {
        static volatile s16* sCurrentGlobalVars = sSequencePlayer.getVariablePtr(16);
        static volatile s16* sCurrentPlayerVars = sSequencePlayer.getVariablePtr(0);
        static volatile s16* sCurrentTrackVars[SequenceSoundPlayer::cTrackNumPerPlayer] = { nullptr };

        static bool sInitVars = true;
        if (sInitVars)
        {
            sInitVars = false;

            for (s32 trackNo = 0; trackNo < SequenceSoundPlayer::cTrackNumPerPlayer; trackNo++)
            {
                sCurrentTrackVars[trackNo] = sSequencePlayer.getTrack_(trackNo).getVariablePtr(0);
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

        if (ImGui::Begin("Seq Var"))
        {
            if (ImGui::BeginTabBar("Vars"))
            {
                if (ImGui::BeginTabItem("Global"))
                {
                    for (u32 i = 0; i < SequenceSoundPlayer::cGlobalVariableNum; i++)
                    {
                        varUI("Global", i, sGlobalVars[i], sCurrentGlobalVars[i]);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Player"))
                {
                    for (u32 i = 0; i < SequenceSoundPlayer::cPlayerVariableNum; i++)
                    {
                        varUI("Local", i, sPlayerVars[i], sCurrentPlayerVars[i]);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Track"))
                {
                    for (u32 trackNo = 0; trackNo < SequenceSoundPlayer::cTrackNumPerPlayer; trackNo++)
                    {
                        for (u32 i = 0; i < SequenceTrack::cTrackVariableNum; i++)
                        {
                            varUI(sead::FormatFixedSafeString<32>("Track_%u", trackNo).cstr(), i, sTrackVars[trackNo][i], sCurrentTrackVars[trackNo][i]);
                        }
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    if (false)
    {
        if (ImGui::Begin("Wave"))
        {
            f32* buf = snd::SoundSystem::getWave();
            f32* fft = snd::SoundSystem::calcFFT();

            ImGui::PlotLines("##Wave", buf, snd::SoundSystem::cSamplePerFrame, 0, "Wave", -1, 1, ImVec2(264, 80));
            ImGui::PlotHistogram("##FFT", fft, snd::SoundSystem::cSamplePerFrame / 2, 0, "FFT", 0, 10, ImVec2(264, 80), 8);
        }
        ImGui::End();
    }

    // //if (false)
    // {
    //     if (ImGui::Begin("FSEQ"))
    //     {
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
    //         }
    //     }
    //     ImGui::End();
    // }

    if (false)
    {
        if (ImGui::Begin("Voices"))
        {
            snd::internal::driver::SoundThreadLock lock;
            ImGui::Text("Channel Count: %d", snd::internal::driver::ChannelMgr::instance()->getChannelCount());
            ImGui::Text("MultiVoice Count: %d", snd::internal::driver::MultiVoiceMgr::instance()->getVoiceCount());
            ImGui::Text("MultiVoice Active Count: %d", snd::internal::driver::MultiVoiceMgr::instance()->getActiveCount());
            ImGui::Text("Voice Count: %d", snd::internal::Voice::detail_getVoiceMgr()->getActiveVoiceCount());
        }
        ImGui::End();
    }

    if (ImGui::Begin(ICON_LC_MUSIC " Player###PlayerWindow"))
    {
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

        if (sCurrentSoundPlayer && !sCurrentSoundPlayer->isActive() && sLastPlayedSound)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
            {
                ImGui::SetTooltip("Last Sound '%s'", sLastPlayedSound->getFormattedName().cstr());
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
    
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
        {
            ImGui::SetTooltip("Middle click to kill all Voices");
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

            f32 barStartX = ImGui::GetCursorPosX();
            ImVec2 barStartScreenPos = ImGui::GetCursorScreenPos();

            ImGui::ProgressBar(progress, ImVec2(adjustSize, 0.0f), "");

            static bool sCanSeek = false;

            if (ImGui::IsItemHovered() && sCurrentSoundPlayer != &sSequencePlayer && (sStreamPlayer.isActive() || sWavePlayer.isActive()))
            {
                sCanSeek = true;
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            if (sCanSeek && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Left)))
            {
                auto normalize = [](f32 min, f32 val, f32 max)
                {
                    return (val - min) / (max - min);
                };

                f32 barEndX = adjustSize + barStartX;
                f32 mouseX = ImGui::GetMousePos().x - ImGui::GetWindowPos().x;
                mouseX = sead::Mathf::clamp2(barStartX, mouseX, barEndX);

                progress = normalize(barStartX, mouseX, barEndX);

                SeekPlayer(progress);
            }
            else
            {
                sCanSeek = false;
            }

            //ImGui::ProgressBar(progress, ImVec2(adjustSize, 0.0f), "");

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
                }
            }
        }
    }
    ImGui::End();
}

static bool PlaySeqSound(const Sound* sound)
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

    if (!PlaySeqFile(seqFile, seqSoundInfo.getStartLabel(), banks, sound->getVolume()))
    {
        return false;
    }

    sSelectedItem = const_cast<Sound*>(sound);

    return true;
}

static bool PlayStrmSound(const Sound* sound)
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

    return true;
}

static bool PlayWaveSound(const Sound* sound, u32 startOffsetSample)
{
    const Item* waveFile = sound->getWaveSoundInfo().getWaveFileRef().getItem();
    if (!waveFile)
    {
        PopupMgr::instance()->addPopup({ "No Wave File attached", nullptr });
        //SEAD_PRINT("No wave file attached\n");
        return false;
    }

    PlayWaveFile(*static_cast<const WaveFile*>(waveFile), -1, sound, startOffsetSample);

    sSelectedItem = const_cast<Sound*>(sound);

    return true;
}

void PlaySound(const Sound* sound, u32 startOffsetSample)
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
            PlayWaveSound(sound, startOffsetSample);
            break;
    }
}

bool PlayWaveFile(const WaveFile& wave, s32 channel, const Sound* sound, u32 startOffsetSample)
{
    SEAD_ASSERT(wave.getItemType() == Item::ItemType::WaveFile);

    if (wave.getChannels().isEmpty())
    {
        PopupMgr::instance()->addPopup({ "Wave File has no channels", nullptr });
        return false;
    }

    {
        snd::internal::driver::SoundThreadLock lock;

        StopAllSoundPlayersWithoutLock();

        sWavePlayer.init();

        if (sound)
        {
            sWavePlayer.setInitialVolume(static_cast<f32>(sound->getVolume()) / 127.0f);
        }

        sWavePlayer.setVolume(sVolume);

        sWavePlayer.prepare(wave, channel, sound, startOffsetSample);

        sCurrentSoundPlayer = &sWavePlayer;

        sSampleCount = sWavePlayer.getSampleCount();
        sSampleRate = sWavePlayer.getSampleRate();
    }

    sSelectedItem = const_cast<WaveFile*>(&wave);

    return true;
}

bool PlayBankNote(u8 key, u8 velocity, const BankFile::VelocityRegion& velocityRegion)
{
    const Item* waveFile = velocityRegion.getWaveFileRef().getItem();
    if (!waveFile)
    {
        return false;
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

    return true;
}

bool PlaySeqFile(const SequenceFile& seqFile, const sead::SafeString& startLabel, const Bank** bankArray, u8 volume)
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

        StopAllSoundPlayersWithoutLock();

        sSequencePlayer.init();

        sSequencePlayer.setInitialVolume(static_cast<f32>(volume) / 127.0f);
        sSequencePlayer.setVolume(sVolume);

        sSequencePlayer.setup(allocTracks, &sSequenceNoteOnCallback2);
        sSequencePlayer.prepare(seqFile, startOffset, banks);

        sCurrentSoundPlayer = &sSequencePlayer;

        sSampleCount = 0;
        sSampleRate = 0;

        //? Init vars
        for (s32 i = 0; i < SequenceSoundPlayer::cPlayerVariableNum; i++)
        {
            const SeqVarInfo& varInfo = sPlayerVars[i];
            if (varInfo.enable)
            {
                sSequencePlayer.setLocalVariable(i, varInfo.value);
            }
        }

        for (s32 trackNo = 0; trackNo < SequenceSoundPlayer::cTrackNumPerPlayer; trackNo++)
        {
            for (s32 i = 0; i < SequenceTrack::cTrackVariableNum; i++)
            {
                const SeqVarInfo& varInfo = sTrackVars[trackNo][i];
                if (varInfo.enable)
                {
                    sSequencePlayer.getTrack_(trackNo).setTrackVariable(i, varInfo.value);
                }
            }
        }
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

static bool SeekPlayer(f32 progress)
{
    progress = sead::Mathf::clamp2(0.0f, progress, 1.0f);

    Item* selectedItem = sSelectedItem;

    if (sWavePlayer.isActive())
    {
        snd::internal::driver::SoundThreadLock lock;
        sWavePlayer.seek(sSampleCount * progress);
    }

    if (sStreamPlayer.isActive())
    {
        snd::internal::driver::SoundThreadLock lock;
        sStreamPlayer.seek(sSampleCount * progress);
    }

    sSelectedItem = selectedItem;

    return true;
}
