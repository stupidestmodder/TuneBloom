#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <icons/IconsLucide.h>

#include <bfsar/Bfsar.h>
#include <bfsar/IdTable.h>

#include <bfsar/SoundPlayer.h>

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

void SetUITab(UIType type);
void FocusInfoWindow();
void FocusPropertiesWindow();

struct FileFilter
{
    FileFilter(const char* name_, const char* filter_)
        : name(name_)
        , filter(filter_)
    {
    }

    const char* name;
    const char* filter;
};

bool OpenFileDialog(sead::BufferedSafeString* outPath, const char* title = nullptr, u32 filterCount = 0, FileFilter* filters = nullptr);
bool SaveFileDialog(sead::BufferedSafeString* outPath, const char* title = nullptr, u32 filterCount = 0, FileFilter* filters = nullptr, const char* defaultExt = nullptr);

extern Bfsar sBfsar;
extern SoundPlayer sSoundPlayer;

extern sead::FixedSafeString<260> sDroppedFilePath;
extern bool sFileDroppedThisFrame;

//

void SelectItem(Item* item);

extern Item* sSelectedItem;
extern Item* sSubSelectedItem;

extern bool sSelectedItemIsSubWindow;

class FileWindow : public Item
{
public:
    FileWindow(Item* file)
        : mOpen(true)
        , mFileRef(this)
    {
        mFileRef.attach(file);

        file->onOpenFileWindow();
    }

    ~FileWindow() override
    {
        if (mFileRef.isAttached())
        {
            mFileRef.getItem()->onCloseFileWindow();
        }
    }

    bool isOpen() const
    {
        return mOpen;
    }

    bool* getOpenPtr()
    {
        return &mOpen;
    }

    const ItemReference& getFileRef() const
    {
        return mFileRef;
    }

    ItemReference& getFileRef()
    {
        return mFileRef;
    }

private:
    bool mOpen;
    ItemReference mFileRef;
};

extern ItemList sFileWindows;

FileWindow* OpenFileWindow(Item* item);

void DrawMenuBar();
void DrawUI();

bool NewFile();
bool OpenFile();
bool SaveFile();
bool SaveFileAs();
bool CloseFile();
bool Exit();

bool CheckBlockCorrupt(const char* fileName, const char* blockName, const void* block);
bool CheckBlockCorruptError(const char* fileName, const char* blockName, const void* block);

// Ui Types
void DrawProjectInfoUI();
void DrawBanksUI();
void DrawWaveArchivesUI();
void DrawGroupsUI();
void DrawPlayersUI();

void DrawPlayerUI();

//
using ItemPropertiesCallback = void (*)(bool clear, Item* item, bool* validate);
using InstanciateItemCallback = Item* (*)();
using CreateItemCallback = InstanciateItemCallback (*)(bool clear);
using ItemNamePrefixCallback = const char* (*)(Item* item);
using ContextMenuCallback = void (*)(Item* item);
using ItemFilterCallback = bool (*)(const Item* item);

void CloseFilter();
bool ItemMatchesFilter(const Item* item);
ItemFilterCallback GetItemFilterCallback();

void WarningPopup(const char* name, const char* content);

inline void DupeNamePopup(const char* name = "###Dupe")
{
    WarningPopup(name, "An item with this name already exists.\nPlease use another one.");
}

InstanciateItemCallback CreateItemFunc(bool clear, InstanciateItemCallback instanciateItemCallback, ItemPropertiesCallback itemPropertiesCallback);
void DrawAllItemsUI(const char* listName, Item::List& list, CreateItemCallback createCallback = nullptr, ItemNamePrefixCallback nameCallback = nullptr, ContextMenuCallback menuCallback = nullptr, ItemFilterCallback filterCallback = nullptr, bool disableAddWindow = false);
void DrawItemPropertiesUI();
bool ItemSelector(const char* name, const Item::List& list, Item** item, bool allowNone = false);
bool WaveArchiveSelector(const char* name, WaveArchiveType* warcType, Item** warc, const Item::List& warcList);
void ItemIdTable(const char* name, IdTable& table, const Item::List& itemList);

void DrawSoundPropertiesUI();
void DrawSoundSetPropertiesUI();
void DrawBankPropertiesUI();
void DrawWaveArchivePropertiesUI();
void DrawGroupPropertiesUI();
void DrawPlayerPropertiesUI();

bool DrawVersionUI(u32* versionPtr, u32 versionByteNum = 3);

void CenteredText(const char* text, const ImVec2& sizeArg = ImGui::GetWindowSize());

inline void CenteredTextX(const char* text, f32 sizeX = ImGui::GetWindowSize().x)
{
    CenteredText(text, ImVec2(sizeX, -1.0f));
}

void HelpMarker(const char* desc);

//? Keyboard

/*
The MIT License (MIT)
by https://github.com/frowrik
Piano keyboard for ImGui v1.1
example:
static int PrevNoteActive = -1;
ImGui_PianoKeyboard("PianoTest", ImVec2(1024, 100), &PrevNoteActive, 21, 108, TestPianoBoardFunct, nullptr, nullptr);
bool TestPianoBoardFunct(void* UserData, int Msg, int Key, float Vel) {
		if (Key >= 128) return false; // midi max keys
		if (Msg == NoteGetStatus) return KeyPresed[Key];
		if (Msg == NoteOn) { KeyPresed[Key] = true; Send_Midi_NoteOn(Key, Vel*127); }
		if (Msg == NoteOff) { KeyPresed[Key] = false; Send_Midi_NoteOff(Key, Vel*127);}
		return false;
}
*/

enum ImGuiPianoKeyboardMsg
{
    NoteGetStatus,
    NoteOn,
    NoteOff,
};

using ImGuiPianoKeyboardProc = bool (*)(void* UserData, s32 Msg, s32 Key, f32 Vel);

struct ImGuiPianoStyles
{
    ImU32 Colors[9] {
        IM_COL32(255, 255, 255, 255), // light note
        IM_COL32(0, 0, 0, 255),       // dark note
        IM_COL32(255, 255, 0, 255),   // active light note
        IM_COL32(200, 200, 0, 255),   // active dark note
        IM_COL32(75, 75, 75, 255),    // background
        IM_COL32(26, 210, 77, 255),   // original light note
        IM_COL32(26, 161, 62, 255),   // original dark note
        IM_COL32(59, 237, 107, 255),  // original active light note
        IM_COL32(31, 207, 78, 255),   // original active dark note
    };

    f32 NoteDarkHeight = 2.0f / 3.0f; // dark note scale h
    f32 NoteDarkWidth  = 2.0f / 3.0f; // dark note scale w
};

void ImGui_PianoKeyboard(const char* IDName, ImVec2 Size, s32* PrevNoteActive, s32 BeginOctaveNote, s32 EndOctaveNote, ImGuiPianoKeyboardProc Callback, void* UserData, ImGuiPianoStyles* Style = nullptr, s32 OriginalKey = -1);
