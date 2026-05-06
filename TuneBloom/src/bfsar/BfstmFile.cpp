#include <bfsar/Sound.h>

#include <bfsar/writer/FileWriter.h>

#include <ui/PopupMgr.h>
#include <ui/UI.h>

#include <snd/snd_StreamSoundFileReader.h>

#include <snd/DecodeAdpcm.h>
#include <snd/StreamSoundPlayer.h>

#include <filedevice/seadFileDevice.h>
#include <stream/seadFileDeviceStream.h>

static bool IsTrackInfoAvailable(u32 version)
{
    return version <= 0x00020000;
}

static bool IsOriginalLoopAvailable(u32 version)
{
    return version >= 0x00040000;
}

bool WriteBfstmFile(sead::FileHandle& handle, const Sound::StreamSoundInfo& soundInfo, u32 version, sead::Endian::Types endian)
{
    sead::FileDeviceWriteStream stream(&handle, sead::Stream::Modes::eBinary);
    stream.setBinaryEndian(endian);

    SEAD_ASSERT(soundInfo.getTrackList().size() > 0);

    struct ChannelInfo
    {
        ChannelInfo()
            : buffer(nullptr), shouldDelete(false)
        {
        }

        ~ChannelInfo()
        {
            if (shouldDelete)
            {
                delete[] buffer;
            }
        }

        u8* buffer;
        bool shouldDelete;
        snd::DspAdpcmParam adpcmParam;
        snd::internal::DspAdpcmLoopParam adpcmLoopParam;
        WaveFile::Channel::SeekData seekData;
    };

    u32 channelNum = 0;
    const WaveFile::Channel* channels[cStrmChannelNum];
    const WaveFile* channelOwners[cStrmChannelNum];
    ChannelInfo channelInfos[cStrmChannelNum];
    for (u32 i = 0; i < cStrmChannelNum; i++)
    {
        channels[i] = nullptr;
        channelOwners[i] = nullptr;
    }

    u32 trackNum = soundInfo.getTrackList().size();
    const Sound::StreamSoundInfo::Track* tracks[cStrmTrackNum];
    for (u32 i = 0; i < cStrmTrackNum; i++)
    {
        if (i < trackNum)
        {
            const Sound::StreamSoundInfo::Track* track = static_cast<const Sound::StreamSoundInfo::Track*>(soundInfo.getTrackList().nth(i)->val());
            tracks[i] = track;

            SEAD_ASSERT(track->getWaveFileRef().isAttached());
            const WaveFile& wave = *static_cast<const WaveFile*>(track->getWaveFileRef().getItem());
            for (s32 ch = 0; ch < wave.getChannels().size(); ch++)
            {
                channels[channelNum] = wave.getChannels().nth(ch);
                channelOwners[channelNum] = &wave;
                channelNum++;
            }
        }
        else
        {
            tracks[i] = nullptr;
        }
    }

    SEAD_ASSERT(channelNum == soundInfo.getAllocateChannelCount());

    SEAD_ASSERT(tracks[0]->getWaveFileRef().isAttached());
    const WaveFile& mainWave = *static_cast<const WaveFile*>(tracks[0]->getWaveFileRef().getItem());

    nw::snd::SampleFormat sampleFormat = nw::snd::internal::WaveFileReader::GetSampleFormat(static_cast<u8>(mainWave.getEncoding()));

    const u32 cDefaultBytesPerBlock = 0x2000;
    const u32 cDefaultBytesPerSeekTableEntry = 2 * sizeof(s16);

    u32 loopStartFrame = mainWave.getLoopStartFrame(true);
    u32 loopEndFrame = mainWave.getLoopEndFrame(true);
    bool isLoop = mainWave.getIsLoop();
    bool isDspAdpcm = mainWave.getEncoding() == WaveFile::Encoding::DspAdpcm;

    u32 sampleCount = loopEndFrame;
    u32 samplePerBlock = nw::snd::internal::Util::GetSampleByByte(cDefaultBytesPerBlock, sampleFormat);
    u32 blockNum = sampleCount / samplePerBlock + (sampleCount % samplePerBlock != 0); // Divide sampleCount by samplePerBlock and ceil

    u32 lastBlockSamples = sampleCount - ((blockNum - 1) * samplePerBlock);
    u32 lastBlockBytes = nw::snd::internal::Util::GetByteBySample(lastBlockSamples, sampleFormat);
    u32 lastBlockBytesPadded = sead::Mathu::roundUpPow2(lastBlockBytes, 0x20);

    {
        u32 mainWaveSize = channels[0]->getDataSize();

        for (u32 i = 0; i < channelNum; i++)
        {
            const WaveFile& currentWave = *channelOwners[i];
            const WaveFile::Channel& channel = *channels[i];
            ChannelInfo& channelInfo = channelInfos[i];

            auto convertEndian = [&](sead::Endian::Types dataEndian)
            {
                if (dataEndian != endian)
                {
                    s16* samples = (s16*)channelInfo.buffer;

                    for (u32 s = 0; s < sampleCount; s++)
                    {
                        samples[s] = sead::Endian::convertS16(dataEndian, endian, samples[s]);
                    }
                }
            };

            if (currentWave.getIsStreamExtended() &&
                currentWave.getLoopStartFrame(true) == loopStartFrame &&
                currentWave.getLoopEndFrame(true) == loopEndFrame)
            {
                channelInfo.buffer = new u8[mainWaveSize];
                channelInfo.shouldDelete = true;

                const void* src = channel.getData();
                u8* dst = channelInfo.buffer;

                sead::MemUtil::copy(dst, src, mainWaveSize);

                if (isDspAdpcm)
                {
                    channelInfo.adpcmParam = channel.getAdpcmParam(true);
                    channelInfo.adpcmLoopParam = channel.getAdpcmLoopParam(true);
                    channelInfo.seekData.mSeekInfo.setBuffer(
                        channel.getSeekData().mSeekInfo.size(),
                        const_cast<WaveFile::Channel::SeekInfo*>(channel.getSeekData().mSeekInfo.getBufferPtr())
                    );
                    channelInfo.seekData.mOwner = false;
                }

                if (currentWave.getEncoding() == WaveFile::Encoding::Pcm16)
                {
                    sead::Endian::Types dataEndian = currentWave.getDataEndian();
                    convertEndian(dataEndian);
                }
            }
            else
            {
                channelInfo.buffer = (u8*)WaveFile::convertChannel_(
                    const_cast<WaveFile::Channel&>(channel), channel.getData(), currentWave.getDataEndian(),
                    currentWave.getEncoding(), currentWave.getEncoding(), nullptr, false, isLoop,
                    currentWave.getSampleCount(), mainWave.getOriginalLoopEndFrame(),
                    mainWave.getOriginalLoopStartFrame(), mainWave.getOriginalLoopEndFrame(),
                    0, 0,
                    loopStartFrame, loopEndFrame,
                    nullptr, nullptr,
                    &channelInfo.adpcmParam, &channelInfo.adpcmLoopParam,
                    &channelInfo.seekData
                );
                channelInfo.shouldDelete = true;

                if (currentWave.getEncoding() == WaveFile::Encoding::Pcm16)
                {
                    sead::Endian::Types dataEndian = sead::Endian::getHostEndian();
                    convertEndian(dataEndian);
                }
            }
        }
    }

    bool hasSeekBlock = isDspAdpcm;
    bool hasRegionBlock = false; // TODO

    u32 fileBlockCount = 2;
    if (hasSeekBlock)
    {
        fileBlockCount++;
    }

    if (hasRegionBlock)
    {
        fileBlockCount++;
    }

    FileWriter writer(&handle, &stream);
    writer.openFile("FSTM", fileBlockCount, version);

    //? Info Block
    {
        writer.openBlock(nw::snd::internal::ElementType_StreamSoundFile_InfoBlock, "INFO");

        writer.openReference("StreamSoundInfoRef");
        writer.openReference("TrackInfoTableRef");
        writer.openReference("ChannelInfoTableRef");

        writer.closeReference("StreamSoundInfoRef", nw::snd::internal::ElementType_StreamSoundFile_StreamSoundInfo);
        writer.pushOffsetBase();
        {
            stream.writeU8(static_cast<u8>(mainWave.getEncoding()));
            stream.writeU8(isLoop);
            stream.writeU8(static_cast<u8>(channelNum));
            stream.writeU8(0); // TODO: ? Region count
            stream.writeU32(mainWave.getSampleRate());
            stream.writeU32(loopStartFrame);
            stream.writeU32(sampleCount);
            stream.writeU32(blockNum);
            stream.writeU32(cDefaultBytesPerBlock);
            stream.writeU32(samplePerBlock);
            stream.writeU32(lastBlockBytes);
            stream.writeU32(lastBlockSamples);
            stream.writeU32(lastBlockBytesPadded);
            stream.writeU32(cDefaultBytesPerSeekTableEntry);
            stream.writeU32(samplePerBlock);

            writer.openReference("SampleDataRef");

            stream.writeU16(sizeof(nw::snd::internal::StreamSoundFile::RegionInfo));
            stream.writeU16(0); // Padding

            writer.openReference("RegionDataRef");
            if (!hasRegionBlock)
            {
                writer.closeNullReference("RegionDataRef");
            }

            if (IsOriginalLoopAvailable(version))
            {
                stream.writeU32(mainWave.getOriginalLoopStartFrame());
                stream.writeU32(mainWave.getOriginalLoopEndFrame());
            }
        }
        writer.popOffsetBase();

        if (IsTrackInfoAvailable(version))
        {
            writer.closeReference("TrackInfoTableRef", nw::snd::internal::ElementType_Table_ReferenceTable);
            writer.pushOffsetBase();
            {
                writer.openReferenceTable("TrackInfoTable", trackNum);

                u32 channelIdxStart = 0;
                for (u32 i = 0; i < trackNum; i++)
                {
                    writer.addReferenceTableReference("TrackInfoTable", nw::snd::internal::ElementType_StreamSoundFile_TrackInfo);

                    const Sound::StreamSoundInfo::Track& track = *tracks[i];

                    writer.pushOffsetBase();
                    {
                        stream.writeU8(track.getVolume());
                        stream.writeU8(track.getPan());
                        stream.writeU8(track.getSPan());
                        stream.writeU8(track.getFlags());

                        writer.openReference("GlobalChannelIndexTable");

                        writer.closeReference("GlobalChannelIndexTable", nw::snd::internal::ElementType_Table_EmbeddingTable);
                        stream.writeU32(track.getChannelCount());

                        for (u32 ch = 0; ch < track.getChannelCount(); ch++)
                        {
                            stream.writeU8(channelIdxStart + ch);
                        }

                        writer.align(0x4);
                    }
                    writer.popOffsetBase();

                    channelIdxStart += track.getChannelCount();
                }

                writer.closeReferenceTable("TrackInfoTable");
            }
            writer.popOffsetBase();
        }
        else
        {
            writer.closeNullReference("TrackInfoTableRef");
        }

        writer.closeReference("ChannelInfoTableRef", nw::snd::internal::ElementType_Table_ReferenceTable);
        writer.pushOffsetBase();
        {
            writer.openReferenceTable("ChannelInfoTable", channelNum);

            for (u32 i = 0; i < channelNum; i++)
            {
                writer.addReferenceTableReference("ChannelInfoTable", nw::snd::internal::ElementType_StreamSoundFile_ChannelInfo);

                writer.pushOffsetBase();
                {
                    writer.openReference(sead::FormatFixedSafeString<32>("DetailChannelInfoRef%i", i));
                }
                writer.popOffsetBase();
            }

            for (u32 i = 0; i < channelNum; i++)
            {
                sead::FormatFixedSafeString<32> refName("DetailChannelInfoRef%i", i);
                if (isDspAdpcm)
                {
                    writer.closeReference(refName, nw::snd::internal::ElementType_Codec_DspAdpcmInfo);

                    const ChannelInfo& channelInfo = channelInfos[i];

                    {
                        const snd::DspAdpcmParam& adpcmParam = channelInfo.adpcmParam;

                        for (u32 j = 0; j < 8; j++)
                        {
                            stream.writeU16(adpcmParam.coef[j][0]);
                            stream.writeU16(adpcmParam.coef[j][1]);
                        }

                        stream.writeU16(adpcmParam.predScale);
                        stream.writeU16(adpcmParam.yn1);
                        stream.writeU16(adpcmParam.yn2);
                    }

                    {
                        const snd::internal::DspAdpcmLoopParam& adpcmLoopParam = channelInfo.adpcmLoopParam;

                        stream.writeU16(adpcmLoopParam.loopPredScale);
                        stream.writeU16(adpcmLoopParam.loopYn1);
                        stream.writeU16(adpcmLoopParam.loopYn2);
                        stream.writeU16(0); // Padding
                    }
                }
                else
                {
                    writer.closeNullReference(refName);
                }
            }

            writer.closeReferenceTable("ChannelInfoTable");
        }
        writer.popOffsetBase();

        writer.align(0x20);
        writer.closeBlock();
    }

    //? Seek Block
    if (hasSeekBlock)
    {
        writer.openBlock(nw::snd::internal::ElementType_StreamSoundFile_SeekBlock, "SEEK");

        for (u32 blockNo = 0; blockNo < blockNum; blockNo++)
        {
            for (u32 ch = 0; ch < channelNum; ch++)
            {
                const ChannelInfo& channelInfo = channelInfos[ch];

                s16 s1 = sead::Endian::fromHostS16(sead::Endian::eLittle, channelInfo.seekData.getSeekInfo(blockNo).yn1);
                s16 s2 = sead::Endian::fromHostS16(sead::Endian::eLittle, channelInfo.seekData.getSeekInfo(blockNo).yn2);

                stream.writeMemBlock(&s1, sizeof(s16));
                stream.writeMemBlock(&s2, sizeof(s16));
            }
        }

        writer.align(0x20);
        writer.closeBlock();
    }

    //? Region Block
    if (hasRegionBlock)
    {
        writer.openBlock(nw::snd::internal::ElementType_StreamSoundFile_RegionBlock, "REGN");

        writer.closeReference("RegionDataRef", nw::snd::internal::ElementType_General_ByteStream);

        // TODO

        writer.align(0x20);
        writer.closeBlock();
    }

    //? Data Block
    {
        writer.openBlock(nw::snd::internal::ElementType_StreamSoundFile_DataBlock, "DATA");

        u32 dataBlockPos = writer.getPosition();
        writer.align(0x20);

        writer.closeReference("SampleDataRef", nw::snd::internal::ElementType_General_ByteStream, writer.getPosition() - dataBlockPos);

        //? Interleave channel blocks
        for (u32 blockNo = 0; blockNo < blockNum; blockNo++)
        {
            for (u32 ch = 0; ch < channelNum; ch++)
            {
                // u32 pos = writer.getPosition();

                const ChannelInfo& channelInfo = channelInfos[ch];

                bool isLastBlock = blockNo == blockNum - 1;
                u32 offset = blockNo * cDefaultBytesPerBlock;
                u32 blockBytes = isLastBlock ? lastBlockBytesPadded : cDefaultBytesPerBlock;

                stream.writeMemBlock(channelInfo.buffer + offset, blockBytes);

                //? Debug
                // {
                //     u32 prevPos = writer.getPosition();
                //     writer.seek(pos);
                //     stream.writeU32(blockNo + 1);
                //     writer.seek(prevPos);
                // }
            }
        }

        writer.closeBlock();
    }

    writer.closeFile();

    return true;
}

bool ReadStreamWaves(Sound* sound, const void* strmFile)
{
    Sound::StreamSoundInfo::Track::List& tracks = sound->getStreamSoundInfo().getTrackList();

    if (tracks.isEmpty())
    {
        return false;
    }

    nw::snd::internal::StreamSoundFileReader reader;
    reader.Initialize(strmFile);

    if (!reader.IsAvailable())
    {
        return false;
    }

    nw::snd::internal::StreamSoundFile::StreamSoundInfo streamSoundInfo;
    if (!reader.ReadStreamSoundInfo(&streamSoundInfo))
    {
        return false;
    }

    bool channelBuffersUsed[cStrmChannelNum];
    u8* channelBuffers[cStrmChannelNum];
    for (u32 i = 0; i < cStrmChannelNum; i++)
    {
        channelBuffersUsed[i] = false;
        channelBuffers[i] = nullptr;
    }

    WaveFile::Encoding encoding = static_cast<WaveFile::Encoding>(streamSoundInfo.encodeMethod);

    u32 channelSize = streamSoundInfo.oneBlockBytes * (streamSoundInfo.blockCount - 1) + streamSoundInfo.lastBlockPaddedBytes;

    for (u32 i = 0; i < streamSoundInfo.channelCount; i++)
    {
        channelBuffers[i] = new u8[channelSize];
    }

    const u8* sampleData = (u8*)strmFile + reader.GetSampleDataOffset();

    for (u32 blockNo = 0; blockNo < streamSoundInfo.blockCount; blockNo++)
    {
        for (u32 channelNo = 0; channelNo < streamSoundInfo.channelCount; channelNo++)
        {
            bool isLastBlock = blockNo == streamSoundInfo.blockCount - 1;
            u32 blockBytes = isLastBlock ? streamSoundInfo.lastBlockPaddedBytes : streamSoundInfo.oneBlockBytes;

            const void* src = sampleData;
            sampleData += blockBytes;

            void* dst = channelBuffers[channelNo] + (blockNo * streamSoundInfo.oneBlockBytes);

            sead::MemUtil::copy(dst, src, blockBytes);
        }
    }

    for (u32 trackNo = 0; trackNo < tracks.size(); trackNo++)
    {
        Sound::StreamSoundInfo::Track& track = *static_cast<Sound::StreamSoundInfo::Track*>(tracks.nth(trackNo)->val());

        WaveFile* wave = new WaveFile();
        wave->mId = sBfsar.getWaveFileList().size();

        wave->mEnableName = true;
        if (tracks.size() == 1)
        {
            wave->mName.format("GUESS_%s", sound->getName().cstr());
        }
        else
        {
            wave->mName.format("GUESS_%s_TRACK_%u", sound->getName().cstr(), trackNo);
        }

        wave->mVersion = sBfsar.getVersionForBfwav();
        wave->mDataEndian = nw::ut::GetFileEndian(*reader.mHeader);
        wave->mEncoding = encoding;
        wave->mIsLoop = streamSoundInfo.isLoop;
        wave->mSampleRate = streamSoundInfo.sampleRate;
        wave->mLoopStartFrame = streamSoundInfo.originalLoopStart;
        wave->mLoopEndFrame = streamSoundInfo.originalLoopEnd;
        if (wave->getLoopStartFrame(true) != streamSoundInfo.loopStart)
        {
            sead::FormatFixedSafeString<1024> msg("Track %u has invalid loop start (%u should be %u)", trackNo, wave->getLoopStartFrame(true), (u32)streamSoundInfo.loopStart);
            PopupMgr::instance()->pushCurrentItemError(msg);
        }
        if (wave->getLoopEndFrame(true) != streamSoundInfo.frameCount)
        {
            sead::FormatFixedSafeString<1024> msg("Track %u has invalid loop end (%u should be %u)", trackNo, wave->getLoopEndFrame(true), (u32)streamSoundInfo.frameCount);
            PopupMgr::instance()->pushCurrentItemError(msg);
        }

        wave->mSampleCount = wave->mLoopEndFrame;

        wave->mUseOriginalData = false;

        for (s32 ch = 0; ch < track.getChannels_().size(); ch++)
        {
            if (ch >= snd::cWaveChannelMax)
            {
                break;
            }

            WaveFile::Channel* channel = wave->mChannels.birthBack();
            SEAD_ASSERT(channel);

            s8 globalChannelIndex = *track.getChannels_().nth(ch);
            SEAD_ASSERT(0 <= globalChannelIndex && globalChannelIndex < cStrmChannelNum);

            channel->mOwnsData = true;
            channel->mData = channelBuffers[globalChannelIndex];
            channel->mDataSize = channelSize;
            channel->mOriginalDataOffset = 0;

            channelBuffersUsed[globalChannelIndex] = true;

            if (wave->mEncoding == WaveFile::Encoding::DspAdpcm)
            {
                nw::snd::DspAdpcmParam adpcmParam;
                nw::snd::internal::DspAdpcmLoopParam adpcmLoopParam;
                reader.ReadDspAdpcmChannelInfo(&adpcmParam, &adpcmLoopParam, globalChannelIndex);

                for (u32 i = 0; i < 8; i++)
                {
                    channel->mAdpcmParam.coef[i][0] = adpcmParam.coef[i][0];
                    channel->mAdpcmParam.coef[i][1] = adpcmParam.coef[i][1];
                }

                channel->mAdpcmParam.predScale = adpcmParam.predScale;
                channel->mAdpcmParam.yn1 = adpcmParam.yn1;
                channel->mAdpcmParam.yn2 = adpcmParam.yn2;
                sead::MemUtil::copy(&channel->mAdpcmParamStream, &channel->mAdpcmParam, sizeof(snd::DspAdpcmParam));

                channel->mAdpcmLoopParam.loopPredScale = adpcmLoopParam.loopPredScale;
                channel->mAdpcmLoopParam.loopYn1 = adpcmLoopParam.loopYn1;
                channel->mAdpcmLoopParam.loopYn2 = adpcmLoopParam.loopYn2;
                sead::MemUtil::copy(&channel->mAdpcmLoopParamStream, &channel->mAdpcmLoopParam, sizeof(snd::internal::DspAdpcmLoopParam));

                channel->mSeekData.mSeekInfo.allocBuffer(streamSoundInfo.blockCount);
                channel->mSeekData.mOwner = true;

                WaveFile::Channel::SeekInfo* seekInfo = channel->mSeekData.mSeekInfo.getBufferPtr();
                for (u32 blockNo = 0; blockNo < streamSoundInfo.blockCount; blockNo++)
                {
                    const u8* seekStart = (u8*)strmFile + reader.GetSeekBlockOffset() + sizeof(nw::ut::BinaryBlockHeader);
                    const WaveFile::Channel::SeekInfo* blockSeek = (WaveFile::Channel::SeekInfo*)seekStart;
                    blockSeek += blockNo * streamSoundInfo.channelCount;
                    seekInfo[blockNo].yn1 = blockSeek[globalChannelIndex].yn1;
                    seekInfo[blockNo].yn2 = blockSeek[globalChannelIndex].yn2;
                }
            }
        }

        if (wave->mEncoding == WaveFile::Encoding::DspAdpcm)
        {
            wave->updateLoopInfo_(true, false); //? Update as the ones in the BFSTM can differ
        }

        wave->mIsLoopDirty = false;
        wave->mIsStreamExtended = true;

        sBfsar.getWaveFileList().pushBack(wave);

        track.getWaveFileRef().attach(wave);
    }

    for (u32 i = 0; i < cStrmChannelNum; i++)
    {
        if (!channelBuffersUsed[i] && channelBuffers[i])
        {
            delete[] channelBuffers[i];
            channelBuffers[i] = nullptr;
        }
    }

    return true;
}
