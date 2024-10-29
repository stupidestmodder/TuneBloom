#include <ui/UI.h>

#include <snd/SoundThread.h>

#include <framework/seadProcessMeter.h>

enum class UIType
{
    Min = 0,

    ProjectInfo = Min,

    AllSounds,
    StreamSounds,
    WaveSounds,
    SequenceSounds,

    AllSoundSets,
    WaveSoundSets,
    SequenceSoundSets,

    Banks,
    WaveArchives,
    Groups,
    Players,

    WaveFiles,
    SequenceFiles,
    BankFiles,

    Max = BankFiles
};

UIType sSelectedUIType = UIType::ProjectInfo;

bool sShowSystemWindow = false;
bool sShowDemoWindow = false;

ItemList sFileWindows;

// Windows
void DrawProjectUI();
void DrawInfoUI();
void DrawSubInfoUI();
void DrawFileUI(ImGuiID dockspaceId);
void DrawPropertiesUI();

void DrawAllSoundsUI();
void DrawStreamSoundsUI();
void DrawWaveSoundsUI();
void DrawSequenceSoundsUI();
void DrawAllSoundSetsUI();
void DrawWaveSoundSetsUI();
void DrawSequenceSoundSetsUI();
void DrawWaveFilesUI();
void DrawSequenceFilesUI();
void DrawBankFilesUI();

extern void HeapInfo();

static void DockBuilder(ImGuiID dockspaceId, const ImVec2& dockspaceSize)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, dockspaceSize);

    ImGuiID mainDockId = dockspaceId;

    ImGuiID dockDown = ImGui::DockBuilderSplitNode(mainDockId, ImGuiDir_Down, 0.15f, nullptr, &mainDockId);

    ImGuiID dock1 = ImGui::DockBuilderSplitNode(mainDockId, ImGuiDir_Left, 0.20f, nullptr, &mainDockId);
    ImGuiID dock2 = ImGui::DockBuilderSplitNode(mainDockId, ImGuiDir_Right, 0.50f, nullptr, &mainDockId);

    ImGui::DockBuilderDockWindow("###ProjectWindow", dock1);
    ImGui::DockBuilderDockWindow("###InfoWindow", mainDockId);
    ImGui::DockBuilderDockWindow("###PropertiesWindow", dock2);
    ImGui::DockBuilderDockWindow("###PlayerWindow", dockDown);

    ImGui::DockBuilderFinish(dockspaceId);
}

static ImGuiID DockSpaceOverViewport(const ImGuiViewport* viewport = nullptr, ImGuiDockNodeFlags dockspace_flags = 0, const ImGuiWindowClass* window_class = nullptr)
{
    if (viewport == NULL)
        viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        host_window_flags |= ImGuiWindowFlags_NoBackground;

    char label[32];
    ImFormatString(label, IM_ARRAYSIZE(label), "DockSpaceViewport_%08X", viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(label, NULL, host_window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("DockSpace");

    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
    {
        DockBuilder(dockspace_id, viewport->WorkSize);
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, window_class);
    ImGui::End();

    return dockspace_id;
}

void DrawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open"))
            {
                OpenFile();
            }

            bool bfsarOpen = sBfsar.isOpen();
            if (!bfsarOpen)
            {
                ImGui::BeginDisabled();
            }

            if (ImGui::MenuItem("Save"))
            {
                SaveFile();
            }

            if (ImGui::MenuItem("Save As"))
            {
                SaveFileAs();
            }

            if (ImGui::MenuItem("Close"))
            {
                CloseFile();
            }

            //if (ImGui::MenuItem("Load External BFGRP"))
            //{
            //    LoadBfgrp();
            //}

            if (!bfsarOpen)
            {
                ImGui::EndDisabled();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("More"))
        {
            ImGui::MenuItem("System Window", nullptr, &sShowSystemWindow);
            ImGui::MenuItem("Demo Window", nullptr, &sShowDemoWindow);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void DrawUI()
{
    ImGuiID dockspaceId = DockSpaceOverViewport();

    DrawProjectUI();
    DrawInfoUI();
    DrawSubInfoUI();
    DrawFileUI(dockspaceId);
    DrawPropertiesUI();
    DrawPlayerUI();

    if (sShowSystemWindow)
    {
        if (ImGui::Begin("System", &sShowSystemWindow))
        {
            HeapInfo();

            if (sead::ProcessMeter::instance())
            {
                bool isEnable = sead::ProcessMeter::instance()->isVisible();
                if (ImGui::Checkbox("ProcessMeter", &isEnable))
                    sead::ProcessMeter::instance()->setVisible(isEnable);
            }
        }
        ImGui::End();
    }

    if (sShowDemoWindow)
        ImGui::ShowDemoWindow(&sShowDemoWindow);
}

void DrawProjectUI()
{
    if (ImGui::Begin(ICON_LC_FOLDER " Project###ProjectWindow"))
    {
        bool selected = false;

        auto resetSelectedItemAndSubWindow = []()
        {
            sSelectedItem = nullptr;
            sSubSelectedItem = nullptr;
            sSelectedItemIsSubWindow = false;
        };

        auto resetSelectedItemAndSubWindowIfNotType = [&](Item::ItemType type)
        {
            if (sSelectedItem && sSelectedItem->getItemType() != type)
            {
                resetSelectedItemAndSubWindow();
            }
        };

        if (ImGui::Selectable(ICON_LC_INFO " Project Info", sSelectedUIType == UIType::ProjectInfo))
        {
            sSelectedUIType = UIType::ProjectInfo;
            selected = true;

            resetSelectedItemAndSubWindow();
        }

        ImGui::Separator();

        if (ImGui::Selectable(ICON_LC_LIST_MUSIC " All Sounds", sSelectedUIType == UIType::AllSounds))
        {
            sSelectedUIType = UIType::AllSounds;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::Sound);
        }

        if (ImGui::Selectable(ICON_LC_DISC_3 " Stream Sounds", sSelectedUIType == UIType::StreamSounds, ImGuiSelectableFlags_SpanAvailWidth))
        {
            sSelectedUIType = UIType::StreamSounds;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::Sound);
        }

        if (ImGui::Selectable(ICON_LC_AUDIO_LINES " Wave Sounds", sSelectedUIType == UIType::WaveSounds))
        {
            sSelectedUIType = UIType::WaveSounds;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::Sound);
        }

        if (ImGui::Selectable(ICON_LC_MUSIC_3 " Sequence Sounds", sSelectedUIType == UIType::SequenceSounds))
        {
            sSelectedUIType = UIType::SequenceSounds;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::Sound);
        }

        ImGui::Separator();

        if (ImGui::Selectable(ICON_LC_MUSIC_4 " All Sound Sets", sSelectedUIType == UIType::AllSoundSets))
        {
            sSelectedUIType = UIType::AllSoundSets;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::SoundSet);
        }

        if (ImGui::Selectable(ICON_LC_AUDIO_LINES " Wave Sound Sets", sSelectedUIType == UIType::WaveSoundSets))
        {
            sSelectedUIType = UIType::WaveSoundSets;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::SoundSet);
        }

        if (ImGui::Selectable(ICON_LC_MUSIC_2 " Sequence Sound Sets", sSelectedUIType == UIType::SequenceSoundSets))
        {
            sSelectedUIType = UIType::SequenceSoundSets;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::SoundSet);
        }

        ImGui::Separator();

        if (ImGui::Selectable(ICON_LC_PIANO " Banks", sSelectedUIType == UIType::Banks))
        {
            sSelectedUIType = UIType::Banks;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::Bank);
        }

        if (ImGui::Selectable(ICON_LC_FILE_MUSIC " Wave Archives", sSelectedUIType == UIType::WaveArchives))
        {
            sSelectedUIType = UIType::WaveArchives;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::WaveArchive);
        }

        if (ImGui::Selectable(ICON_LC_FOLDERS " Groups", sSelectedUIType == UIType::Groups))
        {
            sSelectedUIType = UIType::Groups;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::Group);
        }

        if (ImGui::Selectable(ICON_LC_VOLUME_2 " Players", sSelectedUIType == UIType::Players))
        {
            sSelectedUIType = UIType::Players;
            selected = true;

            resetSelectedItemAndSubWindowIfNotType(Item::ItemType::Player);
        }

        ImGui::Separator();

        if (ImGui::Selectable(ICON_LC_AUDIO_LINES " Wave Files", sSelectedUIType == UIType::WaveFiles))
        {
            sSelectedUIType = UIType::WaveFiles;
            selected = true;
        }

        if (ImGui::Selectable(ICON_LC_MUSIC_3 " Sequence Files", sSelectedUIType == UIType::SequenceFiles))
        {
            sSelectedUIType = UIType::SequenceFiles;
            selected = true;
        }

        if (ImGui::Selectable(ICON_LC_PIANO " Bank Files", sSelectedUIType == UIType::BankFiles))
        {
            sSelectedUIType = UIType::BankFiles;
            selected = true;
        }

        if (ImGui::IsWindowFocused())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                if (sSelectedUIType > UIType::Min)
                {
                    sSelectedUIType = UIType(u32(sSelectedUIType) - 1);
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                if (sSelectedUIType < UIType::Max)
                {
                    sSelectedUIType = UIType(u32(sSelectedUIType) + 1);
                }
            }
        }

        if (selected)
        {
            ImGuiWindow* w = ImGui::FindWindowByName("###FileWindow");
            if (w && w->DockIsActive && !w->Hidden)
            {
                ImGui::SetWindowFocus("###InfoWindow");
            }
        }
    }
    ImGui::End();
}

void DrawInfoUI()
{
    bool pushPadding = sSelectedUIType != UIType::ProjectInfo;
    if (pushPadding)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    }

    bool windowOpen = ImGui::Begin(ICON_LC_PEN " Info###InfoWindow", nullptr, sSelectedUIType == UIType::ProjectInfo ? ImGuiWindowFlags_NoScrollbar : 0);

    if (pushPadding)
    {
        ImGui::PopStyleVar();
    }

    if (windowOpen)
    {
        if (!sBfsar.getSoundArchive())
        {
            CenteredText("No File Open");

            ImGui::End();
            return;
        }

        switch (sSelectedUIType)
        {
            case UIType::ProjectInfo:
                DrawProjectInfoUI();
                break;

            case UIType::AllSounds:
                DrawAllSoundsUI();
                break;

            case UIType::StreamSounds:
                DrawStreamSoundsUI();
                break;

            case UIType::WaveSounds:
                DrawWaveSoundsUI();
                break;

            case UIType::SequenceSounds:
                DrawSequenceSoundsUI();
                break;

            case UIType::AllSoundSets:
                DrawAllSoundSetsUI();
                break;

            case UIType::WaveSoundSets:
                DrawWaveSoundSetsUI();
                break;

            case UIType::SequenceSoundSets:
                DrawSequenceSoundSetsUI();
                break;

            case UIType::Banks:
                DrawBanksUI();
                break;

            case UIType::WaveArchives:
                DrawWaveArchivesUI();
                break;

            case UIType::Groups:
                DrawGroupsUI();
                break;

            case UIType::Players:
                DrawPlayersUI();
                break;

            case UIType::WaveFiles:
                DrawWaveFilesUI();
                break;

            case UIType::SequenceFiles:
                DrawSequenceFilesUI();
                break;

            case UIType::BankFiles:
                DrawBankFilesUI();
                break;

            default:
                break;
        }
    }

    ImGui::End();
}

const char* GroupItemPrefixFunc(Item* item)
{
    Group::ItemInfo* groupItem = (Group::ItemInfo*)item;

    const char* icon = ICON_LC_FILE_QUESTION " ";
    switch (groupItem->getItemRefType())
    {
        case Item::ItemType::Sound:
            if (groupItem->getItemRef().isAttached())
            {
                Sound* sound = (Sound*)groupItem->getItemRef().getItem();
                switch (sound->getSoundType())
                {
                    case Sound::SoundType::Seq:
                        icon = ICON_LC_MUSIC_3 " ";
                        break;

                    case Sound::SoundType::Strm:
                        icon = ICON_LC_DISC_3 " ";
                        break;

                    case Sound::SoundType::Wave:
                        icon = ICON_LC_AUDIO_LINES " ";
                        break;
                }
            }

            break;

        case Item::ItemType::SoundSet:
            if (groupItem->getItemRef().isAttached())
            {
                SoundSet* soundSet = (SoundSet*)groupItem->getItemRef().getItem();
                switch (soundSet->getSoundSetType())
                {
                    case SoundSet::SoundSetType::Wave:
                        icon = ICON_LC_AUDIO_LINES " ";
                        break;

                    case SoundSet::SoundSetType::Seq:
                        icon = ICON_LC_MUSIC_2 " ";
                        break;
                }
            }

            break;

        case Item::ItemType::Bank:
            icon = ICON_LC_PIANO " ";
            break;

        case Item::ItemType::WaveArchive:
            icon = ICON_LC_FILE_MUSIC " ";
            break;
    }

    return icon;
}

void DrawSubInfoUI()
{
    if (!sBfsar.isOpen())
    {
        return;
    }

    sead::FixedSafeString<512> name(ICON_LC_PEN " ");

    switch (sSelectedUIType)
    {
        case UIType::AllSounds:
        {
            if (!sSelectedItem || sSelectedItem->getItemType() != Item::ItemType::Sound)
            {
                return;
            }

            const Sound* sound = static_cast<const Sound*>(sSelectedItem);
            if (sound->getSoundType() != Sound::SoundType::Strm)
            {
                return;
            }

            name.append("Tracks");
            break;
        }

        case UIType::StreamSounds:
            name.append("Tracks");
            break;

        case UIType::Groups:
            name.append("Items");
            break;

        default:
            return;
    }

    name.append("###SubInfoWindow");

    //ImGui::SetNextWindowDockID();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    //bool windowOpen = ImGui::Begin(name.cstr(), nullptr, ImGuiWindowFlags_NoMove);
    bool windowOpen = ImGui::Begin(name.cstr(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PopStyleVar();
    
    if (windowOpen)
    {
        if (!sSelectedItem)
        {
            CenteredText("No Item Selected");

            ImGui::End();
            return;
        }

        switch (sSelectedItem->getItemType())
        {
            case Item::ItemType::Sound:
            {
                Sound* sound = static_cast<Sound*>(sSelectedItem);

                if (sound->getSoundType() == Sound::SoundType::Strm)
                {
                    DrawAllItemsUI("Track", sound->getStreamSoundInfo().getTrackList());
                }
                break;
            }

            case Item::ItemType::Group:
            {
                Group* group = static_cast<Group*>(sSelectedItem);

                DrawAllItemsUI("Item", group->getItemInfoList(), nullptr, &GroupItemPrefixFunc);
                break;
            }
        }
    }

    ImGui::End();
}

void DrawPropertiesUI()
{
    sead::FixedSafeString<512> name(ICON_LC_TABLE_PROPERTIES " Properties");

    if (sSelectedItem && sSelectedItem->getItemType() < Item::ItemType::InternalFileTypes)
    {
        name.appendWithFormat(" - %s", sSelectedItem->getFormattedName().cstr());
    }

    name.append("###PropertiesWindow");

    if (ImGui::Begin(name.cstr()))
    {
        if (!sSelectedItem)
        {
            CenteredText("No Item Selected");

            ImGui::End();
            return;
        }

        if (sSelectedItemIsSubWindow)
        {
            if (!sSubSelectedItem)
            {
                CenteredText("No Item Selected");

                ImGui::End();
                return;
            }

            switch (sSubSelectedItem->getItemType())
            {
                case Item::ItemType::StreamTrack:
                {
                    Sound::StreamSoundInfo::Track* track = static_cast<Sound::StreamSoundInfo::Track*>(sSubSelectedItem);
                    track->drawUI();
                    break;
                }

                case Item::ItemType::GroupItemInfo:
                {
                    Group::ItemInfo* itemInfo = static_cast<Group::ItemInfo*>(sSubSelectedItem);
                    itemInfo->drawUI();
                    break;
                }
            }

            ImGui::End();
            return;
        }

        switch (sSelectedItem->getItemType())
        {
            case Item::ItemType::Sound:
            case Item::ItemType::SoundSet:
            case Item::ItemType::Bank:
            case Item::ItemType::WaveArchive:
            case Item::ItemType::Group:
            case Item::ItemType::Player:
                DrawItemPropertiesUI();
                ImGui::Separator();

                break;
        }

        switch (sSelectedItem->getItemType())
        {
            case Item::ItemType::Sound:
                DrawSoundPropertiesUI();
                break;

            case Item::ItemType::SoundSet:
                DrawSoundSetPropertiesUI();
                break;

            case Item::ItemType::Bank:
                DrawBankPropertiesUI();
                break;

            case Item::ItemType::WaveArchive:
                DrawWaveArchivePropertiesUI();
                break;

            case Item::ItemType::Group:
                DrawGroupPropertiesUI();
                break;

            case Item::ItemType::Player:
                DrawPlayerPropertiesUI();
                break;

            case Item::ItemType::WaveFile:
            {
                WaveFile* wave = static_cast<WaveFile*>(sSelectedItem);
                wave->drawUI();
                break;
            }

            case Item::ItemType::SequenceFile:
            {
                SequenceFile* sequenceFile = static_cast<SequenceFile*>(sSelectedItem);
                sequenceFile->drawUI();
                break;
            }

            case Item::ItemType::BankFile:
            {
                BankFile* bankFile = static_cast<BankFile*>(sSelectedItem);
                bankFile->drawUI();
                break;
            }

            case Item::ItemType::BankFileInstrument:
            {
                BankFile::Instrument* instrument = static_cast<BankFile::Instrument*>(sSelectedItem);
                instrument->drawUI();
                break;
            }
        }
    }
    ImGui::End();
}

void DrawFileUI(ImGuiID dockspaceId)
{
    for (auto it = sFileWindows.robustBegin(); it != sFileWindows.robustEnd(); ++it)
    {
        FileWindow* window = static_cast<FileWindow*>(it->val());
        SEAD_ASSERT(window);

        if (!window->isOpen() || !window->getFileRef().isAttached())
        {
            delete window;
        }
    }

    for (Item* item : sFileWindows)
    {
        FileWindow* window = static_cast<FileWindow*>(item);

        Item* file = window->getFileRef().getItem();

        ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_Appearing);

        bool dirty = false;
        ImGuiWindowFlags flags = ImGuiWindowFlags_None;
        if (file->getItemType() == Item::ItemType::SequenceFile)
        {
            SequenceFile* seq = static_cast<SequenceFile*>(file);
            flags = ImGuiWindowFlags_MenuBar;

            dirty = seq->isDirty();
            flags |= dirty ? ImGuiWindowFlags_UnsavedDocument : 0;
        }

        bool visible = ImGui::Begin(sead::FormatFixedSafeString<512>(ICON_LC_FILE " %s###%u", file->getFormattedName().cstr(), file).cstr(), window->getOpenPtr(), flags);

        if (!window->isOpen() && dirty)
        {
            *window->getOpenPtr() = true;
            // TODO: Open Popup
        }

        if (visible)
        {
            switch (file->getItemType())
            {
                case Item::ItemType::BankFile:
                {
                    BankFile* bank = static_cast<BankFile*>(file);
                    bank->drawFileUI();
                    break;
                }

                case Item::ItemType::SequenceFile:
                {
                    SequenceFile* seq = static_cast<SequenceFile*>(file);
                    seq->drawFileUI();
                    break;
                }
            }
        }
        ImGui::End();
    }
}

void DrawProjectInfoUI()
{
    const ImU8 stepU8 = 1;
    const ImU16 stepU16 = 1;
    const ImU32 stepU32 = 1;

    {
        static const char* sEndianTypes[] = {
            "Big Endian",
            "Little Endian"
        };

        sead::Endian::Types endian = sBfsar.getEndian();
        if (ImGui::Combo("Byte Order", (s32*)&endian, sEndianTypes, IM_ARRAYSIZE(sEndianTypes)))
        {
            sBfsar.setEndian(endian);
        }
    }

    {
        CenteredTextX("Version");

        u32 version = sBfsar.getVersion();
        if (DrawVersionUI(&version))
        {
            sBfsar.setVersion(version);
        }
    }

    //ImGui::SeparatorText("");
    {
        bool stringTable = sBfsar.isIncludeStringTable();
        if (ImGui::Checkbox("Include String Table", &stringTable))
            sBfsar.setIncludeStringTable(stringTable);
    }

    ImGui::SeparatorText("Sound Archive Player");

    {
        SoundArchivePlayerInfo& playerInfo = sBfsar.getSoundArchivePlayerInfo();

        ImGui::InputScalar("Sequence Sound Max", ImGuiDataType_U16, &playerInfo.sequenceSoundMax, &stepU16);
        ImGui::InputScalar("Sequence Track Max", ImGuiDataType_U16, &playerInfo.sequenceTrackMax, &stepU16);
        ImGui::InputScalar("Stream Sound Max", ImGuiDataType_U16, &playerInfo.streamSoundMax, &stepU16);
        ImGui::InputScalar("Stream Track Max", ImGuiDataType_U16, &playerInfo.streamTrackMax, &stepU16);
        ImGui::InputScalar("Stream Channel Max", ImGuiDataType_U16, &playerInfo.streamChannelMax, &stepU16);
        ImGui::InputScalar("Wave Sound Max", ImGuiDataType_U16, &playerInfo.waveSoundMax, &stepU16);
        ImGui::InputScalar("Wave Track Max", ImGuiDataType_U16, &playerInfo.waveTrackMax, &stepU16);

        {
            bool enable = sBfsar.isStreamPrefetchAvailable();
            if (!enable)
                ImGui::BeginDisabled();

            u8 streamBufferTimes = enable ? playerInfo.streamBufferTimes : 0;
            if (ImGui::InputScalar(enable ? "Stream Buffer Times" : "##", ImGuiDataType_U8, &streamBufferTimes, &stepU8))
            {
                playerInfo.streamBufferTimes = streamBufferTimes;
            }

            if (!enable)
                ImGui::EndDisabled();

            if (!enable)
            {
                ImGui::SameLine();
                HelpMarker("Need version to be >= 0.2.2.0");
            }
        }

        ImGui::InputScalar("Options", ImGuiDataType_U32, &playerInfo.options, &stepU32);
    }
}

InstanciateItemCallback CreateSoundFunc(bool clear)
{
    auto innerFunc = [](bool clear, Item* item, bool* validate)
    {
        static Item* sPlayer = nullptr;
        static Sound::SoundType sSoundType = Sound::SoundType::Invalid;

        if (clear)
        {
            sPlayer = sBfsar.getItem(0, sBfsar.getPlayerList());
            sSoundType = Sound::SoundType::Invalid;
        }

        if (!item && !validate)
        {
            ImGui::Separator();

            ItemSelector("Player", sBfsar.getPlayerList(), &sPlayer, false);

            static const char* sSoundTypes[] = { "Invalid", "Sequence", "Stream", "Wave" };
            ImGui::Combo("Sound Type", (s32*)&sSoundType, sSoundTypes, IM_ARRAYSIZE(sSoundTypes));

            WarningPopup("###Player", "Select a valid Player !" "\nIf there are none please add one first.");
            WarningPopup("###SoundType", "Select a valid Sound Type !");
        }
        else if (validate)
        {
            if (!sPlayer)
            {
                ImGui::OpenPopup("###Player");
                *validate = false;
            }
            else if (sSoundType == Sound::SoundType::Invalid)
            {
                ImGui::OpenPopup("###SoundType");
                *validate = false;
            }
        }
        else
        {
            Sound* sound = static_cast<Sound*>(item);

            sound->getPlayerRef().attach(sPlayer);
            sound->setSoundType(sSoundType);
        }
    };

    return CreateItemFunc(clear, []() -> Item* { return new Sound(); }, innerFunc);
}

const char* SoundNamePrefixFunc(Item* item)
{
    Sound* sound = (Sound*)item;

    if (ImGui::Button(sead::FormatFixedSafeString<32>(ICON_LC_PLAY "###%u", sound->getId()).cstr()))
    {
        if (sound != sSelectedItem)
        {
            sSubSelectedItem = nullptr;
            sSelectedItemIsSubWindow = false;
        }

        PlaySound(sound);
    }

    ImGui::SameLine();

    const char* icon = ICON_LC_FILE_QUESTION " ";
    switch (sound->getSoundType())
    {
        case Sound::SoundType::Seq:
            icon = ICON_LC_MUSIC_3 " ";
            break;

        case Sound::SoundType::Strm:
            icon = ICON_LC_DISC_3 " ";
            break;

        case Sound::SoundType::Wave:
            icon = ICON_LC_AUDIO_LINES " ";
            break;
    }

    return icon;
}

void DrawAllSoundsUI()
{
    DrawAllItemsUI("Sound", sBfsar.getSoundList(), &CreateSoundFunc, &SoundNamePrefixFunc);
}

const char* SoundNamePrefixFunc2(Item* item)
{
    Sound* sound = (Sound*)item;

    if (ImGui::Button(sead::FormatFixedSafeString<32>(ICON_LC_PLAY "###%u", sound->getId()).cstr()))
    {
        if (sound != sSelectedItem)
        {
            sSubSelectedItem = nullptr;
            sSelectedItemIsSubWindow = false;
        }

        PlaySound(sound);
    }

    ImGui::SameLine();

    return nullptr;
}

void DrawStreamSoundsUI()
{
    DrawAllItemsUI("Stream Sound", sBfsar.getSoundList(), nullptr, &SoundNamePrefixFunc2, nullptr,
        [](const Item* item)
        {
            const Sound* sound = static_cast<const Sound*>(item);
            return sound->getSoundType() == Sound::SoundType::Strm;
        }
    );
}

void DrawWaveSoundsUI()
{
    DrawAllItemsUI("Wave Sound", sBfsar.getSoundList(), nullptr, &SoundNamePrefixFunc2, nullptr,
        [](const Item* item)
        {
            const Sound* sound = static_cast<const Sound*>(item);
            return sound->getSoundType() == Sound::SoundType::Wave;
        }
    );
}

void DrawSequenceSoundsUI()
{
    DrawAllItemsUI("Sequence Sound", sBfsar.getSoundList(), nullptr, &SoundNamePrefixFunc2, nullptr,
        [](const Item* item)
        {
            const Sound* sound = static_cast<const Sound*>(item);
            return sound->getSoundType() == Sound::SoundType::Seq;
        }
    );
}

const char* SoundSetNamePrefixFunc(Item* item)
{
    SoundSet* sound = (SoundSet*)item;

    const char* icon = ICON_LC_FILE_QUESTION " ";
    switch (sound->getSoundSetType())
    {
        case SoundSet::SoundSetType::Wave:
            icon = ICON_LC_AUDIO_LINES " ";
            break;

        case SoundSet::SoundSetType::Seq:
            icon = ICON_LC_MUSIC_3 " ";
            break;
    }

    return icon;
}

InstanciateItemCallback CreateSoundSetFunc(bool clear)
{
    auto innerFunc = [](bool clear, Item* item, bool* validate)
    {
        static SoundSet::SoundSetType sSoundSetType = SoundSet::SoundSetType::Wave;

        if (clear)
        {
            sSoundSetType = SoundSet::SoundSetType::Wave;
        }

        if (!item && !validate)
        {
            ImGui::Separator();

            static const char* sSoundSetTypes[] = { "Wave", "Sequence" };
            ImGui::Combo("Sound Type", (s32*)&sSoundSetType, sSoundSetTypes, IM_ARRAYSIZE(sSoundSetTypes));
        }
        else if (validate)
        {
        }
        else
        {
            SoundSet* soundSet = static_cast<SoundSet*>(item);

            soundSet->setSoundSetType(sSoundSetType);
        }
    };

    return CreateItemFunc(clear, []() -> Item* { return new SoundSet(); }, innerFunc);
}

void DrawAllSoundSetsUI()
{
    DrawAllItemsUI("Sound Set", sBfsar.getSoundSetList(), &CreateSoundSetFunc, &SoundSetNamePrefixFunc);
}

void DrawWaveSoundSetsUI()
{
    DrawAllItemsUI("Wave Sound Set", sBfsar.getSoundSetList(), nullptr, nullptr, nullptr,
        [](const Item* item)
        {
            const SoundSet* soundSet = static_cast<const SoundSet*>(item);
            return soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave;
        }
    );
}

void DrawSequenceSoundSetsUI()
{
    DrawAllItemsUI("Sequence Sound Set", sBfsar.getSoundSetList(), nullptr, nullptr, nullptr,
        [](const Item* item)
        {
            const SoundSet* soundSet = static_cast<const SoundSet*>(item);
            return soundSet->getSoundSetType() == SoundSet::SoundSetType::Seq;
        }
    );
}

static const char* WaveFileNamePrefixFunc(Item* item)
{
    WaveFile* wave = static_cast<WaveFile*>(item);

    if (ImGui::Button(sead::FormatFixedSafeString<32>(ICON_LC_PLAY "###%u", wave->getId()).cstr()))
    {
        PlayWaveFile(*wave);
    }

    ImGui::SameLine();

    return nullptr;
}

static WaveFile::Encoding sEncoding = WaveFile::Encoding::DspAdpcm;
static WaveFile* sImportWaveFile = nullptr;
static sead::FixedSafeString<512> sWavFilePath;

void WaveFileContextMenuFunc(Item* item)
{
    WaveFile* wave = nullptr;
    if (item)
    {
        wave = static_cast<WaveFile*>(item);
    }

    ImGui::Separator();

    bool disableMenu = wave == nullptr;
    if (disableMenu)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::MenuItem("Replace"))
    {
        sImportWaveFile = nullptr;
        sWavFilePath.clear();

        const u32 filterCount = 1;
        FileFilter filters[filterCount] = {
            { "Wave (*.wav)", "*.wav" }
        };

        if (OpenFileDialog(&sWavFilePath, nullptr, filterCount, filters))
        {
            sEncoding = WaveFile::Encoding::DspAdpcm;
            sImportWaveFile = wave;
        }
    }

    if (ImGui::MenuItem("Export"))
    {
        sead::FixedSafeString<512> path;

        const u32 filterCount = 1;
        FileFilter filters[filterCount] = {
            { "Wave (*.wav)", "*.wav" }
        };

        if (SaveFileDialog(&path, nullptr, filterCount, filters, "wav"))
        {
            wave->writeWavFile(path);
        }
    }

    if (disableMenu)
    {
        ImGui::EndDisabled();
    }
}

void DrawWaveFilesUI()
{
    DrawAllItemsUI("Wave File", sBfsar.getWaveFileList(), nullptr, &WaveFileNamePrefixFunc, &WaveFileContextMenuFunc);

    if (sImportWaveFile && !sWavFilePath.isEmpty())
    {
        ImGui::OpenPopup("WavImport");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("WavImport", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::Text("Select the Encoding");

        if (ImGui::Combo("Encoding", (s32*)&sEncoding, WaveFile::sEncodingTypes, IM_ARRAYSIZE(WaveFile::sEncodingTypes)))
        {
        }

        ImGui::Separator();

        ImVec2 buttonSize((ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().WindowPadding.x * 2.0f) / 2.0f, 0.0f);

        if (ImGui::Button("Import", buttonSize))
        {
            bool success = sImportWaveFile->readWavFile(sWavFilePath, sEncoding);
            SEAD_PRINT("Wav Import: %d\n", success);

            sImportWaveFile = nullptr;
            sWavFilePath.clear();

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", buttonSize))
        {
            sImportWaveFile = nullptr;
            sWavFilePath.clear();

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

FileWindow* OpenFileWindow(Item* item)
{
    sead::FormatFixedSafeString<512> windowName("###%u", item);

    ImGuiWindow* imguiWindow = ImGui::FindWindowByName(windowName.cstr());
    if (!imguiWindow || !imguiWindow->WasActive)
    {
        FileWindow* window = new FileWindow(item);

        sFileWindows.pushBack(window);

        return window;
    }
    else if (imguiWindow)
    {
        ImGui::SetWindowFocus(windowName.cstr());

        return nullptr;
    }
}

const char* SequenceFileNamePrefixFunc(Item* item)
{
    SequenceFile* seq = static_cast<SequenceFile*>(item);

    if (ImGui::Button(sead::FormatFixedSafeString<32>(ICON_LC_FILE_PEN "###%u", seq->getId()).cstr()))
    {
        OpenFileWindow(seq);
    }

    ImGui::SameLine();

    return nullptr;
}

void DrawSequenceFilesUI()
{
    DrawAllItemsUI("Sequence File", sBfsar.getSequenceFileList(), nullptr, &SequenceFileNamePrefixFunc);
}

const char* BankFileNamePrefixFunc(Item* item)
{
    BankFile* bank = static_cast<BankFile*>(item);

    if (ImGui::Button(sead::FormatFixedSafeString<32>(ICON_LC_FILE_PEN "###%u", bank->getId()).cstr()))
    {
        OpenFileWindow(bank);
    }

    ImGui::SameLine();

    return nullptr;
}

void DrawBankFilesUI()
{
    DrawAllItemsUI("Bank File", sBfsar.getBankFileList(), nullptr, &BankFileNamePrefixFunc);
}

bool DrawVersionUI(u32* versionPtr, u32 versionByteNum)
{
    const ImU8 stepU8 = 1;

    //ImGui::Text("0x%08X", *versionPtr);

    bool updated = false;
    for (u32 i = 0; i < versionByteNum; i++)
    {
        ImGui::PushID(i);
        ImGui::PushItemWidth(ImGui::GetWindowContentRegionMax().x / static_cast<f32>(versionByteNum) - 7.0f);

        u32 version = *versionPtr;
        u32 versionByteOffset = ((versionByteNum - 1) - i) * 0x8;

        u8 versionByte = (version >> versionByteOffset) & 0xFF;
        if (ImGui::InputScalar("", ImGuiDataType_U8, &versionByte, &stepU8))
        {
            *versionPtr = (version & ~(0xFF << versionByteOffset)) | (versionByte << versionByteOffset);
            updated = true;
        }

        if (i + 1 < versionByteNum)
        {
            ImGui::SameLine();
        }

        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    return updated;
}

void CenteredText(const char* text, const ImVec2& sizeArg)
{
    ImVec2 textSize = ImGui::CalcTextSize(text);

    if (sizeArg.x != -1)
        ImGui::SetCursorPosX((sizeArg.x / 2.0f) - (textSize.x / 2.0f));

    if (sizeArg.y != -1)
        ImGui::SetCursorPosY((sizeArg.y / 2.0f) - (textSize.y / 2.0f));

    ImGui::Text(text);
}

// From ImGui Demo
void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
