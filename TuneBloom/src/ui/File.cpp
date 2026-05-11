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

Item* sSelectedItem = nullptr;
Item* sSubSelectedItem = nullptr;

bool ValidBFSARHeader(const void* file)
{
    // if (sead::MemUtil::compare(magic, "CSAR", 4) != 0)
    if (sead::MemUtil::compare(file, "FSAR", 4) != 0)
    {
        PopupMgr::instance()->addPopup({ "Selected file is not a valid BFSAR file", nullptr });
        return false;
    }

    const nw::snd::internal::SoundArchiveFile::FileHeader& header = *reinterpret_cast<const nw::snd::internal::SoundArchiveFile::FileHeader*>(file);

    //? Setup global file endian
    {
        const void* byteOrder = sead::PtrUtil::addOffset(&header, offsetof(nw::ut::BinaryFileHeader, byteOrder));
        sFileEndian = sead::Endian::markToEndian(*(u16*)byteOrder);
    }

    // if (false)
    if (!(0x00010000 <= header.version && header.version <= 0x00020200))
    {
        sead::FormatFixedSafeString<64> msg("BFSAR version not supported (0x%08X)", (u32)header.version);
        PopupMgr::instance()->addPopup({ msg, nullptr });
        return false;
    }

    return true;
}

bool NewFile()
{
    CloseFile();

    sBfsar.create();

    util::updateTitle("*New.bfsar");

    return true;
}

bool OpenFile()
{
    sead::FixedSafeString<512> filePath;

    if (!sDroppedFilePath.isEmpty())
    {
        filePath = sDroppedFilePath;
        sDroppedFilePath.clear();
    }
    else
    {
        const u32 filterCount = 1;
        FileFilter filters[filterCount] = {
            { "Sound Archive (*.bfsar)", "*.bfsar" }
        };

        if (!OpenFileDialog(&filePath, nullptr, filterCount, filters))
        {
            return false;
        }
    }

    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
    SEAD_ASSERT(device);

    sead::FileDevice::LoadArg arg;
    arg.path = filePath;

    u8* bfsarFile = device->tryLoad(arg);
    if (!bfsarFile)
    {
        PopupMgr::instance()->addPopup({ "Couldn't open the selected file" });
        return false;
    }

    if (!ValidBFSARHeader(bfsarFile))
    {
        return false;
    }

    CloseFile();

    if (!sBfsar.open(bfsarFile, filePath, nullptr)) //? bfsarFile is freed here
    {
        sead::FormatFixedSafeString<1024> msg(
            "Your BFSAR file is corrupted beyond repair :(\n%s", PopupMgr::instance()->getCorruptInfo().cstr()
        );
        PopupMgr::instance()->addPopup({ msg, nullptr });
        CloseFile();
        return false;
    }

    //sSoundArchive = sBfsar.getSoundArchive();

    sead::FixedSafeString<512> fileName;
    sead::Path::getFileName(&fileName, filePath);

    util::updateTitle(fileName.cstr());

    return true;
}

bool SaveFile()
{
    if (sBfsar.isOpen())
    {
        return sBfsar.save();
    }

    return false;
}

bool SaveFileAs()
{
    if (!sBfsar.isOpen())
    {
        return false;
    }

    if (!sBfsar.validate_())
    {
        return false;
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

            util::updateTitle(fileName.cstr());

            return true;
        }
    }

    return false;
}

bool CloseFile()
{
    PopupMgr::instance()->closeFile();

    sSelectedItem = nullptr;
    sSubSelectedItem = nullptr;
    sSelectedItemIsSubWindow = false;
    sFileWindows.clear();
    CloseFilter();

    sSoundPlayer.reset();

    sBfsar.close();

    util::updateTitle(nullptr);

    return true;
}

bool Exit()
{
    CloseFile();

    sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(util::getFramework());
    fw->requestExit();

    return true;
}

bool CheckBlockCorrupt(const char* fileName, const char* blockName, const void* block)
{
    if (sead::MemUtil::compare(block, blockName, 4) != 0)
    {
        PopupMgr::instance()->setCorruptInfo(sead::FormatFixedSafeString<64>("%s: %s block not found", fileName, blockName));
        return false;
    }

    return true;
}

bool CheckBlockCorruptError(const char* fileName, const char* blockName, const void* block)
{
    if (sead::MemUtil::compare(block, blockName, 4) != 0)
    {
        PopupMgr::instance()->pushCurrentItemError(sead::FormatFixedSafeString<64>("%s: %s block not found", fileName, blockName));
        return false;
    }

    return true;
}
