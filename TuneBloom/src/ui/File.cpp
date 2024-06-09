#include <ui/UI.h>

#include <snd/SoundThread.h>

#include <basis/win/seadWindows.h>
#include <filedevice/seadFileDeviceMgr.h>
#include <filedevice/seadPath.h>
#include <framework/win/seadGameFrameworkBaseWin.h>
#include <stream/seadFileDeviceStream.h>

#include <Utilll.h>

#include <commdlg.h>

#include <string>

bool OpenFileDialog(sead::BufferedSafeString* outPath, const char* title, u32 filterCount, FileFilter* filters)
{
    SEAD_ASSERT(outPath);

    std::string filtersStr;

    filtersStr.append("All Files (*.*)");
    filtersStr.append(1, '\0');
    filtersStr.append("*.*");
    filtersStr.append(1, '\0');

    if (filterCount > 0)
    {
        SEAD_ASSERT(filters);

        for (u32 i = 0; i < filterCount; i++)
        {
            filtersStr.append(filters[i].name);
            filtersStr.append(1, '\0');
            filtersStr.append(filters[i].filter);
            filtersStr.append(1, '\0');
        }
    }

    OPENFILENAMEA ofn;
    sead::MemUtil::fillZero(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = outPath->getBuffer();
    ofn.nMaxFile = outPath->getBufferSize();
    ofn.lpstrFilter = filtersStr.c_str();
    ofn.nFilterIndex = filterCount > 0 ? 2 : 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    ofn.lpstrTitle = title;

    sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(util::getFramework());
    if (fw)
    {
        ofn.hwndOwner = fw->getWindowHandle();
    }

    return GetOpenFileNameA(&ofn);
}

bool SaveFileDialog(sead::BufferedSafeString* outPath, const char* title, u32 filterCount, FileFilter* filters, const char* defaultExt)
{
    SEAD_ASSERT(outPath);

    std::string filtersStr;

    if (filterCount > 0)
    {
        SEAD_ASSERT(filters);

        for (u32 i = 0; i < filterCount; i++)
        {
            filtersStr.append(filters[i].name);
            filtersStr.append(1, '\0');
            filtersStr.append(filters[i].filter);
            filtersStr.append(1, '\0');
        }
    }

    OPENFILENAMEA ofn;
    sead::MemUtil::fillZero(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = outPath->getBuffer();
    ofn.nMaxFile = outPath->getBufferSize();
    ofn.lpstrFilter = filtersStr.c_str();
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defaultExt;

    sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(util::getFramework());
    if (fw)
    {
        ofn.hwndOwner = fw->getWindowHandle();
    }

    return GetSaveFileNameA(&ofn);
}

void InnerFile::drawUI()
{
    {
        static const char* sEndianTypes[] = {
            "Big Endian",
            "Little Endian"
        };

        sead::Endian::Types endian = mEndian;
        if (ImGui::Combo("Byte Order", (s32*)&endian, sEndianTypes, IM_ARRAYSIZE(sEndianTypes)))
        {
            mEndian = endian;
        }
    }

    {
        CenteredTextX("Version");

        u32 version = mVersion;
        if (DrawVersionUI(&version))
        {
            mVersion = version;
        }
    }
}

Bfsar sBfsar;

const SoundDataMgr* sSoundDataMgr = nullptr;
const nw::snd::MemorySoundArchive* sSoundArchive = nullptr;

bool* sSoundInGroup = nullptr;

u32 sStreamSoundCount = 0;
u32 sWaveSoundCount = 0;
u32 sSequenceSoundCount = 0;

u32 sWaveSoundSetCount = 0;
u32 sSequenceSoundSetCount = 0;

Item* sSelectedItem = nullptr;

void OpenFile()
{
    sead::FixedSafeString<512> filePath;

    const u32 filterCount = 1;
    FileFilter filters[filterCount] = {
        { "Sound Archive (*.bfsar)", "*.bfsar" }
    };

    if (!OpenFileDialog(&filePath, nullptr, filterCount, filters))
    {
        return;
    }

    CloseFile();

    if (!sBfsar.open(filePath, nullptr))
    {
        SEAD_PRINT("Not a valid bfsar file\n");
        return;
    }

    sSoundArchive = sBfsar.getSoundArchive();
    sSoundDataMgr = &sBfsar.getSoundDataMgr();

    sead::FixedSafeString<512> fileName;
    sead::Path::getFileName(&fileName, filePath);

    sead::FormatFixedSafeString<512> title("%s - %s", util::cAppName.cstr(), fileName.cstr());

    sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(util::getFramework());
    SEAD_ASSERT(fw);

    fw->setCaption(title);

    sSoundInGroup = new bool[sSoundArchive->GetSoundCount()];
    for (u32 i = 0; i < sSoundArchive->GetSoundCount(); i++)
    {
        sSoundInGroup[i] = false;

        u32 soundId = sSoundArchive->GetSoundIdFromIndex(i);
        const nw::snd::internal::SoundArchiveFile::SoundInfo* soundInfo = sSoundArchive->GetSoundInfo(soundId);
        if (!soundInfo)
            continue;

        if (soundInfo->GetSoundType() == nw::snd::SoundArchive::SOUND_TYPE_STRM)
            sStreamSoundCount++;
        else if (soundInfo->GetSoundType() == nw::snd::SoundArchive::SOUND_TYPE_WAVE)
            sWaveSoundCount++;

        for (u32 groupNo = 0; groupNo < sSoundArchive->GetSoundGroupCount(); groupNo++)
        {
            const nw::snd::internal::SoundArchiveFile::SoundGroupInfo* groupInfo = sSoundArchive->detail_GetSoundGroupInfo(sSoundArchive->GetSoundGroupIdFromIndex(groupNo));
            if (!groupInfo)
                continue;

            if (groupInfo->startId <= soundId && soundId <= groupInfo->endId)
            {
                sSoundInGroup[i] = true;
                break;
            }
        }

        if (!sSoundInGroup[i] && soundInfo->GetSoundType() == nw::snd::SoundArchive::SOUND_TYPE_SEQ)
            sSequenceSoundCount++;
    }

    for (u32 groupNo = 0; groupNo < sSoundArchive->GetSoundGroupCount(); groupNo++)
    {
        const nw::snd::internal::SoundArchiveFile::SoundGroupInfo* groupInfo = sSoundArchive->detail_GetSoundGroupInfo(sSoundArchive->GetSoundGroupIdFromIndex(groupNo));
        if (!groupInfo)
            continue;

        if (groupInfo->GetWaveSoundGroupInfo())
            sWaveSoundSetCount++;
        else
            sSequenceSoundSetCount++;
    }
}

void SaveFile()
{
    if (sBfsar.isOpen())
    {
        sBfsar.save();
    }
}

void SaveFileAs()
{
    if (!sBfsar.isOpen())
    {
        return;
    }

    sead::FixedSafeString<512> path;

    const u32 filterCount = 1;
    FileFilter filters[filterCount] = {
        { "Sound Archive (*.bfsar)", "*.bfsar" }
    };

    if (SaveFileDialog(&path, nullptr, filterCount, filters, "bfsar"))
    {
        if (sBfsar.saveAs(path))
        {
            sead::FixedSafeString<512> fileName;
            sead::Path::getFileName(&fileName, path);

            sead::FormatFixedSafeString<512> title("%s - %s", util::cAppName.cstr(), fileName.cstr());

            sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(util::getFramework());
            SEAD_ASSERT(fw);

            fw->setCaption(title);
        }
    }
}

void CloseFile()
{
    sSelectedItem = nullptr;
    sFileWindows.clear();

    sStreamSoundCount = 0;
    sWaveSoundCount = 0;
    sSequenceSoundCount = 0;

    sWaveSoundSetCount = 0;
    sSequenceSoundSetCount = 0;

    sSampleRate = 0;
    sSampleCount = 0;

    sCurrentSoundInfo = nullptr;

    StopAllSoundPlayers(true);

    if (sSoundInGroup)
    {
        delete sSoundInGroup;
        sSoundInGroup = nullptr;
    }

    sBfsar.close();

    sSoundArchive = nullptr;

    sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(util::getFramework());
    if (fw)
    {
        fw->setCaption(util::cAppName);
    }
}

void OpenGroupFile(const char* file)
{
    if (!sSoundArchive)
        return;

    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
    SEAD_ASSERT(device);

    sead::FileDevice::LoadArg arg;
    //arg.path = "native://" + file;
    arg.path = file;

    u8* groupFile = device->load(arg);

    sBfsar.getSoundDataMgr().SetFileAddressInGroupFile(groupFile);
}

void LoadBfgrp()
{
    if (sSoundArchive)
    {
        sead::FixedSafeString<512> filePath;

        const u32 filterCount = 1;
        FileFilter filters[filterCount] = {
            { "Group File (*.bfgrp)", "*.bfgrp" }
        };

        if (OpenFileDialog(&filePath, nullptr, filterCount, filters))
        {
            OpenGroupFile(filePath.cstr());
        }
    }
}
