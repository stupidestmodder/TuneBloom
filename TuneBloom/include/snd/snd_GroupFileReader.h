#pragma once

#include <snd/snd_GroupFile.h>

namespace nw { namespace snd { namespace internal {

struct GroupItemLocationInfo
{
    u32 fileId;
    const void* address;

    u32 fileSize; // Custom
};

class GroupFileReader
{
public:
    explicit GroupFileReader(const void* groupFile);

    u32 GetGroupItemCount() const { return mInfoBlockBody->GetGroupItemInfoCount(); }
    const nw::snd::internal::GroupFile::GroupItemInfo* GetGroupItemInfo(u32 index) const { return mInfoBlockBody->GetGroupItemInfo(index); }
    bool ReadGroupItemLocationInfo(GroupItemLocationInfo* out, u32 index) const;

    u32 GetGroupItemExCount() const
    {
        if (!mInfoExBlockBody)
            return 0;

        return mInfoExBlockBody->GetGroupItemInfoExCount();
    }

    const nw::snd::internal::GroupFile::GroupItemInfoEx* GetGroupItemInfoEx(u32 index)
    {
        if (!mInfoExBlockBody)
            return nullptr;

        return mInfoExBlockBody->GetGroupItemInfoEx(index);
    }

    bool ReadGroupItemInfoEx(GroupFile::GroupItemInfoEx* out, u32 index) const;

    const GroupFile::InfoBlockBody* mInfoBlockBody;
    const GroupFile::FileBlockBody* mFileBlockBody;
    const GroupFile::InfoExBlockBody* mInfoExBlockBody;
};

} } } // namespace nw::snd::internal
