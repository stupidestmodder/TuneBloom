#include <snd/snd_WaveSoundFileReader.h>

#include <prim/seadMemUtil.h>

namespace nw { namespace snd { namespace internal {

WaveSoundFileReader::WaveSoundFileReader(const void* waveSoundFile)
    : mHeader(nullptr)
    , mInfoBlockBody(nullptr)
{
    SEAD_ASSERT(waveSoundFile);

    {
        const ut::BinaryFileHeader* header = reinterpret_cast<const ut::BinaryFileHeader*>(waveSoundFile);

        //if (sead::MemUtil::compare(header->signature, "CWSD", 4) != 0)
        if (sead::MemUtil::compare(header->signature, "FWSD", 4) != 0)
        {
            SEAD_ASSERT_MSG(false, "not a WAVE SOUND file");
            return;
        }

        //if (false)
        if (!(0x00010000 <= header->version && header->version <= 0x00010100))
        {
            SEAD_ASSERT_MSG(false, "WAVE SOUND version not supported (0x%08X)", (u32)header->version);
            return;
        }
    }

    mHeader = reinterpret_cast<const WaveSoundFile::FileHeader*>(waveSoundFile);

    const WaveSoundFile::InfoBlock* infoBlock = mHeader->GetInfoBlock();

    SEAD_ASSERT(infoBlock);
    if (!infoBlock)
        return;

    if (sead::MemUtil::compare(infoBlock->header.kind, "INFO", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "WAVE SOUND: INFO block is invalid");
        return;
    }

    mInfoBlockBody = &infoBlock->body;
}

u32 WaveSoundFileReader::GetWaveSoundCount() const
{
    SEAD_ASSERT(mInfoBlockBody);
    return mInfoBlockBody->GetWaveSoundCount();
}

u32 WaveSoundFileReader::GetNoteInfoCount(u32 index) const
{
    SEAD_ASSERT(mInfoBlockBody);
    SEAD_ASSERT(index < GetWaveSoundCount());

    const WaveSoundFile::WaveSoundData& wsdData = mInfoBlockBody->GetWaveSoundData(index);
    return wsdData.GetNoteCount();
}

u32 WaveSoundFileReader::GetTrackInfoCount(u32 index) const
{
    SEAD_ASSERT(mInfoBlockBody);
    SEAD_ASSERT(index < GetWaveSoundCount());

    const WaveSoundFile::WaveSoundData& wsdData = mInfoBlockBody->GetWaveSoundData(index);
    return wsdData.GetTrackCount();
}

bool WaveSoundFileReader::ReadWaveSoundInfo(WaveSoundInfo* dst, u32 index) const
{
    SEAD_ASSERT(mInfoBlockBody);
    SEAD_ASSERT(dst);

    const WaveSoundFile::WaveSoundInfo& src = mInfoBlockBody->GetWaveSoundData(index).GetWaveSoundInfo();

    dst->pitch = src.GetPitch();
    dst->pan = src.GetPan();
    dst->surroundPan = src.GetSurroundPan();
    src.GetSendValue(&dst->mainSend, dst->fxSend, AUX_BUS_NUM);
    dst->adshr = src.GetAdshrCurve();

    if (IsFilterSupportedVersion())
    {
        dst->lpfFreq = src.GetLpfFreq();
        dst->biquadType = src.GetBiquadType();
        dst->biquadValue = src.GetBiquadValue();
    }
    else
    {
        dst->lpfFreq = 64;
        dst->biquadType = 0;
        dst->biquadValue = 0;
    }

    return true;
}

bool WaveSoundFileReader::ReadNoteInfo(WaveSoundNoteInfo* dst, u32 index, u32 noteIndex) const
{
    SEAD_ASSERT(mInfoBlockBody);
    SEAD_ASSERT(dst);

    const WaveSoundFile::NoteInfo& src = mInfoBlockBody->GetWaveSoundData(index).GetNoteInfo(noteIndex);

    const Util::WaveId* pWaveId = mInfoBlockBody->GetWaveIdTable().GetWaveId(src.waveIdTableIndex);

    if (!pWaveId)
        return false;

    dst->waveArchiveId = pWaveId->waveArchiveId;
    dst->waveIndex = pWaveId->waveIndex;
    dst->pitch = src.GetPitch();
    dst->adshr = src.GetAdshrCurve();
    dst->originalKey = src.GetOriginalKey();
    dst->pan = src.GetPan();
    dst->surroundPan = src.GetSurroundPan();
    dst->volume = src.GetVolume();

    return true;
}

bool WaveSoundFileReader::IsFilterSupportedVersion() const
{
    const ut::BinaryFileHeader& header = *reinterpret_cast<const ut::BinaryFileHeader*>(mHeader);
    if (header.version >= 0x00010100)
        return true;

    return false;
}

} } } // namespace nw::snd::internal
