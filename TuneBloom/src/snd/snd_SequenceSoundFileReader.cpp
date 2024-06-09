#include <snd/snd_SequenceSoundFileReader.h>

#include <format>

namespace nw { namespace snd { namespace internal {

SequenceSoundFileReader::SequenceSoundFileReader(const void* sequenceFile)
    : mHeader(nullptr)
    , mDataBlockBody(nullptr)
    , mLabelBlockBody(nullptr)
{
    SEAD_ASSERT(sequenceFile);

    {
        const ut::BinaryFileHeader* header = reinterpret_cast<const ut::BinaryFileHeader*>(sequenceFile);

        //if (sead::MemUtil::compare(header->signature, "CSEQ", 4) != 0)
        if (sead::MemUtil::compare(header->signature, "FSEQ", 4) != 0)
        {
            SEAD_ASSERT_MSG(false, "not a SEQUENCE file");
            return;
        }

        //if (false)
        if (!(0x00010000 <= header->version && header->version <= 0x00020000))
        {
            SEAD_ASSERT_MSG(false, "SEQUENCE version not supported (0x%08X)", (u32)header->version);
            return;
        }
    }

    mHeader = reinterpret_cast<const SequenceSoundFile::FileHeader*>(sequenceFile);

    const SequenceSoundFile::DataBlock* dataBlock = mHeader->GetDataBlock();
    SEAD_ASSERT(dataBlock);

    if (sead::MemUtil::compare(dataBlock->header.kind, "DATA", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "SEQUENCE: DATA block is invalid");
        return;
    }

    const SequenceSoundFile::LabelBlock* labelBlock = mHeader->GetLabelBlock();
    SEAD_ASSERT(labelBlock);

    if (sead::MemUtil::compare(labelBlock->header.kind, "LABL", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "SEQUENCE: LABEL block is invalid");
        return;
    }

    mDataBlockBody = &dataBlock->body;
    mLabelBlockBody = &labelBlock->body;
}

const void* SequenceSoundFileReader::GetSequenceData() const
{
    SEAD_ASSERT(mDataBlockBody);
    return mDataBlockBody->GetSequenceData();
}

bool SequenceSoundFileReader::GetOffsetByLabel(const char* label, u32* offsetPtr) const
{
    SEAD_ASSERT(mLabelBlockBody);
    return mLabelBlockBody->GetOffsetByLabel(label, offsetPtr);
}

const char* SequenceSoundFileReader::GetLabelByOffset(u32 offset) const
{
    SEAD_ASSERT(mLabelBlockBody);
    return mLabelBlockBody->GetLabelByOffset(offset);
}

void SequenceSoundFileReader::createLabelCache()
{
    mLabelCache.clear();

    for (s32 i = 0; i < GetLabelCount(); i++)
    {
        const SequenceSoundFile::LabelInfo* labelInfo = mLabelBlockBody->GetLabelInfo(i);
        SEAD_ASSERT(labelInfo);

        u32 offset = labelInfo->referToSequenceData.offset;
        const char* label = labelInfo->label;

        mLabelCache[offset].emplace_back(label);
    }
}

const char* SequenceSoundFileReader::getLabelByOffsetFromCache(u32 offset)
{
    const auto& it = mLabelCache.find(offset);
    if (it != mLabelCache.end())
        return it->second.front().c_str();

    return mLabelCache[offset].emplace_back(std::format("_local_{:d}", offset)).c_str();
}

} } } // namespace nw::snd::internal
