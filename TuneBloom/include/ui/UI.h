#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <icons/IconsLucide.h>

#include <bfsar/Bfsar.h>

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

extern const nw::snd::MemorySoundArchive* sSoundArchive;

extern bool* sSoundInGroup;

extern const SoundDataMgr* sSoundDataMgr;

extern const nw::snd::internal::SoundArchiveFile::SoundInfo* sCurrentSoundInfo;

extern u32 sSampleRate;
extern u32 sSampleCount;

extern u32 sStreamSoundCount;
extern u32 sWaveSoundCount;
extern u32 sSequenceSoundCount;

extern u32 sWaveSoundSetCount;
extern u32 sSequenceSoundSetCount;



//
extern Item* sSelectedItem;

class FileWindow : public Item
{
public:
    FileWindow(Item* file)
        : mOpen(true)
        , mFileRef(this)
    {
        mFileRef.attach(file);
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

void DrawMenuBar();
void DrawUI();

void OpenFile();
void SaveFile();
void SaveFileAs();
void CloseFile();
void LoadBfgrp();

void PlaySound(const nw::snd::internal::SoundArchiveFile::SoundInfo* soundInfo);
void PlaySound(const Sound* sound);
void PlayWaveFile(const WaveFile& wave, s32 channel = -1, const Sound* sound = nullptr);
void PlayBankNote(u8 key, u8 velocity, const BankFile::VelocityRegion& velocityRegion);
void StopAllSoundPlayers(bool stop = false);
void StopAllSoundPlayersWithoutLock(bool stop = false);

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

void WarningPopup(const char* name, const char* content);

inline void DupeNamePopup(const char* name = "###Dupe")
{
    WarningPopup(name, "An item with this name already exists.\nPlease use another one.");
}

InstanciateItemCallback CreateItemFunc(bool clear, InstanciateItemCallback instanciateItemCallback, ItemPropertiesCallback itemPropertiesCallback);
void DrawAllItemsUI(const char* listName, Item::List& list, CreateItemCallback createCallback = nullptr, ItemNamePrefixCallback nameCallback = nullptr, ContextMenuCallback menuCallback = nullptr, ItemFilterCallback filterCallback = nullptr);
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
