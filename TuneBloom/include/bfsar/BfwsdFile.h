#pragma once

#include <bfsar/InnerFile.h>

#include <unordered_map>

class SoundSet;
class WaveArchive;
class WaveFile;

class BfwsdFile : public InnerFile
{
    SEAD_RTTI_OVERRIDE(BfwsdFile, InnerFile);

public:
    BfwsdFile(sead::Endian::Types endian, u32 version)
        : InnerFile()
        , mSoundSet(nullptr)
        , mWaveArchive(nullptr)
        , mWaveArchiveWaveFilesIndexes(nullptr)
        , mUpdateWriteInfo(true)
    {
        mEndian = endian;
        mVersion = version;
    }

    void prepare(const SoundSet* soundSet, const WaveArchive* warc, const std::unordered_map<const WaveArchive*, std::unordered_map<const WaveFile*, u32>>& waveFilesIndexes, bool updateWriteInfo) const
    {
        SEAD_ASSERT(!mSoundSet);
        mSoundSet = soundSet;

        SEAD_ASSERT(!mWaveArchive);
        mWaveArchive = warc;

        const auto& it = waveFilesIndexes.find(warc);
        SEAD_ASSERT(it != waveFilesIndexes.end());

        SEAD_ASSERT(!mWaveArchiveWaveFilesIndexes);
        mWaveArchiveWaveFilesIndexes = &it->second;

        mUpdateWriteInfo = updateWriteInfo;
    }

    static bool isFilterSupportedVersion(u32 version)
    {
        return version >= 0x00010100;
    }

private:
    void doRead(const void* fileAddr) override
    {
    }

    u32 doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const override;

    bool updateWriteInfo_() const override
    {
        return mUpdateWriteInfo;
    }

private:
    mutable const SoundSet* mSoundSet;
    mutable const WaveArchive* mWaveArchive;
    mutable const std::unordered_map<const WaveFile*, u32>* mWaveArchiveWaveFilesIndexes;
    mutable bool mUpdateWriteInfo;
};
