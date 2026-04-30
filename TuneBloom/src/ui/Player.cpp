#include <ui/UI.h>

#include <ui/PopupMgr.h>

#include <bfsar/SoundPlayer.h>

#include <snd/ChannelMgr.h>
#include <snd/MultiVoiceMgr.h>
#include <snd/Voice.h>
#include <snd/VoiceImpl.h>
#include <snd/SoundSystem.h>

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
    DrawAllItemsUI("Player", sBfsar.getPlayerList(),
        &CreatePlayerFunc, nullptr, nullptr, GetItemFilterCallback()
    );
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

SoundPlayer sSoundPlayer;

void DrawPlayerUI()
{
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
        bool isPause = sSoundPlayer.isPause();

        if (ImGui::Button(isPause ? ICON_LC_PLAY : ICON_LC_PAUSE) && sSoundPlayer.isCurrentPlayer())
        {
            if (!sSoundPlayer.isActive())
            {
                sSoundPlayer.playLastSound();
            }
            else
            {
                sSoundPlayer.pause(!isPause);
            }
        }

        if (sSoundPlayer.isCurrentPlayer() && !sSoundPlayer.isActive() && sSoundPlayer.getLastPlayedSound())
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
            {
                ImGui::SetTooltip("Last Sound '%s'", sSoundPlayer.getLastPlayedSound()->getFormattedName().cstr());
            }
        }

        ImGui::SameLine();

        if (ImGui::ButtonEx(ICON_LC_SQUARE, ImVec2(0.0f, 0.0f), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle))
        {
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
            {
                sSoundPlayer.stopAllPlayers(true);
                sSoundPlayer.stopAllVoices();
            }
            else
            {
                sSoundPlayer.stopAllPlayers(false);
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

        u32 sampleCount = sSoundPlayer.getSampleCount();
        u32 sampleRate = sSoundPlayer.getSampleRate();
        s32 currentSample = sSoundPlayer.getPlaySamplePosition();

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
            if (sSoundPlayer.isCurrentPlayerSequence())
            {
                static f32 col = 0.0f;
                static f32 step = 0.003f;

                if (!sSoundPlayer.isPause())
                {
                    col += step;
                    if (col > 0.1f)
                        step = -step;
                    if (col < 0.0f)
                        step = -step;
                }

                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.5f - col, 0.5f - col, 0.5f - col, 1.0f));
                if (sSoundPlayer.isActive())
                    progress = 1.0f;
            }

            f32 barStartX = ImGui::GetCursorPosX();
            ImVec2 barStartScreenPos = ImGui::GetCursorScreenPos();
            barStartScreenPos.y -= 1; // fluh

            ImGui::ProgressBar(progress, ImVec2(adjustSize, 0.0f), "");

            f32 barYSize = ImGui::GetItemRectSize().y;

            static bool sCanSeek = false;

            if (ImGui::IsItemHovered() && !sSoundPlayer.isCurrentPlayerSequence() && sSoundPlayer.isActive())
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

                sSoundPlayer.seek(progress);
            }
            else
            {
                sCanSeek = false;
            }

            //ImGui::ProgressBar(progress, ImVec2(adjustSize, 0.0f), "");

            if (sSoundPlayer.isCurrentPlayerSequence())
                ImGui::PopStyleColor();
            
            //? Draw loop points
            if (!sSoundPlayer.isCurrentPlayerSequence() || !sSoundPlayer.isActive())
            {
                const WaveFile* wave = nullptr;
                if (sSoundPlayer.isActive() && sSoundPlayer.getPlayingWaveFile())
                {
                    wave = sSoundPlayer.getPlayingWaveFile();
                }
                else if (sSelectedItem)
                {
                    if (sSelectedItem->getItemType() == Item::ItemType::WaveFile)
                    {
                        wave = static_cast<WaveFile*>(sSelectedItem);
                    }
                    else if (sSelectedItem->getItemType() == Item::ItemType::Sound)
                    {
                        Sound* sound = static_cast<Sound*>(sSelectedItem);
                        if (sound->getSoundType() == Sound::SoundType::Wave)
                        {
                            wave = static_cast<WaveFile*>(sound->getWaveSoundInfo().getWaveFileRef().getItem());
                        }
                        else if (sound->getSoundType() == Sound::SoundType::Strm && !sound->getStreamSoundInfo().getTrackList().isEmpty())
                        {
                            Sound::StreamSoundInfo::Track* track = static_cast<Sound::StreamSoundInfo::Track*>(sound->getStreamSoundInfo().getTrackList().front()->val());
                            wave = static_cast<WaveFile*>(track->getWaveFileRef().getItem());
                        }
                    }
                }

                if (wave)
                {
                    sampleCount = wave->getSampleCount();

                    ImDrawList* draw = ImGui::GetWindowDrawList();
                    f32 loopStartX = wave->getOriginalLoopStartFrame() / static_cast<f32>(sampleCount);
                    f32 loopEndX = wave->getOriginalLoopEndFrame() / static_cast<f32>(sampleCount);

                    if (wave->getIsLoop())
                    {
                        draw->AddLine(
                            ImVec2(barStartScreenPos.x + loopStartX * adjustSize, barStartScreenPos.y),
                            ImVec2(barStartScreenPos.x + loopStartX * adjustSize, barStartScreenPos.y + barYSize), IM_COL32(0, 255, 0, 255), 2.0f
                        );
                    }

                    draw->AddLine(
                        ImVec2(barStartScreenPos.x + loopEndX * adjustSize, barStartScreenPos.y),
                        ImVec2(barStartScreenPos.x + loopEndX * adjustSize, barStartScreenPos.y + barYSize), IM_COL32(255, 0, 0, 255), 2.0f
                    );
                }
            }
        }

        ImGui::SameLine();

        seconds = sampleRate != 0 ? static_cast<f32>(sampleCount) / static_cast<f32>(sampleRate) : 0.0f;
        minutes = static_cast<u32>(seconds) / 60;

        ImGui::Text("%02d:%02d.%03d", minutes, static_cast<u32>(seconds) % 60, static_cast<u32>(fmodf(seconds * 1000.0f, 1000.0f)));
        ImGui::SameLine();

        {
            f32 volume = sSoundPlayer.getVolume();

            const char* icon = ICON_LC_VOLUME_2;
            if (volume <= 0.5f)
                icon = ICON_LC_VOLUME_1;
            if (volume <= 0.0f)
                icon = ICON_LC_VOLUME_X;

            if (ImGui::BeginPopup("volume"))
            {
                ImGui::Text(icon);

                sead::FormatFixedSafeString<8> str("%d", static_cast<s32>(volume * 100.0f));

                ImGui::SameLine();
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::SliderFloat("##", &volume, 0.0f, 1.0f, str.cstr(), ImGuiSliderFlags_NoInput))
                {
                    sSoundPlayer.setVolume(volume);
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
    }
    ImGui::End();

    if (ImGui::Begin(ICON_LC_SETTINGS_2 " Player Parameters###PlayerParamWindow"))
    {
        sSoundPlayer.drawParameters();
    }
    ImGui::End();

    if (ImGui::Begin(ICON_LC_BINARY " Sequence Variables###SequenceVarWindow"))
    {
        sSoundPlayer.drawSeqVars();
    }
    ImGui::End();
}
