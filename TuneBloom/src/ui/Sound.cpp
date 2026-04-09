#include <ui/UI.h>

#include <imgui/imgui_custom.h>

Sound::~Sound()
{
    if (this == sSoundPlayer.getLastPlayedSound())
    {
        sSoundPlayer.resetLastPlayedSound();
    }
}

const Item* Sound::validate(sead::BufferedSafeString& error) const
{
    if (!Item::validateName(error))
    {
        return this;
    }

    if (!getPlayerRef().isAttached())
    {
        error = "Invalid Player";
        return this;
    }

    switch (mSoundType)
    {
        case SoundType::Seq:
        {
            const Sound::SequenceSoundInfo& seqInfo = getSequenceSoundInfo();
            if (!seqInfo.getSequenceFileRef().isAttached())
            {
                error = "Invalid Sequence File";
                return this;
            }

            const SequenceFile& seqFile = *static_cast<const SequenceFile*>(seqInfo.getSequenceFileRef().getItem());
            if (seqFile.getLabelOffset(seqInfo.getStartLabel()) == SequenceFile::cInvaldOffset)
            {
                error = "Invalid Start Label (Is Sequence File compiled ?)";
                return this;
            }

            break;
        }

        case SoundType::Strm:
        {
            const Sound::StreamSoundInfo& strmInfo = getStreamSoundInfo();
            if (strmInfo.getPath().isEmpty())
            {
                error = "Path is empty";
                return this;
            }

            if (strmInfo.getStreamType() != Sound::StreamSoundInfo::StreamType::NwStreamBinary)
            {
                error = "Only BFSTM streams are supported";
                return this;
            }

            const Sound::StreamSoundInfo::Track::List& tracks = strmInfo.getTrackList();
            if (tracks.isEmpty())
            {
                error = "Streams must have at least 1 Track";
                return this;
            }

            if (tracks.size() > 8)
            {
                error = "Streams can only have up to 8 Tracks";
                return this;
            }

            WaveFile::Encoding mainEncoding = WaveFile::Encoding::DspAdpcm;
            u32 mainSampleRate = 0;
            for (s32 i = 0; i < tracks.size(); i++)
            {
                const Sound::StreamSoundInfo::Track& track = *static_cast<const Sound::StreamSoundInfo::Track*>(tracks.nth(i)->val());
                if (!track.getWaveFileRef().isAttached())
                {
                    error.format("Track %i: Invalid Wave File", i);
                    return &track;
                }

                const WaveFile& waveFile = *static_cast<const WaveFile*>(track.getWaveFileRef().getItem());

                if (i == 0)
                {
                    mainEncoding = waveFile.getEncoding();
                    mainSampleRate = waveFile.getSampleRate();
                }
                else
                {
                    if (mainEncoding != waveFile.getEncoding())
                    {
                        error = "All Stream Tracks must have the same encoding";
                        return this;
                    }

                    if (mainSampleRate != waveFile.getSampleRate())
                    {
                        error = "All Stream Tracks must have the same sample rate";
                        return this;
                    }
                }
            }

            break;
        }

        case SoundType::Wave:
        {
            const Sound::WaveSoundInfo& waveInfo = getWaveSoundInfo();
            if (!waveInfo.getWaveFileRef().isAttached())
            {
                error = "Invalid Wave File";
                return this;
            }

            break;
        }

        default:
            error = "Invalid Sound Type";
            return this;
    }

    return nullptr;
}

void DrawSoundPropertiesUI()
{
    Sound* sound = static_cast<Sound*>(sSelectedItem);

    const ImS8 cStepS8 = 1;
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
        bool isStrm = sound->getSoundType() == Sound::SoundType::Strm;

        if (isStrm)
            ImGui::BeginDisabled();

        bool enableIsFrontBypass = !isStrm ? sound->isEnableIsFrontBypass() : false;
        if (ImGui::Checkbox("Enable Front Bypass", &enableIsFrontBypass))
        {
            sound->setEnableIsFrontBypass(enableIsFrontBypass);
        }

        if (isStrm)
            ImGui::EndDisabled();

        if (!enableIsFrontBypass)
            ImGui::BeginDisabled();

        {
            bool isFrontBypass = !isStrm ? sound->getIsFrontBypass() : false;
            if (ImGui::Checkbox("Front Bypass", &isFrontBypass))
            {
                sound->setIsFrontBypass(isFrontBypass);
            }
        }

        if (!enableIsFrontBypass)
            ImGui::EndDisabled();

        if (isStrm)
        {
            ImGui::SameLine();
            HelpMarker("For Stream Sounds this field is specified in each Track");
        }
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

                if (ImGui::CheckboxFlags("Volume", &flags, Sound::Sound3DInfo::Flags::Volume))
                {
                    sound3DInfo.setFlags(flags);
                }

                ImGui::SameLine();

                if (ImGui::CheckboxFlags("Priority", &flags, Sound::Sound3DInfo::Flags::Priority))
                {
                    sound3DInfo.setFlags(flags);
                }

                ImGui::SameLine();

                if (ImGui::CheckboxFlags("Pan", &flags, Sound::Sound3DInfo::Flags::Pan))
                {
                    sound3DInfo.setFlags(flags);
                }

                ImGui::SameLine();

                if (ImGui::CheckboxFlags("Surround Pan", &flags, Sound::Sound3DInfo::Flags::SPan))
                {
                    sound3DInfo.setFlags(flags);
                }

                ImGui::SameLine();

                if (ImGui::CheckboxFlags("Filter", &flags, Sound::Sound3DInfo::Flags::Filter))
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

            // {
            //     u32 allocateTrackFlags = seqSoundInfo.getAllocateTrackFlags();
            //     //if (ImGui::InputScalar("Allocate Track Flags", ImGuiDataType_U32, &allocateTrackFlags, &cStepU32))
            //     //{
            //     //    seqSoundInfo.setAllocateTrackFlags(allocateTrackFlags);
            //     //}

            //     CenteredTextX("Allocate Tracks");

            //     for (u32 i = 0; i < 16; i++)
            //     {
            //         if (ImGui::CheckboxFlagsT<u32>(sead::FormatFixedSafeString<8>("###%u", i).cstr(), &allocateTrackFlags, 1 << i))
            //         {
            //             seqSoundInfo.setAllocateTrackFlags(allocateTrackFlags);
            //         }

            //         ImGui::SameLine();
            //         f32 cursorPos = ImGui::GetCursorPosX();
            //         ImGui::Text("%u", i);

            //         if ((i + 1) % 8 != 0)
            //         {
            //             ImGui::SameLine();
            //             ImGui::SetCursorPosX(cursorPos + 30.0f);
            //         }
            //     }
            // }

            {
                bool enableStartOffset = seqSoundInfo.isEnableStartOffset();
                if (ImGui::Checkbox("Enable Start Offset", &enableStartOffset))
                {
                    seqSoundInfo.setEnableStartOffset(enableStartOffset);
                }

                if (!enableStartOffset)
                    ImGui::BeginDisabled();

                {
                    const char** labels = nullptr;
                    u32 labelCount = 0;

                    Item* seqFile = seqSoundInfo.getSequenceFileRef().getItem();
                    if (seqFile)
                    {
                        SequenceFile* seq = static_cast<SequenceFile*>(seqFile);
                        const std::vector<std::string>& labelsVec = seq->getLabels();

                        labelCount = labelsVec.size();
                        if (labelCount > 0)
                        {
                            labels = new const char*[labelCount];
                            for (u32 i = 0; i < labelCount; i++)
                            {
                                labels[i] = labelsVec[i].c_str();
                            }
                        }
                    }

                    sead::FixedSafeString<128> startLabel = enableStartOffset ? seqSoundInfo.getStartLabel() : sead::FixedSafeString<128>();
                    if (ImGui::InputTextCombo("Start Label", startLabel.getBuffer(), startLabel.getBufferSize(), labels, labelCount))
                    {
                        seqSoundInfo.getStartLabel() = startLabel;
                    }

                    delete[] labels;
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

            // {
            //     u32 allocateTrackFlags = strmSoundInfo.getAllocateTrackFlags();
            //     //if (ImGui::InputScalar("Allocate Track Flags", ImGuiDataType_U16, &allocateTrackFlags, &cStepU16))
            //     //{
            //     //    strmSoundInfo.setAllocateTrackFlags(allocateTrackFlags);
            //     //}

            //     CenteredTextX("Allocate Tracks");

            //     for (u32 i = 0; i < 8; i++)
            //     {
            //         if (ImGui::CheckboxFlagsT<u32>(sead::FormatFixedSafeString<8>("###%u", i).cstr(), &allocateTrackFlags, 1 << i))
            //         {
            //             strmSoundInfo.setAllocateTrackFlags(allocateTrackFlags);
            //         }

            //         ImGui::SameLine();
            //         f32 cursorPos = ImGui::GetCursorPosX();
            //         ImGui::Text("%u", i);

            //         if ((i + 1) % 8 != 0)
            //         {
            //             ImGui::SameLine();
            //             ImGui::SetCursorPosX(cursorPos + 30.0f);
            //         }
            //     }
            // }

            // {
            //     u16 allocateChannelCount = strmSoundInfo.getAllocateChannelCount();
            //     if (ImGui::InputScalar("Allocate Channel Count", ImGuiDataType_U16, &allocateChannelCount, &cStepU16))
            //     {
            //         strmSoundInfo.setAllocateChannelCount(allocateChannelCount);
            //     }
            // }

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
                        static const char* sStreamTypes[] = { "BFSTM", "ADTS (AAC)" };

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

                        if (strmSoundInfo.getStreamType() == Sound::StreamSoundInfo::StreamType::NwStreamBinary)
                        {
                            ImGui::SameLine();

                            if (!enableSoundExt)
                            {
                                ImGui::EndDisabled();
                            }

                            HelpMarker(
                                "Note: For BFSTM Streams this looping info is ignored\n"
                                "and instead is taken from the first Track attached Wave File"
                            );

                            if (!enableSoundExt)
                            {
                                ImGui::BeginDisabled();
                            }
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

            {
                bool enablePan = waveSoundInfo.isEnablePan();
                if (ImGui::Checkbox("Enable Pan", &enablePan))
                {
                    waveSoundInfo.setEnablePan(enablePan);
                }

                if (!enablePan)
                    ImGui::BeginDisabled();

                {
                    u8 pan = waveSoundInfo.getPan();
                    if (ImGui::InputScalar("Pan", ImGuiDataType_U8, &pan, &cStepU8))
                    {
                        waveSoundInfo.setPan(pan);
                    }
                }

                {
                    s8 surroundPan = waveSoundInfo.getSurroundPan();
                    if (ImGui::InputScalar("Surround Pan", ImGuiDataType_S8, &surroundPan, &cStepS8))
                    {
                        waveSoundInfo.setSurroundPan(surroundPan);
                    }
                }

                if (!enablePan)
                    ImGui::EndDisabled();
            }

            {
                bool enablePitch = waveSoundInfo.isEnablePitch();
                if (ImGui::Checkbox("Enable Pitch", &enablePitch))
                {
                    waveSoundInfo.setEnablePitch(enablePitch);
                }

                if (!enablePitch)
                    ImGui::BeginDisabled();

                {
                    f32 pitch = waveSoundInfo.getPitch();
                    if (ImGui::SliderFloat("Pitch", &pitch, 0.0f, 8.0f))
                    {
                        waveSoundInfo.setPitch(pitch);
                    }
                }

                if (!enablePitch)
                    ImGui::EndDisabled();
            }

            {
                bool enableSend = waveSoundInfo.isEnableSend();
                if (ImGui::Checkbox("Enable Send", &enableSend))
                {
                    waveSoundInfo.setEnableSend(enableSend);
                }

                if (!enableSend)
                    ImGui::BeginDisabled();

                {
                    u8 mainSend = waveSoundInfo.getMainSend();
                    if (ImGui::InputScalar("Main Send", ImGuiDataType_U8, &mainSend, &cStepU8))
                    {
                        waveSoundInfo.setMainSend(mainSend);
                    }
                }

                for (u32 i = 0; i < 3; i++)
                {
                    u8 fxSend = waveSoundInfo.getFxSend(i);
                    if (ImGui::InputScalar(sead::FormatFixedSafeString<16>("Fx Send %u", i).cstr(), ImGuiDataType_U8, &fxSend, &cStepU8))
                    {
                        waveSoundInfo.setFxSend(i, fxSend);
                    }
                }

                if (!enableSend)
                    ImGui::EndDisabled();
            }

            {
                bool enableEnvelope = waveSoundInfo.isEnableEnvelope();
                if (ImGui::Checkbox("Enable Envelope", &enableEnvelope))
                {
                    waveSoundInfo.setEnableEnvelope(enableEnvelope);
                }

                if (!enableEnvelope)
                    ImGui::BeginDisabled();

                {
                    snd::AdshrCurve adshrCurve = waveSoundInfo.getAdshrCurve();

                    bool edited = false;
                    // if (ImGui::InputScalar("Attack", ImGuiDataType_U8, &adshrCurve.attack, &cStepU8))
                    // {
                    //     edited = true;
                    // }

                    // if (ImGui::InputScalar("Decay", ImGuiDataType_U8, &adshrCurve.decay, &cStepU8))
                    // {
                    //     edited = true;
                    // }

                    // if (ImGui::InputScalar("Sustain", ImGuiDataType_U8, &adshrCurve.sustain, &cStepU8))
                    // {
                    //     edited = true;
                    // }

                    // if (ImGui::InputScalar("Hold", ImGuiDataType_U8, &adshrCurve.hold, &cStepU8))
                    // {
                    //     edited = true;
                    // }

                    if (ImGui::InputScalar("Release", ImGuiDataType_U8, &adshrCurve.release, &cStepU8))
                    {
                        edited = true;
                    }

                    if (edited)
                    {
                        waveSoundInfo.setAdshrCurve(adshrCurve);
                    }
                }

                if (!enableEnvelope)
                    ImGui::EndDisabled();
            }

            {
                bool enableFilter = waveSoundInfo.isEnableFilter();
                if (ImGui::Checkbox("Enable Filter", &enableFilter))
                {
                    waveSoundInfo.setEnableFilter(enableFilter);
                }

                if (!enableFilter)
                    ImGui::BeginDisabled();

                {
                    u8 lpfFreq = waveSoundInfo.getLpfFreq();
                    if (ImGui::InputScalar("LPF Frequency", ImGuiDataType_U8, &lpfFreq, &cStepU8))
                    {
                        waveSoundInfo.setLpfFreq(lpfFreq);
                    }
                }

                {
                    u8 biquadType = waveSoundInfo.getBiquadType();
                    if (ImGui::InputScalar("Biquad Type", ImGuiDataType_U8, &biquadType, &cStepU8)) // TODO: Combo ?
                    {
                        waveSoundInfo.setBiquadType(biquadType);
                    }
                }

                {
                    u8 biquadValue = waveSoundInfo.getBiquadValue();
                    if (ImGui::InputScalar("Biquad Value", ImGuiDataType_U8, &biquadValue, &cStepU8))
                    {
                        waveSoundInfo.setBiquadValue(biquadValue);
                    }
                }

                if (!enableFilter)
                    ImGui::EndDisabled();
            }

            ImGui::EndTabItem();
        }

        if (!isWave)
            ImGui::EndDisabled();

        ImGui::EndTabBar();
    }
}

void Sound::StreamSoundInfo::Track::drawUI()
{
    const ImU8 cStepU8 = 1;

    {
        Item* waveFile = getWaveFileRef().getItem();
        if (ItemSelector("Wave File", sBfsar.getWaveFileList(), &waveFile))
        {
            getWaveFileRef().attach(waveFile);
        }
    }

    {
        u8 volume = getVolume();
        if (ImGui::InputScalar(sead::FormatFixedSafeString<32>("Volume (%.3f)###vol", static_cast<f32>(volume) / 127.0f).cstr(), ImGuiDataType_U8, &volume, &cStepU8))
        {
            setVolume(volume);
        }
    }

    {
        u8 pan = getPan();
        if (ImGui::InputScalar("Pan", ImGuiDataType_U8, &pan, &cStepU8))
        {
            setPan(pan);
        }
    }

    {
        u8 span = getSPan();
        if (ImGui::InputScalar("Surround Pan", ImGuiDataType_U8, &span, &cStepU8))
        {
            setSPan(span);
        }
    }

    {
        bool flags = getFlags();
        if (ImGui::Checkbox("Front Bypass", &flags))
        {
            setFlags(flags);
        }
    }

    {
        bool enableSend = sBfsar.isStreamSendAvailable();
        if (!enableSend)
        {
            ImGui::BeginDisabled();
        }

        {
            u8 mainSend = getMainSend();
            if (ImGui::InputScalar("Main Send", ImGuiDataType_U8, &mainSend, &cStepU8))
            {
                setMainSend(mainSend);
            }
        }

        for (u32 i = 0; i < 3; i++)
        {
            u8 fxSend = getFxSend(i);
            if (ImGui::InputScalar(sead::FormatFixedSafeString<16>("Fx Send %u", i).cstr(), ImGuiDataType_U8, &fxSend, &cStepU8))
            {
                setFxSend(i, fxSend);
            }
        }

        if (!enableSend)
        {
            ImGui::EndDisabled();
        }
    }

    {
        bool enableFilter = sBfsar.isFilterSupportedVersion();
        if (!enableFilter)
        {
            ImGui::BeginDisabled();
        }

        {
            u8 lpfFreq = getLpfFreq();
            if (ImGui::InputScalar("LPF Frequency", ImGuiDataType_U8, &lpfFreq, &cStepU8))
            {
                setLpfFreq(lpfFreq);
            }
        }

        {
            u8 biquadType = getBiquadType();
            if (ImGui::InputScalar("Biquad Type", ImGuiDataType_U8, &biquadType, &cStepU8)) // TODO: Combo ?
            {
                setBiquadType(biquadType);
            }
        }

        {
            u8 biquadValue = getBiquadValue();
            if (ImGui::InputScalar("Biquad Value", ImGuiDataType_U8, &biquadValue, &cStepU8))
            {
                setBiquadValue(biquadValue);
            }
        }

        if (!enableFilter)
        {
            ImGui::EndDisabled();
        }
    }
}
