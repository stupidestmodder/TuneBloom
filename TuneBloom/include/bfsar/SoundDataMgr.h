#pragma once

#include <snd/snd_MemorySoundArchive.h>

#include <container/seadBuffer.h>

class SoundDataMgr
{
public:
    SoundDataMgr();

    void init(const nw::snd::MemorySoundArchive* arc, sead::Heap* heap);
    void deinit();

    const void* detail_GetFileAddress(nw::snd::SoundArchive::FileId fileId) const;

    const void* GetFileAddressFromSoundArchive(nw::snd::SoundArchive::FileId fileId) const;
    const void* GetFileAddressFromTable(nw::snd::SoundArchive::FileId fileId) const;

    bool SetFileAddressInGroupFile(const void* address);

    const void* SetFileAddressToTable(nw::snd::SoundArchive::FileId fileId, const void* address);

private:
    const nw::snd::MemorySoundArchive* mSoundArchive;
    sead::Buffer<const void*> mFileTable;
};
