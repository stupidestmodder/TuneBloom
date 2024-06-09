#pragma once

#include <snd/snd_SequenceSoundFile.h>

#include <unordered_map>
#include <vector>
#include <string>

namespace nw { namespace snd { namespace internal {

class SequenceSoundFileReader
{
public:
    explicit SequenceSoundFileReader(const void* sequenceFile);

    bool IsAvailable() const { return mHeader != nullptr; }

    const void* GetSequenceData() const;
    bool GetOffsetByLabel(const char* label, u32* offsetPtr) const;
    const char* GetLabelByOffset(u32 offset) const;

    inline s32 GetLabelCount() const
    {
        return mLabelBlockBody->GetLabelCount();
    }

    inline const char* GetLabel(s32 index) const
    {
        return mLabelBlockBody->GetLabel(index);
    }

    void createLabelCache();
    const char* getLabelByOffsetFromCache(u32 offset);

    const SequenceSoundFile::FileHeader* mHeader;
    const SequenceSoundFile::DataBlockBody* mDataBlockBody;
    const SequenceSoundFile::LabelBlockBody* mLabelBlockBody;

    std::unordered_map<u32, std::vector<std::string>> mLabelCache;
};

} } } // namespace nw::snd::internal
