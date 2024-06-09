#include <bfsar/File.h>

#include <snd/SoundThread.h>

#include <snd/snd_WaveArchiveFileReader.h>

#include <ui/UI.h>

Item* CreateWaveFileFunc()
{
    // TODO
    return nullptr;
}

const char* WaveFileNamePrefixFunc(Item* item)
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

void BfwarFile::drawUI()
{
    InnerFile::drawUI();

    ImGui::SeparatorText(sead::FormatFixedSafeString<16>("Waves (%d)", mWaveList.size()).cstr());

    if (ImGui::BeginChild("ChildWaves", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border))
    {
        DrawAllItemsUI("Wave", mWaveList, nullptr, &WaveFileNamePrefixFunc, &WaveFileContextMenuFunc);
    }
    ImGui::EndChild();

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

void BfwarFile::doRead(const void* fileAddr)
{
    nw::snd::internal::WaveArchiveFileReader reader(fileAddr);

    for (u32 i = 0; i < reader.GetWaveFileCount(); i++)
    {
        WaveFile* wave = new WaveFile();
        wave->mId = i;

        wave->mEnableName = true;
        wave->mName = "Wave";

        wave->read(reader.GetWaveFile(i));

        mWaveList.pushBack(wave);
    }
}

u32 BfwarFile::doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const
{
    FileWriter writer(handle, stream);
    writer.openFile("FWAR", nw::snd::internal::WaveArchiveFile::BLOCK_SIZE, mVersion);

    //? Info Block
    {
        writer.openBlock(nw::snd::internal::ElementType_WaveArchiveFile_InfoBlock, "INFO");

        writer.openSizedReferenceTable("WaveTable", mWaveList.size());

        writer.align(0x20);
        writer.closeBlock();
    }

    //? File Block
    {
        writer.openBlock(nw::snd::internal::ElementType_WaveArchiveFile_FileBlock, "FILE");

        writer.align(0x20);

        for (const Item* item : mWaveList)
        {
            SEAD_ASSERT(item->getItemType() == Item::ItemType::WaveFile);
            const WaveFile* wave = static_cast<const WaveFile*>(item);

            writer.align(0x20);

            writer.addSizedReferenceTableReference("WaveTable", nw::snd::internal::ElementType_General_ByteStream, 0);

            u32 pos = writer.getPosition();
            {
                wave->write(handle, stream, mEndian, wave->getId() == mWaveList.size() - 1);
            }
            u32 size = writer.getPosition() - pos;

            writer.setSizedReferenceTableReferenceSize("WaveTable", size);
        }

        writer.closeSizedReferenceTable("WaveTable");

        writer.closeBlock();
    }

    u32 fileSize = writer.getPosition();

    writer.closeFile();

    return fileSize;
}
