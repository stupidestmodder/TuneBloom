#include <bfsar/SequenceFile.h>

#include <bfsar/SeqCommand.h>

#include <snd/snd_SequenceSoundFileReader.h>
#include <snd/DisposeCallbackMgr.h>

#include <ui/UI.h>
#include <imgui/imgui.h>
#include <imgui/imgui_custom.h>
#include <TextEditor.h>

#include <vector>
#include <string>
#include <unordered_map>

extern bool ParseSequenceFile(std::vector<std::string>* outLines, std::unordered_map<u32, u32>* offsetToLine, const void* seqFile);

SequenceFile::~SequenceFile()
{
    invalidatePlayer_();

    if (mTextEditor)
    {
        delete mTextEditor;
        mTextEditor = nullptr;
    }

    if (mSeqText)
    {
        delete mSeqText;
        mSeqText = nullptr;
    }

    if (mSeqBytes)
    {
        delete mSeqBytes;
        mSeqBytes = nullptr;
        mSeqBytesSize = 0;
    }

    for (u32 i = 0; i < 4; i++)
    {
        delete mBankRefs[i];
        mBankRefs[i] = nullptr;
    }
}

void SequenceFile::onOpenFileWindow()
{
    SEAD_ASSERT(!mTextEditor);
    mTextEditor = new TextEditor();

    mIsDirty = false;

    if (mSeqText)
    {
        mTextEditor->SetText(mSeqText->cstr());
    }
}

void SequenceFile::onCloseFileWindow()
{
    mSeqTextInfo.reset();

    if (mTextEditor)
    {
        if (mIsValid)
        {
            if (mSeqText)
            {
                delete mSeqText;
                mSeqText = nullptr;
            }

            std::string text = mTextEditor->GetText();
            text.pop_back(); // Remove new line at the end

            mSeqText = new sead::HeapSafeString(nullptr, text.c_str());
        }

        delete mTextEditor;
        mTextEditor = nullptr;
    }
}

const Item* SequenceFile::validate(sead::BufferedSafeString& error) const
{
    if (!isValid())
    {
        error = "Sequence File was not successfully compiled";
        return this;
    }

    if (!getSeqBytes())
    {
        error = "Sequence File was not successfully compiled (Missing bytes ?)";
        return this;
    }

    return nullptr;
}

void SequenceFile::drawUI()
{
    mVersion = sBfsar.getVersionForBfseq();
    mEndian = sBfsar.getEndian();

    HelpMarker("Those are derived from the BFSAR");

    ImGui::BeginDisabled();
    InnerFile::drawUI();
    ImGui::EndDisabled();
}

void SequenceFile::drawFileUI()
{
    SEAD_ASSERT(mTextEditor);

    static bool sAutoCompile = false;

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Checkbox("Auto Compile", &sAutoCompile);

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
    {
        Item* bank = mBankRefs[i]->getItem();
        if (ItemSelector(sead::FormatFixedSafeString<16>("Bank %u", i).cstr(), sBfsar.getBankList(), &bank, true))
        {
            mBankRefs[i]->attach(bank);
        }

        Item* bankFile = nullptr;
        if (bank)
        {
            Bank* bankItem = static_cast<Bank*>(bank);
            bankFile = bankItem->getFileRef().getItem();
        }

        if (!bankFile)
        {
            ImGui::BeginDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button(sead::FormatFixedSafeString<32>(ICON_LC_FILE_PEN "###EditBank%u", i).cstr()))
        {
            OpenFileWindow(bankFile);
        }

        if (!bankFile)
        {
            ImGui::EndDisabled();
        }

        if (bank && !bankFile && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNone))
        {
            ImGui::SetTooltip("Bank has no file attached");
        }
    }

    const char** labels = nullptr;
    u32 labelCount = mLabels.size();
    if (labelCount > 0)
    {
        labels = new const char*[labelCount];
        for (u32 i = 0; i < labelCount; i++)
        {
            labels[i] = mLabels[i].c_str();
        }
    }

    ImGui::InputTextCombo("Start Label", mStartLabel.getBuffer(), mStartLabel.getBufferSize(), labels, labelCount);

    delete[] labels;

    if (mLoadError)
    {
        mLoadError = false;
        compile_();
    }

    if (ImGui::Button(ICON_LC_PLAY))
    {
        mSeqTextInfo.reset();

        if (mIsDirty)
        {
            compile_();
        }

        if (mIsValid)
        {
            const Bank* banks[nw::snd::SoundArchive::SEQ_BANK_MAX];
            for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
            {
                banks[i] = nullptr;
            }

            for (u32 i = 0; i < nw::snd::SoundArchive::SEQ_BANK_MAX; i++)
            {
                const Item* item = mBankRefs[i]->getItem();
                if (!item)
                {
                    continue;
                }

                SEAD_ASSERT(item->getItemType() == Item::ItemType::Bank);
                const Bank* bank = static_cast<const Bank*>(item);

                banks[i] = bank;
            }

            sSoundPlayer.playSeqFile(*this, mStartLabel, banks, 127);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button(ICON_LC_FILE_COG))
    {
        if (mIsDirty)
        {
            compile_();
        }
    }

    static bool sShowTrail = true;

    if (mIsValid && sSoundPlayer.getSequencePlayer().isPlayingFile(*this))
    {
        mSeqTextInfo.update(sSoundPlayer.getSequencePlayer(), *mTextEditor, mOffsetToLine, sShowTrail);
    }

    //if (false)
    {
        ImGui::SameLine();

        static bool sFollowSeq = true;
        static u32 sFollowTrack = 0;

        bool force = ImGui::Checkbox("Follow", &sFollowSeq);

        ImGui::SameLine();

        static const char* sTrackNames[] = {
            "Track 0",
            "Track 1",
            "Track 2",
            "Track 3",
            "Track 4",
            "Track 5",
            "Track 6",
            "Track 7",
            "Track 8",
            "Track 9",
            "Track 10",
            "Track 11",
            "Track 12",
            "Track 13",
            "Track 14",
            "Track 15"
        };

        ImGui::Combo("Track", (s32*)&sFollowTrack, sTrackNames, IM_ARRAYSIZE(sTrackNames));

        if (sFollowSeq && sSoundPlayer.isCurrentPlayerSequence() && sSoundPlayer.getSequencePlayer().isPlayingFile(*this))
        {
            const SeqTextInfo::Track& track = mSeqTextInfo.mTracks[sFollowTrack];
            if (track.active && track.line > 0 && (track.updated || !sSoundPlayer.isPause() || force))
            {
                mTextEditor->SetCursorPosition({ track.line - 1, 0 });
            }
        }

        ImGui::SameLine();

        ImGui::Checkbox("Trail", &sShowTrail);
    }

    mTextEditor->Render("SeqText");

    if (mTextEditor->IsTextChanged())
    {
        mSeqTextInfo.reset();
        mTextEditor->SetMarkers({ });

        mIsValid = false;
        mIsDirty = true;

        if (sSoundPlayer.getSequencePlayer().isPlayingFile(*this))
        {
            sSoundPlayer.stopAllPlayers(false);
        }

        if (sAutoCompile)
        {
            compile_(false);
        }
    }
}

u32 SequenceFile::getLabelOffset(const sead::SafeString& label, bool parsed) const
{
    if (!mIsValid)
    {
        return cInvaldOffset;
    }

    auto it = mLabelsStartInfo.find(label.cstr());
    if (it == mLabelsStartInfo.end())
    {
        return cInvaldOffset;
    }

    return parsed ? it->second.parsedOffset : it->second.offset;
}

u32 SequenceFile::getLabelAllocTracks(const sead::SafeString& label) const
{
    if (!mIsValid)
    {
        return 0;
    }

    auto it = mLabelsStartInfo.find(label.cstr());
    if (it == mLabelsStartInfo.end())
    {
        return 0;
    }

    return it->second.allocTrackFlags;
}

std::string SequenceFile::getLabelFromParsedOffset(u32 offset, u32 allocTrackFlags) const
{
    for (auto pair : mLabelsStartInfo)
    {
        const std::string& label = pair.first;
        if (label.empty())
        {
            continue;
        }

        const LabelStartInfo& startInfo = pair.second;

        if (offset == startInfo.parsedOffset && allocTrackFlags == startInfo.allocTrackFlags)
        {
            return pair.first;
        }
    }

    return "";
}

void SequenceFile::setCursorToLabel_(const sead::SafeString& label)
{
    if (!mIsValid || !mTextEditor)
    {
        return;
    }

    auto it = mLabelsStartInfo.find(label.cstr());
    if (it == mLabelsStartInfo.end())
    {
        return;
    }

    u32 offset = it->second.offset;

    auto lineIt = mOffsetToLine.find(offset);
    if (lineIt == mOffsetToLine.end())
    {
        return;
    }

    u32 line = lineIt->second;
    mTextEditor->SetCursorPosition({ s32(line - 1), 0 });
}

void SequenceFile::invalidatePlayer_() const
{
    if (sSoundPlayer.getSequencePlayer().isPlayingFile(*this))
    {
        sSoundPlayer.getSequencePlayer().clearPlayingFile();
    }

    if (mSeqBytes)
    {
        snd::internal::driver::SoundThreadLock lock;

        snd::internal::driver::DisposeCallbackMgr::instance()->dispose(mSeqBytes, mSeqBytesSize);
    }
}

bool SequenceFile::doRead(const void* fileAddr)
{
    std::vector<std::string> lines;
    std::unordered_map<u32, u32> offsetToLine;
    if (!ParseSequenceFile(&lines, &offsetToLine, fileAddr))
    {
        return false;
    }

    std::string fseq;
    for (const auto& line : lines)
    {
        fseq += line + '\n';
    }

    mSeqText = new sead::HeapSafeString(nullptr, fseq.c_str());

    nw::snd::internal::SequenceSoundFileReader reader(fileAddr);
    if (!reader.IsAvailable())
    {
        return false;
    }

    mSeqBytesSize = reader.mHeader->GetDataBlock()->header.size - sizeof(reader.mHeader->GetDataBlock()->header);
    mSeqBytes = new u8[mSeqBytesSize];
    sead::MemUtil::copy(mSeqBytes, reader.GetSequenceData(), mSeqBytesSize);

    mSeqTextInfo.reset();
    mLabelsStartInfo.clear();
    mLabels.clear();
    mOffsetToLine.clear();
    mStartLabel.clear();

    std::vector<std::pair<u32, const char*>> labelOffsets;

    for (s32 i = 0; i < reader.GetLabelCount(); i++)
    {
        const char* label = reader.GetLabel(i);
        SEAD_ASSERT(label);

        u32 offset;
        if (!reader.GetOffsetByLabel(label, &offset))
        {
            return false;
        }

        labelOffsets.push_back(std::make_pair(offset, label));

        u32 allocTrackFlags = 1;
        u32 parsedOffset = MmlParser::ParseAllocTrack(reader.GetSequenceData(), offset, &allocTrackFlags);

        mLabelsStartInfo[label] = { offset, parsedOffset, allocTrackFlags };
    }

    std::sort(labelOffsets.begin(), labelOffsets.end());
    for (u32 i = 0; i < labelOffsets.size(); i++)
    {
        mLabels.push_back(labelOffsets[i].second);
    }

    u32 offset = 0;
    u32 allocTrackFlags = 1;
    u32 parsedOffset = MmlParser::ParseAllocTrack(reader.GetSequenceData(), offset, &allocTrackFlags);

    mLabelsStartInfo[""] = { offset, parsedOffset, allocTrackFlags };

    mOffsetToLine = offsetToLine;

    if (!mLoadError)
    {
        mIsValid = true;
    }

    return true;
}

u32 SequenceFile::doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const
{
    if (!isValid())
    {
        return 0;
    }

    FileWriter writer(handle, stream);
    writer.openFile("FSEQ", 2, mVersion);

    //? Data Block
    {
        writer.openBlock(nw::snd::internal::ElementType_SequenceSoundFile_DataBlock, "DATA");

        stream->writeMemBlock(mSeqBytes, mSeqBytesSize);

        writer.align(0x20);
        writer.closeBlock();
    }

    //? Label Block
    {
        writer.openBlock(nw::snd::internal::ElementType_SequenceSoundFile_LabelBlock, "LABL");

        writer.pushOffsetBase();
        {
            writer.openReferenceTable("LabelInfoTable", mLabels.size());

            std::vector<std::string> labels = mLabels;
            std::sort(labels.begin(), labels.end());

            for (u32 i = 0; i < labels.size(); i++)
            {
                writer.addReferenceTableReference("LabelInfoTable", nw::snd::internal::ElementType_SequenceSoundFile_LabelInfo);

                const std::string& label = labels[i];
                u32 labelSize = label.size();

                auto it = mLabelsStartInfo.find(label);
                SEAD_ASSERT(it != mLabelsStartInfo.end());
                u32 offset = it->second.offset;

                writer.pushOffsetBase();
                {
                    stream->writeU16(nw::snd::internal::ElementType_General_ByteStream); // Reference type
                    stream->writeU16(0); // Padding
                    stream->writeU32(offset);

                    stream->writeU32(labelSize);
                    stream->writeString(label.c_str(), labelSize + 1);

                    writer.align(4);
                }
                writer.popOffsetBase();
            }

            writer.closeReferenceTable("LabelInfoTable");
        }
        writer.popOffsetBase();

        writer.align(0x20);
        writer.closeBlock();
    }

    u32 fileSize = writer.getPosition();

    writer.closeFile();

    return fileSize;
}

//? Below is text pasrsing and compilation code

// trim from left
inline std::string& ltrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from right
inline std::string& rtrim(std::string& s, const char* t = " \t\n\r\f\v")
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from left & right
inline std::string& trim(std::string& s, const char* t = " \t\n\r\f\v")
{
    return ltrim(rtrim(s, t), t);
}

// for string delimiter
std::vector<std::string> split(std::string s, std::string delimiter)
{
    if (s.empty())
    {
        return { };
    }

    size_t pos_start = 0;
    size_t pos_end;
    size_t delim_len = delimiter.length();

    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
    {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;

        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

void SequenceFile::compile_(bool setCursorPos)
{
    mIsValid = false;
    mLabelsStartInfo.clear();
    mLabels.clear();
    mOffsetToLine.clear();

    std::unordered_map<std::string, std::pair<u32, MmlCommandBase*>> globalLabels; // <global label, <line, command after it>>

    std::string currentGlobalLabel;
    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<u32, MmlCommandBase*>>> scopedLocalLabels; // <global label, <local label, <line, command after it>>>

    std::vector<MmlCommandBase*> commands;

    std::unordered_map<MmlCommandBase*, u32> commandLine; // <command, line>
    std::unordered_map<MmlCommandBase*, std::string> commandGlobalScope; // <command, global label>

    struct CmdDeleter
    {
        CmdDeleter(std::vector<MmlCommandBase*>& cmds)
            : mCmds(cmds)
        {
        }

        ~CmdDeleter()
        {
            for (MmlCommandBase* cmd : mCmds)
            {
                delete cmd;
            }
        }

        std::vector<MmlCommandBase*>& mCmds;
    };
    CmdDeleter deleter(commands);

    auto setError = [&](u32 line, const std::string& errorMsg)
    {
        if (mSeqBytes)
        {
            invalidatePlayer_();

            delete mSeqBytes;
            mSeqBytes = nullptr;
            mSeqBytesSize = 0;
        }

        mLabels.clear();
        mOffsetToLine.clear();

        TextEditor::Marker error;
        error.mIsError = true;
        error.mShowDetail = true;
        error.mDetail = errorMsg;

        mTextEditor->SetMarkers({ { line + 1, error } });

        if (setCursorPos)
        {
            mTextEditor->SetCursorPosition({ s32(line), 0 });
            mTextEditor->EnsureCursorVisible();
        }
    };

    std::vector<std::string> labelToCmdQueue;

    u32 lastNonEmptyLine = 0;

    std::vector<std::string> lines = mTextEditor->GetTextLines();
    for (u32 i = 0; i < lines.size(); i++)
    {
        std::string line = lines[i];
        trim(line); // Remove leading and trailing spaces

        if (line.empty() || line.starts_with(';')) // Empty or comment
        {
            continue;
        }

        lastNonEmptyLine = i;

        line = line.substr(0, line.find(';')); // Remove comment
        rtrim(line);

        std::string::size_type labelDelimiter = line.rfind(':');
        if (labelDelimiter != std::string::npos)
        {
            std::string label = line.substr(0, labelDelimiter);
            rtrim(label);

            line = line.substr(labelDelimiter + 1); // Skip the label in case there is a command after it
            ltrim(line);

            if (label.empty())
            {
                setError(i, "Label is empty");
                return;
            }

            if (!label.starts_with('_') && !::isalpha(label[0]))
            {
                setError(i, "Labels must start with an alphabetic character");
                return;
            }

            auto isValidLabelChar = [](int c) -> int
            {
                return ::isalnum(c) || c == '_';
            };

            if (!std::all_of(label.begin(), label.end(), isValidLabelChar))
            {
                setError(i, "Invalid label");
                return;
            }

            if (label.starts_with('_'))
            {
                if (currentGlobalLabel.empty())
                {
                    setError(i, "Local label found outside any global label scope");
                    return;
                }

                std::unordered_map<std::string, std::pair<u32, MmlCommandBase*>>& localLabels = scopedLocalLabels[currentGlobalLabel];

                auto it = localLabels.find(label);
                if (it != localLabels.end())
                {
                    u32 labelLine = it->second.first;
                    setError(i, "Local label '" + label + "' already defined in line " + std::to_string(labelLine + 1));
                    return;
                }

                localLabels[label] = { i, nullptr };
            }
            else
            {
                auto it = globalLabels.find(label);
                if (it != globalLabels.end())
                {
                    u32 labelLine = it->second.first;
                    setError(i, "Global label '" + label + "' already defined in line " + std::to_string(labelLine + 1));
                    return;
                }

                globalLabels[label] = { i, nullptr };
                currentGlobalLabel = label; // Update current scope

                mLabels.push_back(label);
            }

            labelToCmdQueue.push_back(label);
        }

        if (line.empty())
        {
            continue;
        }

        std::string cmdStr = line.substr(0, line.find(' '));
        std::string argsStr = line.substr(cmdStr.size());
        ltrim(argsStr);

        std::vector<std::string> args = split(argsStr, ",");
        for (auto& arg : args)
        {
            trim(arg);
        }

        std::string errorMsg;
        MmlCommandBase* cmd = parseCommand_(cmdStr, args, errorMsg);
        if (!cmd)
        {
            setError(i, errorMsg);

            return;
        }

        commands.push_back(cmd);
        commandLine[cmd] = i;

        // Associate this command with the current global scope
        commandGlobalScope[cmd] = currentGlobalLabel;

        for (const auto& label : labelToCmdQueue)
        {
            if (label.starts_with('_'))
            {
                std::unordered_map<std::string, std::pair<u32, MmlCommandBase*>>& localLabels = scopedLocalLabels[currentGlobalLabel];
                localLabels[label].second = cmd;
            }
            else
            {
                globalLabels[label].second = cmd;
            }
        }

        labelToCmdQueue.clear();
    }

    //? FIN
    {
        MmlCommandBase* cmd = new MmlCommandFin(false);
        commands.push_back(cmd);

        for (const auto& label : labelToCmdQueue)
        {
            if (label.starts_with('_'))
            {
                std::unordered_map<std::string, std::pair<u32, MmlCommandBase*>>& localLabels = scopedLocalLabels[currentGlobalLabel];
                localLabels[label].second = cmd;
            }
            else
            {
                globalLabels[label].second = cmd;
            }
        }

        labelToCmdQueue.clear();

        commandLine[cmd] = lastNonEmptyLine;
    }

    std::unordered_map<MmlCommandBase*, u32> commandOffset; // <command, byte offset>
    std::vector<MmlCommandBase*> labelCommands;

    std::vector<u8> seqBytes;
    for (MmlCommandBase* cmd : commands)
    {
        SEAD_ASSERT(cmd);

        u32 offset = seqBytes.size();
        commandOffset[cmd] = offset;

        std::vector<u8> cmdBytes = cmd->encode();
        if (cmdBytes.empty())
        {
            return;
        }

        seqBytes.insert(seqBytes.end(), cmdBytes.begin(), cmdBytes.end());

        if (sead::IsDerivedTypes<MmlCommandOpenTrack>(cmd) || sead::IsDerivedTypes<MmlCommandJump>(cmd) || sead::IsDerivedTypes<MmlCommandCall>(cmd))
        {
            labelCommands.push_back(cmd);
        }

        mOffsetToLine[offset] = commandLine[cmd] + 1;
    }

    //? Fix commands that needs labels
    for (MmlCommandBase* cmd : labelCommands)
    {
        u32 offset = commandOffset[cmd];

        LabelObj* labelObj = nullptr;

        MmlCommandOpenTrack* openTrack = sead::DynamicCast<MmlCommandOpenTrack>(cmd);
        MmlCommandJump* jump = sead::DynamicCast<MmlCommandJump>(cmd);
        MmlCommandCall* call = sead::DynamicCast<MmlCommandCall>(cmd);
        if (openTrack)
        {
            labelObj = &openTrack->mLabelObj;
        }
        else if (jump)
        {
            labelObj = &jump->mLabelObj;
        }
        else if (call)
        {
            labelObj = &call->mLabelObj;
        }
        else
        {
            SEAD_ASSERT(false);
            return;
        }

        const std::string& label = labelObj->mLabel;

        // Resolve label based on its type (local or global)
        if (label.starts_with('_'))
        {
            // Local label - look up in the scope associated with this command
            const std::string& scope = commandGlobalScope[cmd];
            auto scopeIt = scopedLocalLabels.find(scope);
            if (scopeIt == scopedLocalLabels.end())
            {
                u32 line = commandLine[cmd];
                setError(line, "Local label '" + label + "' not found in scope of '" + scope + "'");
                return;
            }

            std::unordered_map<std::string, std::pair<u32, MmlCommandBase*>>& localLabels = scopeIt->second;
            auto it = localLabels.find(label);
            if (it == localLabels.end())
            {
                u32 line = commandLine[cmd];
                setError(line, "Local label '" + label + "' not found in scope of '" + scope + "'");
                return;
            }

            // Retrieve the command for this local label
            MmlCommandBase* dstCmd = it->second.second;
            labelObj->mOffset = commandOffset[dstCmd];
        }
        else
        {
            // Global label
            auto it = globalLabels.find(label);
            if (it == globalLabels.end())
            {
                u32 line = commandLine[cmd];
                setError(line, "Global label '" + label + "' not found");
                return;
            }

            MmlCommandBase* dstCmd = it->second.second;
            labelObj->mOffset = commandOffset[dstCmd];
        }

        if (labelObj->mOffset > 0xFFFFFF)
        {
            setError(0, "Jumping to out of bounds offset");
            return;
        }

        std::vector<u8> cmdBytes = cmd->encode();
        sead::MemUtil::copy(seqBytes.data() + offset, cmdBytes.data(), cmdBytes.size());
    }

    if (mSeqBytes)
    {
        invalidatePlayer_();

        delete mSeqBytes;
        mSeqBytes = nullptr;
        mSeqBytesSize = 0;
    }

    mSeqBytesSize = seqBytes.size();
    mSeqBytes = new u8[mSeqBytesSize];
    sead::MemUtil::copy(mSeqBytes, seqBytes.data(), mSeqBytesSize);

    auto getStartInfo = [&](MmlCommandBase* cmd) -> LabelStartInfo
    {
        u32 offset = commandOffset[cmd];

        u32 parsedOffset = offset;
        u32 allocTrackFlags = 1;
        MmlCommandAllocTrack* allocTrack = sead::DynamicCast<MmlCommandAllocTrack>(cmd);
        if (allocTrack)
        {
            SeqArg16* arg = sead::DynamicCast<SeqArg16>(allocTrack->mArg);
            SEAD_ASSERT(arg);

            parsedOffset += 3; // Skip alloctrack command
            allocTrackFlags = arg->mValue | 1; // Always enable track 0
        }

        return { offset, parsedOffset, allocTrackFlags };
    };

    for (auto pair : globalLabels)
    {
        const auto& label = pair.first;
        MmlCommandBase* dstCmd = pair.second.second;

        mLabelsStartInfo[label] = getStartInfo(dstCmd);
    }

    mLabelsStartInfo[""] = getStartInfo(commands[0]); // Empty label points to start

    mIsValid = true;
    mIsDirty = false;
}

template <typename T>
T pop_vector(std::vector<T>& vec)
{
    if (vec.empty())
    {
        return { };
    }

    T elem = vec.front();
    vec.erase(vec.begin());
    return elem;
}

bool toInt(int* out, std::string str)
{
    SEAD_ASSERT(out);

    bool negative = false;
    if (str.starts_with('-'))
    {
        negative = true;
        str = str.substr(1);
    }
    else if (str.starts_with('+'))
    {
        str = str.substr(1);
    }

    s32 base = 10;
    if (str.starts_with("0x"))
    {
        base = 16;

        if (!std::all_of(str.begin() + 2, str.end(), ::isxdigit))
        {
            return false;
        }
    }
    else if (str.starts_with("0b"))
    {
        base = 2;

        auto isbdigit = [](int c) -> int
        {
            return c == '0' || c == '1';
        };

        if (!std::all_of(str.begin() + 2, str.end(), isbdigit))
        {
            return false;
        }
    }
    else
    {
        if (!std::all_of(str.begin(), str.end(), ::isdigit))
        {
            return false;
        }
    }

    int value;
    if (std::from_chars(str.data() + (base != 10 ? 2 : 0), str.data() + str.size(), value, base).ec != std::errc{})
    {
        return false;
    }

    *out = negative ? -value : value;
    return true;
}

bool ParseIntArg(int* out, std::vector<std::string>& args, u32 curArg, std::string& errorMsg)
{
    SEAD_ASSERT(out);

    std::string arg = pop_vector(args);
    if (arg.empty())
    {
        errorMsg = "Argument " + std::to_string(curArg) + " is empty";
        return false;
    }

    int argInt;
    if (!toInt(&argInt, arg))
    {
        errorMsg = "Argument " + std::to_string(curArg) + " is invalid. Must be a number";
        return false;
    }

    *out = argInt;
    return true;
}

bool ParseListArg(int* out, std::vector<std::string>& args, u32 curArg, const char** list, u32 listSize, const std::string& type, std::string& errorMsg)
{
    SEAD_ASSERT(out);

    std::string arg = pop_vector(args);
    if (arg.empty())
    {
        errorMsg = "Argument " + std::to_string(curArg) + " is empty";
        return false;
    }

    int argInt = -1;
    for (u32 i = 0; i < listSize; i++)
    {
        if (arg == list[i])
        {
            argInt = i;
            break;
        }
    }

    if (argInt < 0 && !toInt(&argInt, arg))
    {
        errorMsg = "Argument " + std::to_string(curArg) + " is invalid. Must be a number or " + type;
        return false;
    }

    *out = argInt;
    return true;
}

bool ParseVariableArg(int* out, std::vector<std::string>& args, u32 curArg, std::string& errorMsg)
{
    SEAD_ASSERT(out);

    const u32 cVariableNum = 16 * 3;
    const char* cVariables[cVariableNum] = {
        "VAR_0",
        "VAR_1",
        "VAR_2",
        "VAR_3",
        "VAR_4",
        "VAR_5",
        "VAR_6",
        "VAR_7",
        "VAR_8",
        "VAR_9",
        "VAR_10",
        "VAR_11",
        "VAR_12",
        "VAR_13",
        "VAR_14",
        "VAR_15",

        "GVAR_0",
        "GVAR_1",
        "GVAR_2",
        "GVAR_3",
        "GVAR_4",
        "GVAR_5",
        "GVAR_6",
        "GVAR_7",
        "GVAR_8",
        "GVAR_9",
        "GVAR_10",
        "GVAR_11",
        "GVAR_12",
        "GVAR_13",
        "GVAR_14",
        "GVAR_15",

        "TVAR_0",
        "TVAR_1",
        "TVAR_2",
        "TVAR_3",
        "TVAR_4",
        "TVAR_5",
        "TVAR_6",
        "TVAR_7",
        "TVAR_8",
        "TVAR_9",
        "TVAR_10",
        "TVAR_11",
        "TVAR_12",
        "TVAR_13",
        "TVAR_14",
        "TVAR_15"
    };

    int argInt;
    if (!ParseListArg(&argInt, args, curArg, cVariables, cVariableNum, "variable identifier", errorMsg))
    {
        return false;
    }

    if (!(0 <= argInt && argInt < cVariableNum))
    {
        errorMsg = "Argument " + std::to_string(curArg) + " must in the range [0, " + std::to_string(cVariableNum) + ")";
        return false;
    }

    *out = argInt;
    return true;
}

struct CommandInfo;

using CommandFactory = MmlCommandBase* (*)();
using ArgCallback = bool (*)(MmlCommandBase* base, const std::string& cmdName, const CommandInfo& info, const std::vector<std::string>& extensions, const std::vector<std::string>& args, std::string& errorMsg);

template <typename T>
MmlCommandBase* TCommandFactory()
{
    return new T();
}

enum class SeqArgType
{
    None,
    ArgU8,
    ArgS8,
    ArgU16,
    ArgS16,
    ArgVMIDI,
    ArgBool
};

struct CommandInfo
{
    CommandFactory factory;
    SeqArgType defaultArgType;
    u32 baseArgCount;
    bool allowVariableAndRandom;
    bool allowTimeChange;
    bool allowIf;
    ArgCallback argCallback;
    u32 version = 0x00010000;
};

MmlCommandBase* BuildCommand(const std::string& cmdName, const CommandInfo& info, std::vector<std::string> extensions, std::vector<std::string> args, std::string& errorMsg)
{
    u32 maxExtNum = 0;
    maxExtNum += info.allowVariableAndRandom;
    maxExtNum += info.allowTimeChange;
    maxExtNum += info.allowIf;

    u32 argNum = info.baseArgCount;

    bool isConditional = false;

    if (extensions.size() > maxExtNum)
    {
        errorMsg = "Too many extensions";
        return nullptr;
    }

    bool foundVariableAndRandomExt = false;
    bool foundTimeChangeExt = false;
    bool foundIfExt = false;
    for (u32 i = 0; i < extensions.size(); i++)
    {
        if (extensions[i] == "r")
        {
            if (foundVariableAndRandomExt)
            {
                errorMsg = "'_r' extension present multiple times";
                return nullptr;
            }

            foundVariableAndRandomExt = true;

            if (i != 0)
            {
                errorMsg = "'_r' extension must be first";
                return nullptr;
            }
            else if (!info.allowVariableAndRandom)
            {
                errorMsg = "Command '" + cmdName + "' doesn't allow '_r' extension";
                return nullptr;
            }

            argNum += 1;
        }
        else if (extensions[i] == "v")
        {
            if (foundVariableAndRandomExt)
            {
                errorMsg = "'_v' extension present multiple times";
                return nullptr;
            }

            foundVariableAndRandomExt = true;

            if (i != 0)
            {
                errorMsg = "'_v' extension must be first";
                return nullptr;
            }
            else if (!info.allowVariableAndRandom)
            {
                errorMsg = "Command '" + cmdName + "' doesn't allow '_v' extension";
                return nullptr;
            }
        }
        else if (extensions[i] == "t")
        {
            if (foundTimeChangeExt)
            {
                errorMsg = "'_t' extension present multiple times";
                return nullptr;
            }

            foundTimeChangeExt = true;

            if (!info.allowTimeChange)
            {
                errorMsg = "Command '" + cmdName + "' doesn't allow '_t' extension";
                return nullptr;
            }

            argNum += 1;
        }
        else if (extensions[i] == "tr")
        {
            if (foundTimeChangeExt)
            {
                errorMsg = "'_tr' extension present multiple times";
                return nullptr;
            }

            foundTimeChangeExt = true;

            if (!info.allowTimeChange)
            {
                errorMsg = "Command '" + cmdName + "' doesn't allow '_tr' extension";
                return nullptr;
            }

            argNum += 2;
        }
        else if (extensions[i] == "tv")
        {
            if (foundTimeChangeExt)
            {
                errorMsg = "'_tv' extension present multiple times";
                return nullptr;
            }

            foundTimeChangeExt = true;

            if (!info.allowTimeChange)
            {
                errorMsg = "Command '" + cmdName + "' doesn't allow '_tv' extension";
                return nullptr;
            }

            argNum += 1;
        }
        else if (extensions[i] == "if")
        {
            if (foundIfExt)
            {
                errorMsg = "'_if' extension present multiple times";
                return nullptr;
            }

            foundIfExt = true;

            if (i != extensions.size() - 1)
            {
                errorMsg = "'_if' extension must be last";
                return nullptr;
            }
            else if (!info.allowIf)
            {
                errorMsg = "Command '" + cmdName + "' doesn't allow '_if' extension";
                return nullptr;
            }

            isConditional = true;
        }
        else
        {
            errorMsg = "Unknown extension '_" + extensions[i] + "'";
            return nullptr;
        }
    }

    if (args.size() > argNum)
    {
        errorMsg = "Too many arguments. Expected " + std::to_string(argNum) + ", got " + std::to_string(args.size());
        return nullptr;
    }
    else if (args.size() < argNum)
    {
        errorMsg = "Missing arguments. Expected " + std::to_string(argNum) + ", got " + std::to_string(args.size());
        return nullptr;
    }

    // Ugh
    struct CmdDeleter
    {
        CmdDeleter()
            : cmd(nullptr)
        {
        }

        ~CmdDeleter()
        {
            if (cmd)
            {
                delete cmd;
                cmd = nullptr;
            }
        }

        MmlCommandBase* cmd;
    };

    MmlCommandBase* base = info.factory();
    base->mConditional = isConditional;

    CmdDeleter deleter;
    deleter.cmd = base;

    if (info.argCallback)
    {
        if (!info.argCallback(base, cmdName, info, extensions, args, errorMsg))
        {
            if (errorMsg.empty())
            {
                errorMsg = "Internal error !";
            }

            return nullptr;
        }
    }
    else if (info.defaultArgType == SeqArgType::ArgBool)
    {
        u8 arg = 0;
        if (cmdName.ends_with("on"))
        {
            if (cmdName.starts_with("damper")) // cringe
            {
                arg = 127;
            }
            else
            {
                arg = 1;
            }
        }
        else if (cmdName.ends_with("off"))
        {
            arg = 0;
        }
        else
        {
            errorMsg = "Internal error !";
            return nullptr;
        }

        SeqArgBase* seqArg = new SeqArg8(arg, false);
        if (!base->setArg1(seqArg))
        {
            delete seqArg;
            errorMsg = "Internal error !";
            return nullptr;
        }
    }
    else
    {
        // TODO: Range checks

        auto parseNormal = [&](u32 curArg, u32 innerArg)
        {
            int argInt;
            if (!ParseIntArg(&argInt, args, innerArg, errorMsg))
            {
                return false;
            }

            SeqArgBase* seqArg = nullptr;
            if (curArg == 1) // TODO: Unhardcode this ?
            {
                seqArg = new SeqArg16(argInt, true);
            }
            else
            {
                switch (info.defaultArgType)
                {
                    case SeqArgType::ArgU8:
                        seqArg = new SeqArg8(argInt, false);
                        break;
                    case SeqArgType::ArgS8:
                        seqArg = new SeqArg8(argInt, true);
                        break;
                    case SeqArgType::ArgU16:
                        seqArg = new SeqArg16(argInt, false);
                        break;
                    case SeqArgType::ArgS16:
                        seqArg = new SeqArg16(argInt, true);
                        break;
                    case SeqArgType::ArgVMIDI:
                        seqArg = new SeqArgVMIDI(argInt);
                        break;
                }
            }

            if (!seqArg || !base->setArg(curArg, seqArg))
            {
                delete seqArg;
                errorMsg = "Argument " + std::to_string(innerArg) + ": Internal error !";
                return false;
            }

            return true;
        };

        auto parseRandom = [&](u32 curArg, u32 innerArg)
        {
            int minInt;
            if (!ParseIntArg(&minInt, args, innerArg, errorMsg))
            {
                return false;
            }

            innerArg++;

            int maxInt;
            if (!ParseIntArg(&maxInt, args, innerArg, errorMsg))
            {
                return false;
            }

            SeqArgBase* seqArg = new SeqArgRandom(minInt, maxInt);
            if (!base->setArg(curArg, seqArg))
            {
                delete seqArg;
                errorMsg = "Argument (" + std::to_string(innerArg - 1) + ", " + std::to_string(innerArg) + "): Internal error !";
                return false;
            }

            return true;
        };

        auto parseVariable = [&](u32 curArg, u32 innerArg)
        {
            int argInt;
            if (!ParseVariableArg(&argInt, args, innerArg, errorMsg))
            {
                return false;
            }

            SeqArgBase* seqArg = new SeqArgVariable(argInt);
            if (!base->setArg(curArg, seqArg))
            {
                delete seqArg;
                errorMsg = "Argument " + std::to_string(innerArg) + ": Internal error !";
                return false;
            }

            return true;
        };

        bool foundExt[] = { foundVariableAndRandomExt, foundTimeChangeExt };

        u32 curArg = 0;
        for (u32 i = 0; i < argNum; i++)
        {
            std::string ext;
            if (foundExt[curArg])
            {
                ext = pop_vector(extensions);
            }

            if (ext == "r" || ext == "tr")
            {
                if (!parseRandom(curArg, i + 1))
                {
                    return nullptr;
                }

                i++;
            }
            else if (ext == "v" || ext == "tv")
            {
                if (!parseVariable(curArg, i + 1))
                {
                    return nullptr;
                }
            }
            else
            {
                if (!parseNormal(curArg, i + 1))
                {
                    return nullptr;
                }
            }

            curArg++;
        }
    }

    deleter.cmd = nullptr;

    return base;
}

bool CmdNoteArgCallback(MmlCommandBase* base, const std::string& cmdName, const CommandInfo& info, const std::vector<std::string>& extensions_, const std::vector<std::string>& args_, std::string& errorMsg)
{
    SEAD_ASSERT(base);

    std::vector<std::string> extensions = extensions_;
    std::vector<std::string> args = args_;

    MmlCommandNote* cmd = static_cast<MmlCommandNote*>(base);

    bool foundKey = false;
    for (u32 key = 0; key < MmlCommandNote::sKeysNum; key++)
    {
        if (cmdName == MmlCommandNote::sKeys[key])
        {
            cmd->mKey = key;

            foundKey = true;
            break;
        }
    }

    if (!foundKey)
    {
        return false;
    }

    u32 curArg = 1;

    int velocityInt;
    if (!ParseIntArg(&velocityInt, args, curArg, errorMsg))
    {
        return false;
    }

    cmd->mVelocity = velocityInt;

    curArg++;

    SeqArgBase* length = nullptr;

    std::string ext = pop_vector(extensions);
    if (ext == "r")
    {
        int minInt;
        if (!ParseIntArg(&minInt, args, curArg, errorMsg))
        {
            return false;
        }

        curArg++;

        int maxInt;
        if (!ParseIntArg(&maxInt, args, curArg, errorMsg))
        {
            return false;
        }

        length = new SeqArgRandom(minInt, maxInt);
    }
    else if (ext == "v")
    {
        int argInt;
        if (!ParseVariableArg(&argInt, args, curArg, errorMsg))
        {
            return false;
        }

        length = new SeqArgVariable(argInt);
    }
    else
    {
        int lenInt;
        if (!ParseIntArg(&lenInt, args, curArg, errorMsg))
        {
            return false;
        }

        length = new SeqArgVMIDI(lenInt);
    }

    if (!cmd->setArg2(length))
    {
        delete length;
        return false;
    }

    return true;
}

bool ParseLabelArg(std::string* out, std::vector<std::string>& args, u32 curArg, std::string& errorMsg)
{
    SEAD_ASSERT(out);

    std::string arg = pop_vector(args);
    if (arg.empty())
    {
        errorMsg = "Argument " + std::to_string(curArg) + " is empty";
        return false;
    }

    auto isValidLabelChar = [](int c) -> int
    {
        return ::isalnum(c) || c == '_';
    };

    if (!arg.starts_with('_') && !::isalpha(arg[0]) || !std::all_of(arg.begin(), arg.end(), isValidLabelChar))
    {
        errorMsg = "Argument " + std::to_string(curArg) + " is not a valid label";
        return false;
    }

    *out = arg;
    return true;
}

bool CmdOpenTrackArgCallback(MmlCommandBase* base, const std::string& cmdName, const CommandInfo& info, const std::vector<std::string>& extensions_, const std::vector<std::string>& args_, std::string& errorMsg)
{
    SEAD_ASSERT(base);

    std::vector<std::string> args = args_;

    MmlCommandOpenTrack* cmd = static_cast<MmlCommandOpenTrack*>(base);

    int trackNo;
    if (!ParseIntArg(&trackNo, args, 1, errorMsg))
    {
        return false;
    }

    if (!(0 <= trackNo && trackNo < 16))
    {
        errorMsg = "Argument 1 must in the range [0, 16)";
        return false;
    }

    cmd->mTrackNo = trackNo;

    if (!ParseLabelArg(&cmd->mLabelObj.mLabel, args, 2, errorMsg))
    {
        return false;
    }

    return true;
}

bool CmdJumpAndCallArgCallback(MmlCommandBase* base, const std::string& cmdName, const CommandInfo& info, const std::vector<std::string>& extensions_, const std::vector<std::string>& args_, std::string& errorMsg)
{
    SEAD_ASSERT(base);

    std::vector<std::string> args = args_;

    std::string label;
    if (!ParseLabelArg(&label, args, 1, errorMsg))
    {
        return false;
    }

    MmlCommandJump* jump = sead::DynamicCast<MmlCommandJump>(base);
    MmlCommandCall* call = sead::DynamicCast<MmlCommandCall>(base);
    if (jump)
    {
        jump->mLabelObj.mLabel = label;
    }
    else if (call)
    {
        call->mLabelObj.mLabel = label;
    }
    else
    {
        return false;
    }

    return true;
}

bool CmdAllocTrackArgCallback(MmlCommandBase* base, const std::string& cmdName, const CommandInfo& info, const std::vector<std::string>& extensions_, const std::vector<std::string>& args_, std::string& errorMsg)
{
    SEAD_ASSERT(base);

    std::vector<std::string> args = args_;

    MmlCommandAllocTrack* cmd = static_cast<MmlCommandAllocTrack*>(base);

    std::string arg = pop_vector(args);
    if (arg.empty())
    {
        errorMsg = "Argument 1 is empty";
        return false;
    }

    bool validArg = true;
    int argInt = 0;

    std::vector<std::string> tokens = split(arg, "|");
    for (auto token : tokens)
    {
        trim(token);

        int track = -1;
        for (u32 i = 0; i < 16; i++)
        {
            if (token == "TRACK_" + std::to_string(i))
            {
                track = 1 << i;
                break;
            }
        }

        if (track < 0 && !toInt(&track, token))
        {
            validArg = false;
            break;
        }

        argInt |= track;
    }

    if (!validArg && !toInt(&argInt, arg))
    {
        errorMsg = "Argument 1 is invalid. Must be a number or track identifier";
        return false;
    }

    SeqArgBase* seqArg = new SeqArg16(argInt, false);
    if (!cmd->setArg1(seqArg))
    {
        delete seqArg;
        return false;
    }

    return true;
}

bool CmdPortaArgCallback(MmlCommandBase* base, const std::string& cmdName, const CommandInfo& info, const std::vector<std::string>& extensions_, const std::vector<std::string>& args_, std::string& errorMsg)
{
    SEAD_ASSERT(base);

    std::vector<std::string> args = args_;

    MmlCommandPorta* cmd = static_cast<MmlCommandPorta*>(base);

    int argInt;
    if (!ParseListArg(&argInt, args, 1, MmlCommandNote::sKeys, MmlCommandNote::sKeysNum, "note identifier", errorMsg))
    {
        return false;
    }

    if (!(0 <= argInt && argInt < MmlCommandNote::sKeysNum))
    {
        errorMsg = "Argument 1 must in the range [0, " + std::to_string(MmlCommandNote::sKeysNum) + ")";
        return false;
    }

    SeqArgBase* seqArg = new SeqArg8(argInt, false);
    if (!cmd->setArg1(seqArg))
    {
        delete seqArg;
        return false;
    }

    return true;
}

bool CmdModTypeArgCallback(MmlCommandBase* base, const std::string& cmdName, const CommandInfo& info, const std::vector<std::string>& extensions_, const std::vector<std::string>& args_, std::string& errorMsg)
{
    SEAD_ASSERT(base);

    std::vector<std::string> args = args_;

    MmlCommandModType* cmd = static_cast<MmlCommandModType*>(base);

    int argInt;
    if (!ParseListArg(&argInt, args, 1, MmlCommandModType::cTypes, MmlCommandModType::cTypeNum, "modulation type identifier", errorMsg))
    {
        return false;
    }

    if (!(0 <= argInt && argInt < MmlCommandModType::cTypeNum))
    {
        errorMsg = "Argument 1 must in the range [0, " + std::to_string(MmlCommandModType::cTypeNum) + ")";
        return false;
    }

    SeqArgBase* seqArg = new SeqArg8(argInt, false);
    if (!cmd->setArg1(seqArg))
    {
        delete seqArg;
        return false;
    }

    return true;
}

bool CmdPrintVarArgCallback(MmlCommandBase* base, const std::string& cmdName, const CommandInfo& info, const std::vector<std::string>& extensions_, const std::vector<std::string>& args_, std::string& errorMsg)
{
    SEAD_ASSERT(base);

    std::vector<std::string> args = args_;

    MmlCommandPrintVar* cmd = static_cast<MmlCommandPrintVar*>(base);

    int argInt;
    if (!ParseVariableArg(&argInt, args, 1, errorMsg))
    {
        return false;
    }

    SeqArgBase* seqArg = new SeqArg8(argInt);
    if (!cmd->setArg1(seqArg))
    {
        delete seqArg;
        errorMsg = "Argument 1: Internal error !";
        return false;
    }

    return true;
}

template <typename T>
bool CmdVarArgCallback(MmlCommandBase* base, const std::string& cmdName, const CommandInfo& info, const std::vector<std::string>& extensions_, const std::vector<std::string>& args_, std::string& errorMsg)
{
    SEAD_ASSERT(base);

    std::vector<std::string> extensions = extensions_;
    std::vector<std::string> args = args_;

    T* cmd = static_cast<T*>(base);

    u32 curArg = 1;

    int arg1Int;
    if (!ParseVariableArg(&arg1Int, args, curArg, errorMsg))
    {
        return false;
    }

    SeqArgBase* seqArg1 = new SeqArgVariable(arg1Int);
    if (!cmd->setArg1(seqArg1))
    {
        delete seqArg1;
        errorMsg = "Argument " + std::to_string(curArg) + ": Internal error !";
        return false;
    }

    curArg++;

    SeqArgBase* seqArg2 = nullptr;

    std::string ext = pop_vector(extensions);
    if (ext == "r")
    {
        int minInt;
        if (!ParseIntArg(&minInt, args, curArg, errorMsg))
        {
            return false;
        }

        curArg++;

        int maxInt;
        if (!ParseIntArg(&maxInt, args, curArg, errorMsg))
        {
            return false;
        }

        seqArg2 = new SeqArgRandom(minInt, maxInt);
    }
    else if (ext == "v")
    {
        int argInt;
        if (!ParseVariableArg(&argInt, args, curArg, errorMsg))
        {
            return false;
        }

        seqArg2 = new SeqArgVariable(argInt);
    }
    else
    {
        int valueInt;
        if (!ParseIntArg(&valueInt, args, curArg, errorMsg))
        {
            return false;
        }

        seqArg2 = new SeqArg16(valueInt, true);
    }

    if (!cmd->setArg2(seqArg2))
    {
        delete seqArg2;
        return false;
    }

    return true;
}

std::pair<std::string, CommandInfo> GetCommandInfo(const std::string& cmd)
{
    static std::unordered_map<std::string, CommandInfo> sCommandInfo = {
        { "wait",            { &TCommandFactory<MmlCommandWait>,          SeqArgType::ArgVMIDI, 1, true,  false, true, nullptr } },
        { "prg",             { &TCommandFactory<MmlCommandPrg>,           SeqArgType::ArgVMIDI, 1, true,  false, true, nullptr } },

        { "opentrack",       { &TCommandFactory<MmlCommandOpenTrack>,     SeqArgType::None,     2, false, false, true, &CmdOpenTrackArgCallback } },
        { "jump",            { &TCommandFactory<MmlCommandJump>,          SeqArgType::None,     1, false, false, true, &CmdJumpAndCallArgCallback } },
        { "call",            { &TCommandFactory<MmlCommandCall>,          SeqArgType::None,     1, false, false, true, &CmdJumpAndCallArgCallback } },

        { "timebase",        { &TCommandFactory<MmlCommandTimebase>,      SeqArgType::ArgU8,    1, false, false, true, nullptr } },
        { "env_hold",        { &TCommandFactory<MmlCommandEnvHold>,       SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "monophonic_on",   { &TCommandFactory<MmlCommandMonophonic>,    SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "monophonic_off",  { &TCommandFactory<MmlCommandMonophonic>,    SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "velocity_range",  { &TCommandFactory<MmlCommandVelocityRange>, SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "biquad_type",     { &TCommandFactory<MmlCommandBiquadType>,    SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "biquad_value",    { &TCommandFactory<MmlCommandBiquadValue>,   SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "bank_select",     { &TCommandFactory<MmlCommandBankSelect>,    SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "mod_phase",       { &TCommandFactory<MmlCommandModPhase>,      SeqArgType::ArgU8,    1, true,  false, true, nullptr, 0x00020000 } },
        { "mod_curve",       { &TCommandFactory<MmlCommandModCurve>,      SeqArgType::ArgU8,    1, true,  false, true, nullptr, 0x00020000 } },
        { "frontbypass_on",  { &TCommandFactory<MmlCommandFrontBypass>,   SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "frontbypass_off", { &TCommandFactory<MmlCommandFrontBypass>,   SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "pan",             { &TCommandFactory<MmlCommandPan>,           SeqArgType::ArgU8,    1, true,  true,  true, nullptr } },
        { "volume",          { &TCommandFactory<MmlCommandVolume>,        SeqArgType::ArgU8,    1, true,  true,  true, nullptr } },
        { "main_volume",     { &TCommandFactory<MmlCommandMainVolume>,    SeqArgType::ArgU8,    1, true,  true,  true, nullptr } },
        { "transpose",       { &TCommandFactory<MmlCommandTranspose>,     SeqArgType::ArgS8,    1, true,  false, true, nullptr } },
        { "pitchbend",       { &TCommandFactory<MmlCommandPitchBend>,     SeqArgType::ArgS8,    1, true,  true,  true, nullptr } },
        { "bendrange",       { &TCommandFactory<MmlCommandBendRange>,     SeqArgType::ArgU8,    1, false, false, true, nullptr } },
        { "prio",            { &TCommandFactory<MmlCommandPrio>,          SeqArgType::ArgU8,    1, false, false, true, nullptr } },
        { "notewait_on",     { &TCommandFactory<MmlCommandNoteWait>,      SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "notewait_off",    { &TCommandFactory<MmlCommandNoteWait>,      SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "tieon",           { &TCommandFactory<MmlCommandTie>,           SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "tieoff",          { &TCommandFactory<MmlCommandTie>,           SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "porta",           { &TCommandFactory<MmlCommandPorta>,         SeqArgType::ArgU8,    1, false, false, true, &CmdPortaArgCallback } },
        { "mod_depth",       { &TCommandFactory<MmlCommandModDepth>,      SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "mod_speed",       { &TCommandFactory<MmlCommandModSpeed>,      SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "mod_type",        { &TCommandFactory<MmlCommandModType>,       SeqArgType::ArgU8,    1, false, false, true, &CmdModTypeArgCallback } },
        { "mod_range",       { &TCommandFactory<MmlCommandModRange>,      SeqArgType::ArgU8,    1, false, false, true, nullptr } },
        { "porta_on",        { &TCommandFactory<MmlCommandPortaSw>,       SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "porta_off",       { &TCommandFactory<MmlCommandPortaSw>,       SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "porta_time",      { &TCommandFactory<MmlCommandPortaTime>,     SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "attack",          { &TCommandFactory<MmlCommandAttack>,        SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "decay",           { &TCommandFactory<MmlCommandDecay>,         SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "sustain",         { &TCommandFactory<MmlCommandSustain>,       SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "release",         { &TCommandFactory<MmlCommandRelease>,       SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "loop_start",      { &TCommandFactory<MmlCommandLoopStart>,     SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "volume2",         { &TCommandFactory<MmlCommandVolume2>,       SeqArgType::ArgU8,    1, true,  true,  true, nullptr } },
        { "printvar",        { &TCommandFactory<MmlCommandPrintVar>,      SeqArgType::ArgU8,    1, false, false, true, &CmdPrintVarArgCallback } },
        { "span",            { &TCommandFactory<MmlCommandSurroundPan>,   SeqArgType::ArgU8,    1, true,  true,  true, nullptr } },
        { "lpf_cutoff",      { &TCommandFactory<MmlCommandLPFCutoff>,     SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "fxsend_a",        { &TCommandFactory<MmlCommandFxSendA>,       SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "fxsend_b",        { &TCommandFactory<MmlCommandFxSendB>,       SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "mainsend",        { &TCommandFactory<MmlCommandMainSend>,      SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "init_pan",        { &TCommandFactory<MmlCommandInitPan>,       SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "mute",            { &TCommandFactory<MmlCommandMute>,          SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "fxsend_c",        { &TCommandFactory<MmlCommandFxSendC>,       SeqArgType::ArgU8,    1, true,  false, true, nullptr } },
        { "damper_on",       { &TCommandFactory<MmlCommandDamper>,        SeqArgType::ArgBool,  0, false, false, true, nullptr } },
        { "damper_off",      { &TCommandFactory<MmlCommandDamper>,        SeqArgType::ArgBool,  0, false, false, true, nullptr } },

        { "mod_delay",       { &TCommandFactory<MmlCommandModDelay>,      SeqArgType::ArgS16,   1, true,  false, true, nullptr } },
        { "tempo",           { &TCommandFactory<MmlCommandTempo>,         SeqArgType::ArgS16,   1, true,  false, true, nullptr } },
        { "sweep_pitch",     { &TCommandFactory<MmlCommandSweepPitch>,    SeqArgType::ArgS16,   1, true,  false, true, nullptr } },
        { "mod_period",      { &TCommandFactory<MmlCommandModPeriod>,     SeqArgType::ArgS16,   1, true,  false, true, nullptr } },

        { "env_reset",       { &TCommandFactory<MmlCommandEnvReset>,      SeqArgType::None,     0, false, false, true, nullptr } },
        { "loop_end",        { &TCommandFactory<MmlCommandLoopEnd>,       SeqArgType::None,     0, false, false, true, nullptr } },
        { "ret",             { &TCommandFactory<MmlCommandRet>,           SeqArgType::None,     0, false, false, true, nullptr } },
        { "alloctrack",      { &TCommandFactory<MmlCommandAllocTrack>,    SeqArgType::ArgU16,   1, false, false, true, &CmdAllocTrackArgCallback } },
        { "fin",             { &TCommandFactory<MmlCommandFin>,           SeqArgType::None,     0, false, false, true, nullptr } },

        { "setvar",          { &TCommandFactory<MmlCommandSetVar>,        SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandSetVar> } },
        { "addvar",          { &TCommandFactory<MmlCommandAddVar>,        SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandAddVar> } },
        { "subvar",          { &TCommandFactory<MmlCommandSubVar>,        SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandSubVar> } },
        { "mulvar",          { &TCommandFactory<MmlCommandMulVar>,        SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandMulVar> } },
        { "divvar",          { &TCommandFactory<MmlCommandDivVar>,        SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandDivVar> } },
        { "shiftvar",        { &TCommandFactory<MmlCommandShiftVar>,      SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandShiftVar> } },
        { "randvar",         { &TCommandFactory<MmlCommandRandVar>,       SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandRandVar> } },
        { "andvar",          { &TCommandFactory<MmlCommandAndVar>,        SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandAndVar> } },
        { "orvar",           { &TCommandFactory<MmlCommandOrVar>,         SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandOrVar> } },
        { "xorvar",          { &TCommandFactory<MmlCommandXorVar>,        SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandXorVar> } },
        { "notvar",          { &TCommandFactory<MmlCommandNotVar>,        SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandNotVar> } },
        { "modvar",          { &TCommandFactory<MmlCommandModVar>,        SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandModVar> } },

        { "cmp_eq",          { &TCommandFactory<MmlCommandCmpEq>,         SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandCmpEq> } },
        { "cmp_ge",          { &TCommandFactory<MmlCommandCmpGe>,         SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandCmpGe> } },
        { "cmp_gt",          { &TCommandFactory<MmlCommandCmpGt>,         SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandCmpGt> } },
        { "cmp_le",          { &TCommandFactory<MmlCommandCmpLe>,         SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandCmpLe> } },
        { "cmp_lt",          { &TCommandFactory<MmlCommandCmpLt>,         SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandCmpLt> } },
        { "cmp_ne",          { &TCommandFactory<MmlCommandCmpNe>,         SeqArgType::ArgU8,    2, true,  false, true, &CmdVarArgCallback<MmlCommandCmpNe> } },

        // TODO: Rest of extended commands
    };

    static bool sInitedCmdInfo = false;
    if (!sInitedCmdInfo)
    {
        sInitedCmdInfo = true;

        for (u32 key = 0; key < MmlCommandNote::sKeysNum; key++)
        {
            sCommandInfo[MmlCommandNote::sKeys[key]] = { &TCommandFactory<MmlCommandNote>, SeqArgType::ArgVMIDI, 2, true, false, true, &CmdNoteArgCallback };
        }
    }

    auto it = sCommandInfo.find(cmd);
    if (it != sCommandInfo.end())
    {
        return *it;
    }

    std::vector<std::string> parts = split(cmd, "_");
    std::string baseCommand;
    std::string extensionChain;

    for (size_t i = 0; i < parts.size(); i++)
    {
        // Rebuild potential base command and extension chain
        baseCommand = parts[0];
        for (size_t j = 1; j < parts.size() - i; j++)
        {
            baseCommand += "_" + parts[j];
        }

        // If we found a valid base command in the map, check for extensions
        it = sCommandInfo.find(baseCommand);
        if (it != sCommandInfo.end())
        {
            //CommandInfo commandInfo = it->second;

            return *it;
        }
    }

    return { };
}

MmlCommandBase* SequenceFile::parseCommand_(const std::string& str, const std::vector<std::string>& args, std::string& errorMsg)
{
    MmlCommandBase* cmd = nullptr;

    auto info = GetCommandInfo(str);

    const std::string& cmdName = info.first;
    if (!cmdName.empty())
    {
        const CommandInfo& cmdInfo = info.second;
        if (cmdInfo.version > sBfsar.getVersionForBfseq())
        {
            errorMsg = "Command '" + cmdName + "' requires BFSEQ version >= " + std::format("0x{:08X}", cmdInfo.version);
            errorMsg += " (Current: " + std::format("0x{:08X}", sBfsar.getVersionForBfseq()) + ")";
            return nullptr;
        }

        std::string extensions = str.substr(cmdName.size());

        std::vector<std::string> ext = split(extensions, "_");
        if (ext.size() > 0)
        {
            pop_vector(ext);
        }

        cmd = BuildCommand(cmdName, cmdInfo, ext, args, errorMsg);
    }
    else
    {
        errorMsg = "Unknown command '" + str + "'";
    }

    return cmd;
}
