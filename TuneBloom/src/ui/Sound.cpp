#include <ui/UI.h>

extern void ParseSequenceFile(u32 fileId);

void DrawSoundPropertiesUI()
{
    Sound* sound = static_cast<Sound*>(sSelectedItem);

    const ImU8 cStepU8 = 1;
    const ImU16 cStepU16 = 1;
    const ImU32 cStepU32 = 1;

    {
        Item* player = sound->getPlayerRef().getItem();
        if (ItemSelector("Player", sBfsar.getPlayerList(), &player))
        {
            sound->getPlayerRef().attach(player);
        }
    }

    {
        u8 volume = sound->getVolume();
        if (ImGui::InputScalar(sead::FormatFixedSafeString<32>("Volume (%.3f)###vol", static_cast<f32>(volume) / 127.0f).cstr(), ImGuiDataType_U8, &volume, &cStepU8))
        {
            sound->setVolume(volume);
        }
    }

    {
        u8 remoteFilter = sound->getRemoteFilter();
        if (ImGui::InputScalar("Remote Filter", ImGuiDataType_U8, &remoteFilter, &cStepU8))
        {
            sound->setRemoteFilter(remoteFilter);
        }
    }

    {
        bool enablePanParam = sound->isEnablePanParam();
        if (ImGui::Checkbox("Enable Pan Param", &enablePanParam))
        {
            sound->setEnablePanParam(enablePanParam);
        }

        if (!enablePanParam)
            ImGui::BeginDisabled();

        {
            static const char* sPanModes[] = {
                "Dual",
                "Balance",
                //"Invalid"
            };

            snd::PanMode panMode = sound->getPanMode();
            if (ImGui::Combo("Pan Mode", (s32*)&panMode, sPanModes, IM_ARRAYSIZE(sPanModes)))
            {
                sound->setPanMode(panMode);
            }
        }

        {
            static const char* sPanCurves[] = { 
                "Sqrt",
                "Sqrt0Db",
                "Sqrt0DbClamp",
                "SinCos",
                "SinCos0Db",
                "SinCos0DbClamp",
                "Linear",
                "Linear0Db",
                "Linear0DbClamp",
                //"Invalid"
            };

            snd::PanCurve panCurve = sound->getPanCurve();
            if (ImGui::Combo("Pan Curve", (s32*)&panCurve, sPanCurves, IM_ARRAYSIZE(sPanCurves)))
            {
                sound->setPanCurve(panCurve);
            }
        }

        if (!enablePanParam)
            ImGui::EndDisabled();
    }

    {
        bool enablePlayerParam = sound->isEnablePlayerParam();
        if (ImGui::Checkbox("Enable Player Param", &enablePlayerParam))
        {
            sound->setEnablePlayerParam(enablePlayerParam);
        }

        if (!enablePlayerParam)
            ImGui::BeginDisabled();

        {
            u8 playerPriority = sound->getPlayerPriority();
            if (ImGui::InputScalar("Player Priority", ImGuiDataType_U8, &playerPriority, &cStepU8))
            {
                sound->setPlayerPriority(playerPriority);
            }
        }

        {
            u8 actorPlayerId = sound->getActorPlayerId();
            if (ImGui::InputScalar("Actor Player Id", ImGuiDataType_U8, &actorPlayerId, &cStepU8))
            {
                sound->setActorPlayerId(actorPlayerId);
            }
        }

        if (!enablePlayerParam)
            ImGui::EndDisabled();
    }

    {
        if (ImGui::CollapsingHeader("User Param"))
        {
            for (u32 i = 0; i < 4; i++)
            {
                bool enableUserParam = sound->isEnableUserParam(i);
                if (ImGui::Checkbox(sead::FormatFixedSafeString<64>("Enable User Param %u", i).cstr(), &enableUserParam))
                {
                    sound->setEnableUserParam(i, enableUserParam);
                }

                if (!enableUserParam)
                    ImGui::BeginDisabled();

                {
                    u32 userParam = sound->getUserParam(i);
                    if (ImGui::InputScalar(sead::FormatFixedSafeString<64>("User Param %u", i).cstr(), ImGuiDataType_U32, &userParam, &cStepU32))
                    {
                        sound->setUserParam(i, userParam);
                    }
                }

                if (!enableUserParam)
                    ImGui::EndDisabled();
            }

            ImGui::Separator();
        }
    }

    {
        bool enableIsFrontBypass = sound->isEnableIsFrontBypass();
        if (ImGui::Checkbox("Enable Front Bypass", &enableIsFrontBypass))
        {
            sound->setEnableIsFrontBypass(enableIsFrontBypass);
        }

        if (!enableIsFrontBypass)
            ImGui::BeginDisabled();

        {
            bool isFrontBypass = sound->getIsFrontBypass();
            if (ImGui::Checkbox("Front Bypass", &isFrontBypass))
            {
                sound->setIsFrontBypass(isFrontBypass);
            }
        }

        if (!enableIsFrontBypass)
            ImGui::EndDisabled();
    }

    {
        bool enableSound3DInfo = sound->isEnableSound3DInfo();
        if (ImGui::Checkbox("Enable Sound 3D Info", &enableSound3DInfo))
        {
            sound->setEnableSound3DInfo(enableSound3DInfo);
        }

        if (ImGui::CollapsingHeader("Sound 3D Info"))
        {
            if (!enableSound3DInfo)
                ImGui::BeginDisabled();

            Sound::Sound3DInfo& sound3DInfo = sound->getSound3DInfo();

            {
                u32 flags = enableSound3DInfo ? sound3DInfo.getFlags() : 0;
                //if (ImGui::InputScalar("Flags", ImGuiDataType_U32, &flags, &cStepU32))
                //{
                //    sound3DInfo.setFlags(flags);
                //}

                CenteredTextX("Flags");

                if (ImGui::CheckboxFlagsT<u32>("Volume", &flags, Sound::Sound3DInfo::Flags::Volume))
                {
                    sound3DInfo.setFlags(flags);
                }

                ImGui::SameLine();

                if (ImGui::CheckboxFlagsT<u32>("Priority", &flags, Sound::Sound3DInfo::Flags::Priority))
                {
                    sound3DInfo.setFlags(flags);
                }

                ImGui::SameLine();

                if (ImGui::CheckboxFlagsT<u32>("Pan", &flags, Sound::Sound3DInfo::Flags::Pan))
                {
                    sound3DInfo.setFlags(flags);
                }

                ImGui::SameLine();

                if (ImGui::CheckboxFlagsT<u32>("SPan", &flags, Sound::Sound3DInfo::Flags::SPan))
                {
                    sound3DInfo.setFlags(flags);
                }

                ImGui::SameLine();

                if (ImGui::CheckboxFlagsT<u32>("Filter", &flags, Sound::Sound3DInfo::Flags::Filter))
                {
                    sound3DInfo.setFlags(flags);
                }
            }

            {
                f32 decayRatio = enableSound3DInfo ? sound3DInfo.getDecayRatio() : 0.5f;
                if (ImGui::SliderFloat("Decay Ratio", &decayRatio, 0.0f, 1.0f))
                {
                    sound3DInfo.setDecayRatio(decayRatio);
                }
            }

            {
                static const char* sCurveTypes[] = { "Logarithmic", "Linear" };

                u32 decayCurve = (enableSound3DInfo ? sound3DInfo.getDecayCurve() : Sound::Sound3DInfo::DecayCurve::Logarithmic) - 1;
                if (ImGui::Combo("Decay Curve", (s32*)&decayCurve, sCurveTypes, IM_ARRAYSIZE(sCurveTypes)))
                {
                    sound3DInfo.setDecayCurve(decayCurve + 1);
                }
            }

            {
                u8 dopplerFactor = enableSound3DInfo ? sound3DInfo.getDopplerFactor() : 0;
                if (ImGui::InputScalar("Doppler Factor", ImGuiDataType_U8, &dopplerFactor, &cStepU8))
                {
                    sound3DInfo.setDopplerFactor(dopplerFactor);
                }
            }

            if (!enableSound3DInfo)
                ImGui::EndDisabled();

            ImGui::Separator();
        }
    }

    {
        static const char* sSoundTypes[] = {
            "Sequence",
            "Stream",
            "Wave"
        };

        u32 soundType = (u32)sound->getSoundType() - 1;
        if (ImGui::Combo("Sound Type", (s32*)&soundType, sSoundTypes, IM_ARRAYSIZE(sSoundTypes)))
        {
            sound->setSoundType(static_cast<Sound::SoundType>(soundType + 1));
        }
    }

    if (ImGui::BeginTabBar("SoundTabBar"))
    {
        bool isSeq = sound->getSoundType() == Sound::SoundType::Seq;
        bool isStrm = sound->getSoundType() == Sound::SoundType::Strm;
        bool isWave = sound->getSoundType() == Sound::SoundType::Wave;

        if (!isSeq)
            ImGui::BeginDisabled();

        if (ImGui::BeginTabItem("Sequence", nullptr, isSeq ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
        {
            Sound::SequenceSoundInfo& seqSoundInfo = sound->getSequenceSoundInfo();

            {
                Item* seqFile = seqSoundInfo.getSequenceFileRef().getItem();
                if (ItemSelector("Sequence File", sBfsar.getSequenceFileList(), &seqFile))
                {
                    seqSoundInfo.getSequenceFileRef().attach(seqFile);
                }
            }

            //if (ImGui::CollapsingHeader("Banks"))
            {
                for (u32 i = 0; i < 4; i++)
                {
                    Item* bank = seqSoundInfo.getBankRef(i).getItem();
                    if (ItemSelector(sead::FormatFixedSafeString<16>("Bank %u", i).cstr(), sBfsar.getBankList(), &bank, true))
                    {
                        seqSoundInfo.getBankRef(i).attach(bank);
                    }
                }

                //ImGui::Separator();
            }

            {
                u32 allocateTrackFlags = seqSoundInfo.getAllocateTrackFlags();
                //if (ImGui::InputScalar("Allocate Track Flags", ImGuiDataType_U32, &allocateTrackFlags, &cStepU32))
                //{
                //    seqSoundInfo.setAllocateTrackFlags(allocateTrackFlags);
                //}

                CenteredTextX("Allocate Tracks");

                for (u32 i = 0; i < 16; i++)
                {
                    if (ImGui::CheckboxFlagsT<u32>(sead::FormatFixedSafeString<8>("###%u", i).cstr(), &allocateTrackFlags, 1 << i))
                    {
                        seqSoundInfo.setAllocateTrackFlags(allocateTrackFlags);
                    }

                    ImGui::SameLine();
                    f32 cursorPos = ImGui::GetCursorPosX();
                    ImGui::Text("%u", i);

                    if ((i + 1) % 8 != 0)
                    {
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(cursorPos + 30.0f);
                    }
                }
            }

            {
                bool enableStartOffset = seqSoundInfo.isEnableStartOffset();
                if (ImGui::Checkbox("Enable Start Offset", &enableStartOffset))
                {
                    seqSoundInfo.setEnableStartOffset(enableStartOffset);
                }

                if (!enableStartOffset)
                    ImGui::BeginDisabled();

                {
                    u32 startOffset = seqSoundInfo.getStartOffset();
                    if (ImGui::InputScalar("Start Offset", ImGuiDataType_U32, &startOffset, &cStepU32))
                    {
                        seqSoundInfo.setStartOffset(startOffset);
                    }
                }

                if (!enableStartOffset)
                    ImGui::EndDisabled();
            }

            {
                bool enablePriority = seqSoundInfo.isEnablePriority();
                if (ImGui::Checkbox("Enable Priority", &enablePriority))
                {
                    seqSoundInfo.setEnablePriority(enablePriority);
                }

                if (!enablePriority)
                    ImGui::BeginDisabled();

                {
                    u8 channelPriority = seqSoundInfo.getChannelPriority();
                    if (ImGui::InputScalar("Channel Priority", ImGuiDataType_U8, &channelPriority, &cStepU8))
                    {
                        seqSoundInfo.setChannelPriority(channelPriority);
                    }
                }

                {
                    bool isReleasePriorityFix = seqSoundInfo.getIsReleasePriorityFix();
                    if (ImGui::Checkbox("Fix Priority At Release", &isReleasePriorityFix))
                    {
                        seqSoundInfo.setIsReleasePriorityFix(isReleasePriorityFix);
                    }
                }

                if (!enablePriority)
                    ImGui::EndDisabled();
            }

            ImGui::EndTabItem();
        }

        if (!isSeq)
            ImGui::EndDisabled();

        if (!isStrm)
            ImGui::BeginDisabled();

        if (ImGui::BeginTabItem("Stream", nullptr, isStrm ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
        {
            Sound::StreamSoundInfo& strmSoundInfo = sound->getStreamSoundInfo();

            {
                sead::FixedSafeString<512> path(strmSoundInfo.getPath());
                if (ImGui::InputText("Path", path.getBuffer(), path.getBufferSize(), ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit())
                {
                    if (path != strmSoundInfo.getPath())
                    {
                        strmSoundInfo.getPath() = path;
                    }
                }
            }

            {
                u32 allocateTrackFlags = strmSoundInfo.getAllocateTrackFlags();
                //if (ImGui::InputScalar("Allocate Track Flags", ImGuiDataType_U16, &allocateTrackFlags, &cStepU16))
                //{
                //    strmSoundInfo.setAllocateTrackFlags(allocateTrackFlags);
                //}

                CenteredTextX("Allocate Tracks");

                for (u32 i = 0; i < 8; i++)
                {
                    if (ImGui::CheckboxFlagsT<u32>(sead::FormatFixedSafeString<8>("###%u", i).cstr(), &allocateTrackFlags, 1 << i))
                    {
                        strmSoundInfo.setAllocateTrackFlags(allocateTrackFlags);
                    }

                    ImGui::SameLine();
                    f32 cursorPos = ImGui::GetCursorPosX();
                    ImGui::Text("%u", i);

                    if ((i + 1) % 8 != 0)
                    {
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(cursorPos + 30.0f);
                    }
                }
            }

            {
                u16 allocateChannelCount = strmSoundInfo.getAllocateChannelCount();
                if (ImGui::InputScalar("Allocate Channel Count", ImGuiDataType_U16, &allocateChannelCount, &cStepU16))
                {
                    strmSoundInfo.setAllocateChannelCount(allocateChannelCount);
                }
            }

            {
                bool enableTracks = sBfsar.isStreamTrackInfoAvailable();

                if (!enableTracks)
                    ImGui::BeginDisabled();

                // TODO
                ImGui::Text("Tracks... (%d)", strmSoundInfo.getTracks().size());

                if (!enableTracks)
                    ImGui::EndDisabled();
            }

            {
                bool enableSend = sBfsar.isStreamSendAvailable();

                if (!enableSend)
                    ImGui::BeginDisabled();

                {
                    f32 pitch = enableSend ? strmSoundInfo.getPitch() : 1.0f;
                    if (ImGui::SliderFloat("Pitch", &pitch, 0.0f, 8.0f))
                    {
                        strmSoundInfo.setPitch(pitch);
                    }
                }

                {
                    u8 mainSend = enableSend ? strmSoundInfo.getMainSend() : 127;
                    if (ImGui::InputScalar("Main Send", ImGuiDataType_U8, &mainSend, &cStepU8))
                    {
                        strmSoundInfo.setMainSend(mainSend);
                    }
                }

                for (u32 i = 0; i < 3; i++)
                {
                    u8 fxSend = enableSend ? strmSoundInfo.getFxSend(i) : 0;
                    if (ImGui::InputScalar(sead::FormatFixedSafeString<16>("Fx Send %u", i).cstr(), ImGuiDataType_U8, &fxSend, &cStepU8))
                    {
                        strmSoundInfo.setFxSend(i, fxSend);
                    }
                }

                bool enableStreamSoundExtension = enableSend ? strmSoundInfo.isEnableStreamSoundExtension() : false;
                if (ImGui::Checkbox("Enable Stream Sound Extension", &enableStreamSoundExtension))
                {
                    strmSoundInfo.setEnableStreamSoundExtension(enableStreamSoundExtension);
                }

                if (!enableSend)
                    ImGui::EndDisabled();

                if (ImGui::CollapsingHeader("Stream Sound Extension"))
                {
                    bool enableSoundExt = enableSend && enableStreamSoundExtension;

                    if (!enableSoundExt)
                        ImGui::BeginDisabled();

                    {
                        static const char* sStreamTypes[] = { "BFSTM", "ADTS" };

                        u32 streamType = (enableSend ? strmSoundInfo.getStreamType() : Sound::StreamSoundInfo::StreamType::NwStreamBinary) - 1;
                        if (ImGui::Combo("Stream Type", (s32*)&streamType, sStreamTypes, IM_ARRAYSIZE(sStreamTypes)))
                        {
                            strmSoundInfo.setStreamType(static_cast<Sound::StreamSoundInfo::StreamType>(streamType + 1));
                        }
                    }

                    {
                        bool isLoop = enableSend ? strmSoundInfo.getIsLoop() : false;
                        if (ImGui::Checkbox("Is Loop", &isLoop))
                        {
                            strmSoundInfo.setIsLoop(isLoop);
                        }
                    }

                    {
                        u32 loopStartFrame = enableSend ? strmSoundInfo.getLoopStartFrame() : 0;
                        if (ImGui::InputScalar("Loop Start Frame", ImGuiDataType_U32, &loopStartFrame, &cStepU32))
                        {
                            strmSoundInfo.setLoopStartFrame(loopStartFrame);
                        }
                    }

                    {
                        u32 loopEndFrame = enableSend ? strmSoundInfo.getLoopEndFrame() : 0;
                        if (ImGui::InputScalar("Loop End Frame", ImGuiDataType_U32, &loopEndFrame, &cStepU32))
                        {
                            strmSoundInfo.setLoopEndFrame(loopEndFrame);
                        }
                    }

                    if (!enableSoundExt)
                        ImGui::EndDisabled();

                    ImGui::Separator();
                }
            }

            {
                bool enablePrefetch = sBfsar.isStreamPrefetchAvailable();

                if (!enablePrefetch)
                    ImGui::BeginDisabled();

                {
                    // Item* prefetchFile = enablePrefetch ? strmSoundInfo.getPrefetchFileRef().getItem() : nullptr;
                    // if (ItemSelector("Prefetch File", sBfsar.getFileList(), &prefetchFile, true))
                    // {
                    //     strmSoundInfo.getPrefetchFileRef().attach(prefetchFile);
                    // }
                }

                if (!enablePrefetch)
                    ImGui::EndDisabled();
            }

            ImGui::EndTabItem();
        }

        if (!isStrm)
            ImGui::EndDisabled();

        if (!isWave)
            ImGui::BeginDisabled();

        if (ImGui::BeginTabItem("Wave", nullptr, isWave ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
        {
            Sound::WaveSoundInfo& waveSoundInfo = sound->getWaveSoundInfo();

            {
                // u32 index = waveSoundInfo.getIndex();
                // if (ImGui::InputScalar("Index", ImGuiDataType_U32, &index, &cStepU32))
                // {
                //     waveSoundInfo.setIndex(index);
                // }
            }

            {
                Item* waveFile = waveSoundInfo.getWaveFileRef().getItem();
                if (ItemSelector("Wave File", sBfsar.getWaveFileList(), &waveFile))
                {
                    waveSoundInfo.getWaveFileRef().attach(waveFile);
                }
            }

            {
                // u32 allocateTrackCount = waveSoundInfo.getAllocateTrackCount();
                // if (ImGui::InputScalar("Allocate Track Count", ImGuiDataType_U32, &allocateTrackCount, &cStepU32))
                // {
                //     waveSoundInfo.setAllocateTrackCount(allocateTrackCount);
                // }
            }

            {
                bool enablePriority = waveSoundInfo.isEnablePriority();
                if (ImGui::Checkbox("Enable Priority", &enablePriority))
                {
                    waveSoundInfo.setEnablePriority(enablePriority);
                }

                if (!enablePriority)
                    ImGui::BeginDisabled();

                {
                    u8 channelPriority = waveSoundInfo.getChannelPriority();
                    if (ImGui::InputScalar("Channel Priority", ImGuiDataType_U8, &channelPriority, &cStepU8))
                    {
                        waveSoundInfo.setChannelPriority(channelPriority);
                    }
                }

                {
                    bool isReleasePriorityFix = waveSoundInfo.getIsReleasePriorityFix();
                    if (ImGui::Checkbox("Fix Priority At Release", &isReleasePriorityFix))
                    {
                        waveSoundInfo.setIsReleasePriorityFix(isReleasePriorityFix);
                    }
                }

                if (!enablePriority)
                    ImGui::EndDisabled();
            }

            ImGui::EndTabItem();
        }

        if (!isWave)
            ImGui::EndDisabled();

        ImGui::EndTabBar();
    }

    if (false)
    {
        if (ImGui::Button("Parse Sequence File"))
        {
            if (sound->getSoundType() != Sound::SoundType::Seq)
                return;

            SEAD_PRINT("FSEQ for sound: %s\n", sound->getNameOrNull().cstr());

            //ParseSequenceFile(sound->getFileId());
        }
    }
}
