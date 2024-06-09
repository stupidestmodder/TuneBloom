#pragma once

#include <snd/snd_WaveArchiveFile.h>

namespace nw { namespace snd { namespace internal {

class WaveArchiveFileReader
{
public:
    static const u32 SIGNATURE_WARC_TABLE;

    WaveArchiveFileReader();
    WaveArchiveFileReader(const void* pWaveArchiveFile, bool isIndividual = false);

    void Initialize(const void* warcFile, bool isIndividual = false);
    void Finalize();

    void InitializeFileTable();
    bool IsAvailable() const { return mHeader != nullptr; }

    u32 GetWaveFileCount() const;
    u32 GetWaveFileSize(u32 waveIndex) const;
    u32 GetWaveFileOffsetFromFileHead(u32 waveIndex) const;

    // When batch loading.
    const void* GetWaveFile(u32 waveIndex) const;

    // Use when loading individually.
    const void* SetWaveFile(u32 waveIndex, const void* pWaveFile);
    bool IsLoaded(u32 waveIndex) const
    {
        if (!mIsInitialized)
            return false;

        if (GetWaveFile(waveIndex))
            return true;

        return false;
    }

    bool HasIndividualLoadTable() const;

    struct IndividualLoadTable
    {
        const void* waveFile[1];
    };

    const void* GetWaveFileForWhole(u32 waveIndex) const
    {
        u32 offset = mInfoBlockBody->GetOffsetFromFileBlockBody(waveIndex);
        return sead::PtrUtil::addOffset(&mHeader->GetFileBlock()->body, offset);
    }

    const void* GetWaveFileForIndividual(u32 waveIndex) const
    {
        SEAD_ASSERT(mLoadTable);
        return mLoadTable->waveFile[waveIndex];
    }

    const WaveArchiveFile::FileHeader* mHeader;
    const WaveArchiveFile::InfoBlockBody* mInfoBlockBody;
    IndividualLoadTable* mLoadTable;
    bool mIsInitialized;
};

} } } // namespace nw::snd::internal
