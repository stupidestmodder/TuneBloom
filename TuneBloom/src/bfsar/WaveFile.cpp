#include <bfsar/WaveFile.h>

#include <snd/DecodeAdpcm.h>
#include <snd/DisposeCallbackMgr.h>
#include <snd/SoundThread.h>

#include <snd/snd_WaveFileReader.h>

#include <ui/UI.h>

#include <filedevice/seadFileDeviceMgr.h>

const char* WaveFile::sEncodingTypes[3] = {
    "Pcm8",
    "Pcm16",
    "DspAdpcm",
    //"ImaAdpcm"
};

WaveFile::Channel::~Channel()
{
    {
        snd::internal::driver::SoundThreadLock lock;

        snd::internal::driver::DisposeCallbackMgr::instance()->dispose(mData, mDataSize);
    }

    if (mOwnsData && mData)
    {
        delete[] mData;
        mData = nullptr;
    }
}

WaveFile::~WaveFile()
{
    {
        snd::internal::driver::SoundThreadLock lock;

        mChannels.freeBuffer();
    }

    invalidateOriginalData_();
}

void WaveFile::drawUI()
{
    InnerFile::drawUI();

    ImGui::SeparatorText("");

    const ImU32 cStepU32 = 1;

    {
        u32 encoding = (u32)mEncoding;
        if (ImGui::Combo("Encoding", (s32*)&encoding, sEncodingTypes, IM_ARRAYSIZE(sEncodingTypes)))
        {
            if (encoding != (u32)mEncoding)
            {
                ImGui::OpenPopup("Encoding");
            }
        }
    }

    {
        bool isLoop = mIsLoop;
        if (ImGui::Checkbox("Is Loop", &isLoop))
        {
            mIsLoop = isLoop;
        }
    }

    {
        ImGui::BeginDisabled();

        u32 sampleRate = mSampleRate;
        if (ImGui::InputScalar("Sample Rate", ImGuiDataType_U32, &sampleRate, &cStepU32))
        {
            //mSampleRate = sampleRate;
        }

        ImGui::EndDisabled();
    }

    {
        u32 loopStartFrame = mLoopStartFrame;
        if (ImGui::InputScalar("Loop Start Frame", ImGuiDataType_U32, &loopStartFrame, &cStepU32))
        {
            if (loopStartFrame >= mLoopEndFrame)
            {
                loopStartFrame = mLoopEndFrame - 1;
            }

            mLoopStartFrame = loopStartFrame;
        }
    }

    {
        u32 loopEndFrame = mLoopEndFrame;
        if (ImGui::InputScalar("Loop End Frame", ImGuiDataType_U32, &loopEndFrame, &cStepU32))
        {
            if (loopEndFrame > mSampleCount)
            {
                loopEndFrame = mSampleCount;
            }
            else if (loopEndFrame < 1)
            {
                loopEndFrame = 1;
            }

            if (mLoopStartFrame >= loopEndFrame)
            {
                mLoopStartFrame = loopEndFrame - 1;
            }

            if (mOriginalLoopStartFrame >= loopEndFrame)
            {
                mOriginalLoopStartFrame = loopEndFrame - 1;
            }

            mLoopEndFrame = loopEndFrame;
        }
    }

    {
        bool enableOriginalLoop = isOriginalLoopAvailable();

        if (!enableOriginalLoop)
            ImGui::BeginDisabled();

        u32 originalLoopStartFrame = mOriginalLoopStartFrame;
        if (ImGui::InputScalar("Original Loop Start Frame", ImGuiDataType_U32, &originalLoopStartFrame, &cStepU32))
        {
            if (originalLoopStartFrame >= mLoopEndFrame)
            {
                originalLoopStartFrame = mLoopEndFrame - 1;
            }

            mOriginalLoopStartFrame = originalLoopStartFrame;
        }

        if (!enableOriginalLoop)
            ImGui::EndDisabled();
    }

    ImGui::SeparatorText("");

    ImGui::Text("Channels (%d) - %s", mChannels.size(), mChannels.size() == 1 ? "Mono" : "Stereo");

    if (ImGui::BeginChild("Channels", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border))
    {
        for (u32 i = 0; i < mChannels.size(); i++)
        {
            const Channel* channel = mChannels.nth(i);

            if (ImGui::Button(sead::FormatFixedSafeString<32>(ICON_LC_PLAY "###%u", i).cstr()))
            {
                PlayWaveFile(*this, i);
            }

            ImGui::SameLine();

            static const sead::SafeString sChannels[snd::cWaveChannelMax] = { "Left", "Right" };

            sead::FormatFixedSafeString<32> name("[%u] Channel", i);

            if (mChannels.size() > 1)
            {
                name.appendWithFormat(" - %s", sChannels[i].cstr());
            }

            if (ImGui::Selectable(name.cstr()))
            {
            }
        }
    }
    ImGui::EndChild();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Encoding", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::Text("So true...");
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().WindowPadding.x, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void WaveFile::doRead(const void* fileAddr)
{
    nw::snd::internal::WaveFileReader reader(fileAddr);

    nw::snd::internal::WaveInfo waveInfo;
    reader.ReadWaveInfo(&waveInfo);

    mDataEndian = waveInfo.endian;
    mEncoding = static_cast<Encoding>(reader.mInfoBlockBody->encoding);
    mIsLoop = waveInfo.loopFlag;
    mSampleRate = waveInfo.sampleRate;
    mLoopStartFrame = waveInfo.loopStartFrame;
    mLoopEndFrame = waveInfo.loopEndFrame;
    mOriginalLoopStartFrame = waveInfo.originalLoopStartFrame;

    mSampleCount = waveInfo.loopEndFrame;

    mUseOriginalData = true;
    mOriginalDataSize = reader.mHeader->GetDataBlock()->header.size - 0x20;
    mOriginalData = new u8[mOriginalDataSize];
    sead::MemUtil::copy(mOriginalData, waveInfo.channelParam[0].dataAddress, mOriginalDataSize);

    const void* dataBlockBody = sead::PtrUtil::addOffset(mOriginalData, -0x18);

    for (s32 i = 0; i < waveInfo.channelCount; i++)
    {
        if (i >= snd::cWaveChannelMax)
        {
            break;
        }

        const nw::snd::internal::WaveInfo::ChannelParam& channelParam = waveInfo.channelParam[i];

        Channel* channel = mChannels.birthBack();
        SEAD_ASSERT(channel);

        channel->mOwnsData = false;
        channel->mData = reader.mInfoBlockBody->GetChannelInfo(i).GetSamplesAddress(dataBlockBody);
        channel->mDataSize = nw::snd::internal::Util::GetByteBySample(mSampleCount, nw::snd::internal::WaveFileReader::GetSampleFormat((u8)mEncoding));
        channel->mOriginalDataOffset = reader.mInfoBlockBody->GetChannelInfo(i).referToSamples.offset;

        if (mEncoding == Encoding::DspAdpcm)
        {
            for (u32 i = 0; i < 8; i++)
            {
                channel->mAdpcmParam.coef[i][0] = channelParam.adpcmParam.coef[i][0];
                channel->mAdpcmParam.coef[i][1] = channelParam.adpcmParam.coef[i][1];
            }

            channel->mAdpcmParam.predScale = channelParam.adpcmParam.predScale;
            channel->mAdpcmParam.yn1 = channelParam.adpcmParam.yn1;
            channel->mAdpcmParam.yn2 = channelParam.adpcmParam.yn2;

            channel->mAdpcmLoopParam.loopPredScale = channelParam.adpcmLoopParam.loopPredScale;
            channel->mAdpcmLoopParam.loopYn1 = channelParam.adpcmLoopParam.loopYn1;
            channel->mAdpcmLoopParam.loopYn2 = channelParam.adpcmLoopParam.loopYn2;
        }
    }
}

u32 WaveFile::doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const
{
    FileWriter writer(handle, stream);
    writer.openFile("FWAV", 2, mVersion);

    //? Info Block
    {
        writer.openBlock(nw::snd::internal::ElementType_WaveFile_InfoBlock, "INFO");

        stream->writeU8((u8)mEncoding);
        stream->writeU8(mIsLoop);
        stream->writeU16(0); // Padding
        stream->writeU32(mSampleRate);
        stream->writeU32(mLoopStartFrame);
        stream->writeU32(mLoopEndFrame);
        stream->writeU32(isOriginalLoopAvailable() ? mOriginalLoopStartFrame : 0);

        writer.pushOffsetBase();
        {
            writer.openReferenceTable("ChannelInfoTable", mChannels.size());

            for (u32 i = 0; i < mChannels.size(); i++)
            {
                writer.addReferenceTableReference("ChannelInfoTable", nw::snd::internal::ElementType_WaveFile_ChannelInfo);

                writer.pushOffsetBase();
                {
                    writer.openReference(sead::FormatFixedSafeString<16>("Samples%u", i));
                    writer.openReference(sead::FormatFixedSafeString<16>("AdpcmInfo%u", i));

                    stream->writeU32(0); // Reserved
                }
                writer.popOffsetBase();
            }

            for (u32 i = 0; i < mChannels.size(); i++)
            {
                const Channel* channel = mChannels.nth(i);

                if (mEncoding == Encoding::DspAdpcm)
                {
                    writer.closeReference(sead::FormatFixedSafeString<16>("AdpcmInfo%u", i), nw::snd::internal::ElementType_Codec_DspAdpcmInfo);

                    {
                        const snd::DspAdpcmParam& adpcmParam = channel->getAdpcmParam();

                        for (u32 j = 0; j < 8; j++)
                        {
                            stream->writeU16(adpcmParam.coef[j][0]);
                            stream->writeU16(adpcmParam.coef[j][1]);
                        }

                        stream->writeU16(adpcmParam.predScale);
                        stream->writeU16(adpcmParam.yn1);
                        stream->writeU16(adpcmParam.yn2);
                    }

                    {
                        const snd::internal::DspAdpcmLoopParam& adpcmLoopParam = channel->getAdpcmLoopParam();

                        stream->writeU16(adpcmLoopParam.loopPredScale);
                        stream->writeU16(adpcmLoopParam.loopYn1);
                        stream->writeU16(adpcmLoopParam.loopYn2);
                        stream->writeU16(0); // Padding
                    }
                }
                else
                {
                    writer.closeNullReference(sead::FormatFixedSafeString<16>("AdpcmInfo%u", i));
                }
            }

            writer.closeReferenceTable("ChannelInfoTable");
        }
        writer.popOffsetBase();

        writer.align(0x20);
        writer.closeBlock();
    }

    //? Data Block
    {
        writer.openBlock(nw::snd::internal::ElementType_WaveFile_DataBlock, "DATA");

        u32 dataBlockBodyPos = writer.getPosition();
        writer.align(0x20);

        if (mUseOriginalData)
        {
            for (u32 i = 0; i < mChannels.size(); i++)
            {
                const Channel* channel = mChannels.nth(i);

                writer.closeReference(
                    sead::FormatFixedSafeString<16>("Samples%u", i),
                    nw::snd::internal::ElementType_General_ByteStream,
                    channel->mOriginalDataOffset
                );
            }

            stream->writeMemBlock(mOriginalData, mOriginalDataSize);
        }
        else
        {
            u32 sampleBytes = nw::snd::internal::Util::GetByteBySample(mSampleCount, nw::snd::internal::WaveFileReader::GetSampleFormat((u8)mEncoding));

            for (u32 i = 0; i < mChannels.size(); i++)
            {
                const Channel* channel = mChannels.nth(i);

                writer.align(0x20);

                writer.closeReference(
                    sead::FormatFixedSafeString<16>("Samples%u", i),
                    nw::snd::internal::ElementType_General_ByteStream,
                    writer.getPosition() - dataBlockBodyPos
                );

                switch (mEncoding)
                {
                    case Encoding::Pcm8:
                    case Encoding::DspAdpcm:
                        stream->writeMemBlock(channel->getData(), sampleBytes);
                        break;

                    case Encoding::Pcm16:
                        if (mDataEndian == mEndian)
                        {
                            stream->writeMemBlock(channel->getData(), sampleBytes);
                        }
                        else
                        {
                            const s16* data = static_cast<const s16*>(channel->getData());
                            s16* samples = new s16[mSampleCount];

                            for (u32 j = 0; j < mSampleCount; j++)
                            {
                                samples[j] = sead::Endian::convertS16(mDataEndian, mEndian, data[j]);
                            }

                            stream->writeMemBlock(samples, sampleBytes);

                            delete[] samples;
                        }

                        break;

                    default:
                        SEAD_ASSERT_MSG(false, "Invalid Encoding");
                        break;
                }
            }

            writer.align(0x8);
        }

        writer.closeBlock();
    }

    u32 fileSize = writer.getPosition();

    writer.closeFile();

    return fileSize;
}

bool WaveFile::readWavFile(const sead::SafeString& path, Encoding encoding)
{
    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
    SEAD_ASSERT(device);

    sead::FileHandle handle;
    device->tryOpen(&handle, path, sead::FileDevice::FileOpenFlag::eReadOnly, 0);

    if (!handle.getDevice())
    {
        return false;
    }

    sead::Endian::Types endian = sead::Endian::eLittle;

    sead::FileDeviceReadStream stream(&handle, sead::Stream::Modes::eBinary);
    stream.setBinaryEndian(endian);

    char riff[4];
    stream.readMemBlock(riff, 4);

    if (sead::MemUtil::compare(riff, "RIFF", 4) != 0)
    {
        return false;
    }

    stream.readU32(); // File Size

    char wave[4];
    stream.readMemBlock(wave, 4);

    if (sead::MemUtil::compare(wave, "WAVE", 4) != 0)
    {
        return false;
    }

    char fmt[4];
    stream.readMemBlock(fmt, 4);

    if (sead::MemUtil::compare(fmt, "fmt ", 4) != 0)
    {
        return false;
    }

    stream.readU32(); // Chunk Size

    u16 format = stream.readU16();
    if (format != 1)
    {
        return false;
    }

    u16 numChannels = stream.readU16();
    if (numChannels > snd::cWaveChannelMax)
    {
        return false;
    }

    u32 sampleRate = stream.readU32();

    stream.readU32(); // Byte Rate

    stream.readU16(); // Block Align

    u16 bitsPerSample = stream.readU16();
    if (bitsPerSample != 8 && bitsPerSample != 16)
    {
        return false;
    }

    char data[4];
    stream.readMemBlock(data, 4);

    if (sead::MemUtil::compare(data, "data", 4) != 0)
    {
        return false;
    }

    u32 sampleBytes = stream.readU32();

    snd::SampleFormat sampleFormat = bitsPerSample == 8 ? snd::SampleFormat::PcmS8 : snd::SampleFormat::PcmS16;
    u32 sampleCount = sampleBytes / numChannels / (bitsPerSample / 8);

    {
        snd::internal::driver::SoundThreadLock lock;

        mChannels.clear();
    }

    invalidateOriginalData_();

    u8* channels[snd::cWaveChannelMax] = { nullptr, nullptr };

    if (numChannels == 1)
    {
        u8* samples = new u8[sampleBytes];
        stream.readMemBlock(samples, sampleBytes);

        //? Convert from unsigned Pcm8 to signed Pcm8
        if (sampleFormat == snd::SampleFormat::PcmS8)
        {
            for (u32 i = 0; i < sampleCount; i++)
            {
                samples[i] = samples[i] - 128;
            }
        }

        channels[0] = samples;
    }
    else
    {
        for (u32 i = 0; i < numChannels; i++)
        {
            channels[i] = new u8[sampleBytes / numChannels];
        }

        u8* samples = new u8[sampleBytes];
        stream.readMemBlock(samples, sampleBytes);

        for (u32 i = 0; i < sampleCount; i++)
        {
            for (u32 ch = 0; ch < numChannels; ch++)
            {
                switch (sampleFormat)
                {
                    case snd::SampleFormat::PcmS8:
                    {
                        u8* channel = channels[ch];

                        channel[i] = samples[i * numChannels + ch];
                        channel[i] -= 128; //? Convert from unsigned Pcm8 to signed Pcm8
                        break;
                    }

                    case snd::SampleFormat::PcmS16:
                    {
                        s16* channel = (s16*)channels[ch];
                        s16* samples16 = (s16*)samples;

                        channel[i] = samples16[i * numChannels + ch];
                        break;
                    }
                }
            }
        }

        delete[] samples;
    }

    mDataEndian = endian;
    mEncoding = encoding;
    mSampleRate = sampleRate;
    mSampleCount = sampleCount;

    // TODO: Loop info ?
    mLoopStartFrame = 0;
    mOriginalLoopStartFrame = 0;
    mLoopEndFrame = mSampleCount;

    for (u32 i = 0; i < numChannels; i++)
    {
        Channel* channel = mChannels.birthBack();
        SEAD_ASSERT(channel);

        channel->mOwnsData = true;
        channel->mData = channels[i]; // TODO: Convert to Encoding
        channel->mDataSize = sampleBytes / numChannels;
    }

    return true;
}

bool WaveFile::writeWavFile(const sead::SafeString& path, s32 channelIdx)
{
    sead::FileDevice* device = sead::FileDeviceMgr::instance()->findDevice("native");
    SEAD_ASSERT(device);

    sead::FileHandle handle;
    device->tryOpen(&handle, path, sead::FileDevice::FileOpenFlag::eWriteOnly, 0);

    if (!handle.getDevice())
    {
        device->tryOpen(&handle, path, sead::FileDevice::FileOpenFlag::eCreate, 0);

        if (!handle.getDevice())
        {
            return false;
        }
    }

    sead::Endian::Types endian = sead::Endian::eLittle;

    sead::FileDeviceWriteStream stream(&handle, sead::Stream::Modes::eBinary);
    stream.setBinaryEndian(endian);

    stream.writeString("RIFF", 4);
    stream.writeU32(0); // File Size
    stream.writeString("WAVE", 4);

    SEAD_ASSERT(channelIdx < (s32)snd::cWaveChannelMax);

    u32 numChannels = channelIdx == -1 ? mChannels.size() : 1;
    u32 bitsPerSample = mEncoding == WaveFile::Encoding::Pcm8 ? 8 : 16;

    stream.writeString("fmt ", 4);
    stream.writeU32(0x10); // Chunk Size
    stream.writeU16(1); // Format (1 = Pcm)
    stream.writeU16(numChannels);
    stream.writeU32(mSampleRate);
    stream.writeU32(mSampleRate * numChannels * (bitsPerSample / 8)); // Byte Rate
    stream.writeU16(numChannels * (bitsPerSample / 8)); // Block Align
    stream.writeU16(bitsPerSample);

    stream.writeString("data", 4);
    stream.writeU32(mSampleCount * numChannels * (bitsPerSample / 8)); // Chunk Size

    auto writePcm8 = [&]()
    {
        const s8** channels = new const s8*[numChannels];

        for (u32 ch = 0; ch < numChannels; ch++)
        {
            WaveFile::Channel* channel = mChannels.nth(channelIdx == -1 ? ch : channelIdx);

            channels[ch] = static_cast<const s8*>(channel->getData());
        }

        u8* interleavedSamples = new u8[mSampleCount * numChannels];

        for (u32 i = 0; i < mSampleCount; i++)
        {
            for (u32 ch = 0; ch < numChannels; ch++)
            {
                const s8* data = channels[ch];

                u8 sample = data[i] + 128; //? Convert from signed Pcm8 to unsigned Pcm8
                interleavedSamples[i * numChannels + ch] = sample;
            }
        }

        stream.writeMemBlock(interleavedSamples, mSampleCount * numChannels * sizeof(u8));

        delete[] interleavedSamples;

        delete[] channels;
    };

    auto writePcm16 = [&]()
    {
        const s16** channels = new const s16*[numChannels];

        for (u32 ch = 0; ch < numChannels; ch++)
        {
            WaveFile::Channel* channel = mChannels.nth(channelIdx == -1 ? ch : channelIdx);

            channels[ch] = static_cast<const s16*>(channel->getData());
        }

        s16* interleavedSamples = new s16[mSampleCount * numChannels];

        for (u32 i = 0; i < mSampleCount; i++)
        {
            for (u32 ch = 0; ch < numChannels; ch++)
            {
                const s16* data = channels[ch];

                interleavedSamples[i * numChannels + ch] = sead::Endian::convertS16(mDataEndian, endian, data[i]);
            }
        }

        stream.writeMemBlock(interleavedSamples, mSampleCount * numChannels * sizeof(s16));

        delete[] interleavedSamples;

        delete[] channels;
    };

    auto writeDspAdpcm = [&]()
    {
        s16** channels = new s16*[numChannels];

        for (u32 ch = 0; ch < numChannels; ch++)
        {
            WaveFile::Channel* channel = mChannels.nth(channelIdx == -1 ? ch : channelIdx);

            const snd::DspAdpcmParam& dspAdpcmParam = channel->getAdpcmParam();

            snd::AdpcmContext adpcmContext;
            adpcmContext.pred_scale = dspAdpcmParam.predScale;
            adpcmContext.yn1 = dspAdpcmParam.yn1;
            adpcmContext.yn2 = dspAdpcmParam.yn2;

            snd::AdpcmParam adpcmParam;
            for (u32 i = 0; i < 8; i++)
            {
                adpcmParam.coef[i][0] = dspAdpcmParam.coef[i][0];
                adpcmParam.coef[i][1] = dspAdpcmParam.coef[i][1];
            }

            s16* data = new s16[mSampleCount];
            snd::internal::DecodeDspAdpcm(0, adpcmContext, adpcmParam, channel->getData(), mSampleCount, data);

            channels[ch] = data;
        }

        s16* interleavedSamples = new s16[mSampleCount * numChannels];

        for (u32 i = 0; i < mSampleCount; i++)
        {
            for (u32 ch = 0; ch < numChannels; ch++)
            {
                s16* data = channels[ch];

                interleavedSamples[i * numChannels + ch] = data[i];
            }
        }

        stream.writeMemBlock(interleavedSamples, mSampleCount * numChannels * sizeof(s16));

        delete[] interleavedSamples;

        for (u32 ch = 0; ch < numChannels; ch++)
        {
            s16* data = channels[ch];

            delete[] data;
        }

        delete[] channels;
    };

    switch (mEncoding)
    {
        case WaveFile::Encoding::Pcm8:
            writePcm8();
            break;

        case WaveFile::Encoding::Pcm16:
            writePcm16();
            break;

        case WaveFile::Encoding::DspAdpcm:
            writeDspAdpcm();
            break;

        default:
            SEAD_ASSERT_MSG(false, "Invalid Encoding");
            break;
    }

    // TODO: Loop info ?

    // Update File Size
    u32 fileSize = handle.getCurrentSeekPos() - 8;
    handle.seek(4, sead::FileDevice::SeekOrigin::eBegin);
    stream.writeU32(fileSize);

    return true;
}
