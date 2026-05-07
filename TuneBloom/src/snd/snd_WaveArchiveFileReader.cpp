#include <snd/snd_WaveArchiveFileReader.h>

#include <basis/seadRawPrint.h>
#include <prim/seadMemUtil.h>

#include <ui/PopupMgr.h>

#define NW_SND_DEBUG_PRINT_ENABLE

namespace nw { namespace snd { namespace internal {

WaveArchiveFileReader::WaveArchiveFileReader()
    : mHeader(nullptr)
    , mInfoBlockBody(nullptr)
    , mLoadTable(nullptr)
    , mIsInitialized(false)
{
}

WaveArchiveFileReader::WaveArchiveFileReader(const void* pWaveArchiveFile, bool isIndividual)
    : mHeader(nullptr)
    , mInfoBlockBody(nullptr)
    , mLoadTable(nullptr)
    , mIsInitialized(false)
{
    Initialize(pWaveArchiveFile, isIndividual);
}

void WaveArchiveFileReader::Initialize(const void* pWaveArchiveFile, bool isIndividual)
{
    if (pWaveArchiveFile == nullptr)
    {
        PopupMgr::instance()->pushCurrentItemError("No file");
        return;
    }

    {
        const ut::BinaryFileHeader* header = reinterpret_cast<const ut::BinaryFileHeader*>(pWaveArchiveFile);

        // if (sead::MemUtil::compare(header->signature, "CWAR", 4) != 0)
        if (sead::MemUtil::compare(header->signature, "FWAR", 4) != 0)
        {
            PopupMgr::instance()->pushCurrentItemError("File is not a valid BFWAR");
            return;
        }

        // if (false)
        if (header->version != 0x00010000)
        {
            sead::FormatFixedSafeString<64> msg("BFWAR version not supported (0x%08X)", (u32)header->version);
            PopupMgr::instance()->pushCurrentItemError(msg);
            return;
        }
    }

    mHeader = reinterpret_cast<const WaveArchiveFile::FileHeader*>(pWaveArchiveFile);
    mInfoBlockBody = &mHeader->GetInfoBlock()->body;

    mIsInitialized = true;

    mLoadTable = nullptr;
    if (isIndividual && HasIndividualLoadTable())
    {
        mLoadTable = reinterpret_cast<IndividualLoadTable*>(
            sead::PtrUtil::addOffset(
                const_cast<void*>(pWaveArchiveFile),
                mHeader->GetFileBlockOffset() + sizeof(SIGNATURE_WARC_TABLE)
            )
        );
    }
}

void WaveArchiveFileReader::Finalize()
{
    if (mIsInitialized)
    {
        mHeader = nullptr;
        mInfoBlockBody = nullptr;
        mLoadTable = nullptr;
        mIsInitialized = false;
    }
}

void WaveArchiveFileReader::InitializeFileTable()
{
    SEAD_ASSERT(mInfoBlockBody);
    SEAD_ASSERT(mLoadTable);

    for (u32 i = 0; i < GetWaveFileCount(); i++)
    {
        mLoadTable->waveFile[i] = nullptr;
    }
}

u32 WaveArchiveFileReader::GetWaveFileCount() const
{
    if (!mIsInitialized)
        return 0;

    SEAD_ASSERT(mInfoBlockBody);
    return mInfoBlockBody->GetWaveFileCount();
}

u32 WaveArchiveFileReader::GetWaveFileSize(u32 waveIndex) const
{
    if (!mIsInitialized)
        return 0;

    SEAD_ASSERT(mInfoBlockBody);
    return mInfoBlockBody->GetSize(waveIndex);
}

u32 WaveArchiveFileReader::GetWaveFileOffsetFromFileHead(u32 waveIndex) const
{
    if (!mIsInitialized)
        return 0;

    SEAD_ASSERT(mInfoBlockBody);
    u32 result =
        + mHeader->GetFileBlockOffset()
        + offsetof(WaveArchiveFile::FileBlock, body)
        + mInfoBlockBody->GetOffsetFromFileBlockBody(waveIndex);

    return result;
}

const void* WaveArchiveFileReader::GetWaveFile(u32 waveIndex) const
{
    if (!mIsInitialized)
        return nullptr;

    SEAD_ASSERT(mInfoBlockBody);
    if (waveIndex >= GetWaveFileCount())
        return nullptr;

    if (mLoadTable)
        return GetWaveFileForIndividual(waveIndex);

    return GetWaveFileForWhole(waveIndex);
}

const void* WaveArchiveFileReader::SetWaveFile(u32 waveIndex, const void* pWaveFile)
{
    if (!mIsInitialized)
        return nullptr;

    if (!mLoadTable)
        return nullptr;

    if (waveIndex >= GetWaveFileCount())
        return nullptr;

    const void* preAddress = GetWaveFileForIndividual(waveIndex);
    mLoadTable->waveFile[waveIndex] = pWaveFile;

#ifdef NW_SND_DEBUG_PRINT_ENABLE
    for (u32 i = 0; i < GetWaveFileCount(); i++)
    {
        SEAD_PRINT("  [%3d] %p\n", i, mLoadTable->waveFile[i]);
    }
#endif // NW_SND_DEBUG_PRINT_ENABLE

    return preAddress;
}

bool WaveArchiveFileReader::HasIndividualLoadTable() const
{
    if (!mIsInitialized)
        return false;

    const u32* signature = reinterpret_cast<const u32*>(sead::PtrUtil::addOffset(mHeader, mHeader->GetFileBlockOffset()));
    //if (*signature == SIGNATURE_WARC_TABLE)
    //    return true;

    if (sead::MemUtil::compare(signature, "FWAT", 4) == 0)
        return true;

    return false;
}

} } } // namespace nw::snd::internal
