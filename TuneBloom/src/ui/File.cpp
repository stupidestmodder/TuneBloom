#include <ui/UI.h>

#include <ui/PopupMgr.h>

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

//* https://gist.github.com/danzek/d7192d250c951804dec05125f5223a30
bool CreateDirectoryRecursively(const std::string& directory)
{
    const std::string cSeparators("\\/");

    // If the specified directory name doesn't exist, do our thing
    DWORD fileAttributes = ::GetFileAttributesA(directory.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES)
    {
        // Recursively do it all again for the parent directory, if any
        size_t slashIndex = directory.find_last_of(cSeparators);
        if (slashIndex != std::string::npos)
        {
            CreateDirectoryRecursively(directory.substr(0, slashIndex));
        }

        // Create the last directory on the path (the recursive calls will have taken
        // care of the parent directories by now)
        BOOL result = ::CreateDirectoryA(directory.c_str(), nullptr);
        if (result == FALSE)
        {
            //throw std::runtime_error("Could not create directory");
            return false;
        }
    }
    else
    {
        // Specified directory name already exists as a file or directory
        bool isDirectoryOrJunction = ((fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) || ((fileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0);

        if (!isDirectoryOrJunction)
        {
            //throw std::runtime_error("Could not create directory because a file with the same name exists");
            return false;
        }
    }

    return true;
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

//const nw::snd::MemorySoundArchive* sSoundArchive = nullptr;
//const SoundDataMgr* sSoundDataMgr = nullptr;

Item* sSelectedItem = nullptr;
Item* sSubSelectedItem = nullptr;

void NewFile()
{
    PopupMgr::instance()->addPopup({ "TODO", nullptr });
}

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
        PopupMgr::instance()->addPopup({ "Selected file is not a valid BFSAR file", nullptr });
        //SEAD_PRINT("Not a valid bfsar file\n");
        return;
    }

    //sSoundArchive = sBfsar.getSoundArchive();
    //sSoundDataMgr = &sBfsar.getSoundDataMgr();

    sead::FixedSafeString<512> fileName;
    sead::Path::getFileName(&fileName, filePath);

    sead::FormatFixedSafeString<512> title("%s - %s", util::cAppName.cstr(), fileName.cstr());

    sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(util::getFramework());
    SEAD_ASSERT(fw);

    fw->setCaption(title);
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
    sSubSelectedItem = nullptr;
    sSelectedItemIsSubWindow = false;
    sFileWindows.clear();

    sLastPlayedSound = nullptr;

    sSampleRate = 0;
    sSampleCount = 0;

    StopAllSoundPlayers(true);

    sBfsar.close();

    //sSoundArchive = nullptr;

    sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(util::getFramework());
    if (fw)
    {
        fw->setCaption(util::cAppName);
    }
}
