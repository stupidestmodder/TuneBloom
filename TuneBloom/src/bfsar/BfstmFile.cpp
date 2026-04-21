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

bool WriteBfstmFile(sead::FileHandle& handle, const Sound::StreamSoundInfo& soundInfo)
{
    sead::FileDeviceWriteStream stream(&handle, sead::Stream::Modes::eBinary);
    stream.setBinaryEndian(sBfsar.getEndian());

    SEAD_ASSERT(soundInfo.getTrackList().size() > 0);

    u32 channelNum = 0;
    const WaveFile::Channel* channels[cStrmChannelNum];
    const WaveFile* channelOwners[cStrmChannelNum];
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

    nw::snd::SampleFormat sampleFormat = static_cast<nw::snd::SampleFormat>(mainWave.getEncoding());

    const u32 cDefaultBytesPerBlock = 0x2000;
    const u32 cDefaultBytesPerSeekTableEntry = 2 * sizeof(s16);
    //const u32 cDefaultSamplesPerSeekTableEntry = nw::snd::internal::Util::GetSampleByByte(cDefaultBytesPerBlock, sampleFormat);

    u32 sampleCount = mainWave.getLoopEndFrame(true);
    u32 samplePerBlock = nw::snd::internal::Util::GetSampleByByte(cDefaultBytesPerBlock, sampleFormat);
    u32 blockNum = sampleCount / samplePerBlock + (sampleCount % samplePerBlock != 0); // Divide sampleCount by samplePerBlock and ceil

    u32 lastBlockSamples = sampleCount - ((blockNum - 1) * samplePerBlock);
    u32 lastBlockBytes = nw::snd::internal::Util::GetByteBySample(lastBlockSamples, sampleFormat);
    u32 lastBlockBytesPadded = sead::Mathu::roundUpPow2(lastBlockBytes, 0x20);

    u32 version = sBfsar.getVersionForBfstm();
    bool hasSeekBlock = mainWave.getEncoding() == WaveFile::Encoding::DspAdpcm;
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
            stream.writeU8(mainWave.getIsLoop());
            stream.writeU8(static_cast<u8>(channelNum));
            stream.writeU8(0); // TODO: ? Region count
            stream.writeU32(mainWave.getSampleRate());
            stream.writeU32(mainWave.getLoopStartFrame(true));
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
                if (mainWave.getEncoding() == WaveFile::Encoding::DspAdpcm)
                {
                    writer.closeReference(refName, nw::snd::internal::ElementType_Codec_DspAdpcmInfo);

                    const WaveFile::Channel& channel = *channels[i];

                    {
                        const snd::DspAdpcmParam& adpcmParam = channel.getAdpcmParam(true);

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
                        const snd::internal::DspAdpcmLoopParam& adpcmLoopParam = channel.getAdpcmLoopParam(true);

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
                const WaveFile::Channel& channel = *channels[ch];
                if (channel.getSeekInfoBlocks() == 0)
                {
                    // TODO: This needs to be spooled

                    WaveFile* currentWave = (WaveFile*)channelOwners[ch];

                    u32 samplesPcm = currentWave->getLoopEndFrame(true);

                    ADPCMINFO adpcmInfo;
                    sead::MemUtil::fillZero(&adpcmInfo, sizeof(adpcmInfo));
                    FillAdpcmInfo(&adpcmInfo, channel.getAdpcmParam(true), channel.getAdpcmLoopParam(true));

                    s16* samples = new s16[samplesPcm];
                    decode((u8*)channel.getData(), samples, &adpcmInfo, samplesPcm);

                    WaveFile::buildSeekTable_(samples, samplesPcm, snd::SampleFormat::PcmS16, (WaveFile::Channel&)channel);

                    delete[] samples;
                }

                s16 s1 = sead::Endian::fromHostS16(sead::Endian::eLittle, channel.getSeekInfo(blockNo).yn1);
                s16 s2 = sead::Endian::fromHostS16(sead::Endian::eLittle, channel.getSeekInfo(blockNo).yn2);

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

                const WaveFile::Channel& channel = *channels[ch];
                const WaveFile& currentWave = *channelOwners[ch];

                u32 offset = blockNo * cDefaultBytesPerBlock;

                u32 dataSize = nw::snd::internal::Util::GetByteBySample(currentWave.getLoopEndFrame(false), sampleFormat);
                // u32 dataSize = channel.getDataSize();
                const u8* data = static_cast<const u8*>(channel.getData()) + offset;

                bool isLastBlock = blockNo == blockNum - 1;

              //u32 blockBytes = isLastBlock ? lastBlockBytes : cDefaultBytesPerBlock;
                u32 blockBytes = isLastBlock ? lastBlockBytesPadded : cDefaultBytesPerBlock; //? Write padded last block to account for random bytes at the end ?
                u32 blockBytesPadded = isLastBlock ? lastBlockBytesPadded : cDefaultBytesPerBlock;

                // Calculate the remaining data size from the current offset
                u32 remainingDataSize = dataSize > offset ? dataSize - offset : 0;

                // Write the smaller of the block size or the remaining data
                u32 writeBytes = sead::Mathu::min(blockBytes, remainingDataSize);

                stream.writeMemBlock(data, writeBytes);

                // Handle padding if necessary
                if (writeBytes < blockBytesPadded)
                {
                    static const u8 cPad[cDefaultBytesPerBlock] = { 0 };

                    u32 padSize = blockBytesPadded - writeBytes;

                    stream.writeMemBlock(cPad, padSize);
                }

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
    u32 sampleSize = encoding == WaveFile::Encoding::Pcm16 ? sizeof(s16) : sizeof(u8);

    //u32 channelSize = streamSoundInfo.frameCount * sampleSize;
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

                WaveFile::Channel::SeekInfo* seekInfo = new WaveFile::Channel::SeekInfo[streamSoundInfo.blockCount];
                channel->mSeekInfo = seekInfo;
                channel->mSeekInfoBlocks = streamSoundInfo.blockCount;
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

        if (encoding == WaveFile::Encoding::DspAdpcm)
        {
            wave->updateLoopInfo_(true, false); //? Update as the ones in the BFSTM can differ
        }

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
