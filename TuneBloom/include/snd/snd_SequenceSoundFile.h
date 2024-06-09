#pragma once

#include <snd/snd_ElementType.h>
#include <snd/snd_Util.h>

#include <prim/seadSafeString.h>

namespace nw { namespace snd { namespace internal {

struct SequenceSoundFile
{
    struct DataBlock;
    struct LabelBlock;

    struct FileHeader : public Util::SoundFileHeader
    {
        const DataBlock* GetDataBlock() const
        {
            return reinterpret_cast<const DataBlock*>(GetBlock(ElementType_SequenceSoundFile_DataBlock));
        }

        const LabelBlock* GetLabelBlock() const
        {
            return reinterpret_cast<const LabelBlock*>(GetBlock(ElementType_SequenceSoundFile_LabelBlock));
        }
    };

    struct DataBlockBody
    {
        u8 sequenceData[1];

        const void* GetSequenceData() const { return reinterpret_cast<const void*>(sequenceData); }
    };

    struct DataBlock
    {
        ut::BinaryBlockHeader header;
        DataBlockBody body;
    };

    struct LabelInfo;
    struct LabelBlockBody
    {
        Util::ReferenceTable labelInfoReferenceTable;

        inline s32 GetLabelCount() const { return labelInfoReferenceTable.count; }

        const LabelInfo* GetLabelInfo(s32 index) const
        {
            SEAD_ASSERT(index < GetLabelCount());

            return reinterpret_cast<const LabelInfo*>(labelInfoReferenceTable.GetReferedItem(index));
        }

        const char* GetLabel(s32 index) const
        {
            const LabelInfo* labelInfo = GetLabelInfo(index);
            SEAD_ASSERT(labelInfo);

            return labelInfo->label;
        }

        const char* GetLabelByOffset(u32 offset) const
        {
            for (s32 i = 0; i < GetLabelCount(); i++)
            {
                const LabelInfo* labelInfo = GetLabelInfo(i);
                SEAD_ASSERT(labelInfo);

                if (labelInfo->referToSequenceData.offset == static_cast<s32>(offset))
                    return labelInfo->label;
            }

            return nullptr;
        }

        bool GetOffset(s32 index, u32* offsetPtr) const
        {
            const LabelInfo* labelInfo = GetLabelInfo(index);
            SEAD_ASSERT(labelInfo);

            *offsetPtr = labelInfo->referToSequenceData.offset;
            return true;
        }

        bool GetOffsetByLabel(const char* label, u32* offsetPtr) const
        {
            SEAD_ASSERT(offsetPtr);

            const size_t labelLength = sead::SafeString(label).calcLength();

            for (s32 i = 0; i < GetLabelCount(); i++)
            {
                const LabelInfo* labelInfo = GetLabelInfo(i);
                SEAD_ASSERT(labelInfo);

                if (std::strncmp(label, labelInfo->label, labelLength) == 0)
                {
                    *offsetPtr = labelInfo->referToSequenceData.offset;
                    return true;
                }
            }

            return false;
        }
    };

    struct LabelBlock
    {
        ut::BinaryBlockHeader header;
        LabelBlockBody body;
    };

    struct LabelInfo
    {
        Util::Reference referToSequenceData;
        ut::ResU32 labelStringLength;
        char label[1];
    };
};

} } } // namespace nw::snd::internal
