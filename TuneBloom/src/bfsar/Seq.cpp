#include <bfsar/SeqCommand.h>

#include <ui/UI.h>

#include <heap/seadFrameHeap.h>
#include <heap/seadHeapMgr.h>

#include <snd/snd_SequenceSoundFileReader.h>

#include <map>

#include <TextEditor.h>

extern TextEditor* sTextEditor;

extern MmlCommandBase* nw__snd__internal__driver__MmlParser__Parse(const u8*& trackData, nw::snd::internal::SequenceSoundFileReader& reader);

extern std::map<u32, u32>* sOffsetToLine;

void ParseSequenceFile(u32 fileId, sead::Heap* heap)
{
    std::vector<std::string> lines;
    std::map<u32, u32> offsetToLine; 

    {
        sead::CurrentHeapSetter chs(heap);

        const void* file = sBfsar.getSoundDataMgr().detail_GetFileAddress(fileId);
        if (!file)
            return;

        nw::snd::internal::SequenceSoundFileReader reader(file);
        reader.createLabelCache();

        const void* seqData = reader.GetSequenceData();
        const nw::ut::BinaryBlockHeader* seqDataBlockHeader = &reader.mHeader->GetDataBlock()->header;
        const u8* seqDataEnd = (u8*)seqDataBlockHeader + seqDataBlockHeader->size;

        const u8* trackPtr = (const u8*)seqData;

        std::map<u32, MmlCommandBase*> commands;
        while (true)
        {
            if (*trackPtr == 0xFF)
            {
                const u8* remainPtr = trackPtr + 1;
                u32 remainSize = seqDataEnd - remainPtr;
                bool allZero = true;

                for (u32 i = 0; i < remainSize; i++)
                {
                    if (remainPtr[i] != 0)
                    {
                        allZero = false;
                        break;
                    }
                }

                if (allZero)
                    break;
            }

            u32 offset = trackPtr - seqData;
            commands[offset] = nw__snd__internal__driver__MmlParser__Parse(trackPtr, reader);
        }

        const auto& labelCache = reader.mLabelCache;

        u32 endOffset = trackPtr - seqData;
        for (const auto& it : labelCache)
        {
            SEAD_ASSERT(commands.find(it.first) != commands.end() || it.first == endOffset);
        }

        for (const auto& it : commands)
        {
            u32 offset = it.first;
            MmlCommandBase* cmdInst = it.second;

            const auto& labelListIt = labelCache.find(offset);
            if (labelListIt != labelCache.end())
            {
                if (lines.size() != 0)
                    lines.emplace_back("");

                for (const auto& label : labelListIt->second)
                {
                    lines.emplace_back(label + ':');
                }
            }

            std::string cmd = "(null)";
            if (cmdInst)
                cmd = cmdInst->toString();

            u32 line = lines.size() + 1;

            offsetToLine[offset] = line;
            //SEAD_PRINT("offset: %u, line: %u, cmd: %s\n", offset, line, cmd.c_str());

            lines.emplace_back("    " + cmd);
        }

        const auto& labelListIt = labelCache.find(endOffset);
        if (labelListIt != labelCache.end() && commands.find(endOffset) == commands.end())
        {
            if (lines.size() != 0)
                lines.emplace_back("");

            for (const auto& label : labelListIt->second)
            {
                lines.emplace_back(label + ':');
            }
        }

        lines.emplace_back("");
    }

    if (sTextEditor)
        sTextEditor->SetTextLines(lines);

    if (sOffsetToLine)
        *sOffsetToLine = offsetToLine;

    //std::string fseq;
    for (const auto& line : lines)
    {
        //SEAD_PRINT("%s\n", line.c_str());
        //fseq += line + '\n';
    }

    //SEAD_PRINT("%s\n", fseq.c_str());
}

void ParseSequenceFile(u32 fileId)
{
    sead::Heap* heap = sead::FrameHeap::tryCreate(30 * 1024 * 1024, "SeqHeap", nullptr);
    heap->setEnableWarning(false);

    ParseSequenceFile(fileId, heap);

    heap->destroy();
}
