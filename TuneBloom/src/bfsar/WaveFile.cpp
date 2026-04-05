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
    dispose_();

    if (mOwnsData && mData)
    {
        delete[] mData;
        mData = nullptr;
    }

    freeSeekInfo_();
}

void WaveFile::Channel::dispose_()
{
    snd::internal::driver::SoundThreadLock lock;

    snd::internal::driver::DisposeCallbackMgr::instance()->dispose(mData, mDataSize);
}

void WaveFile::Channel::freeSeekInfo_()
{
    if (mSeekInfo)
    {
        delete[] mSeekInfo;
        mSeekInfo = nullptr;
        mSeekInfoBlocks = 0;
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

const Item* WaveFile::validate(sead::BufferedSafeString& error) const
{
    switch (getEncoding())
    {
        case Encoding::Pcm8:
        case Encoding::Pcm16:
        case Encoding::DspAdpcm:
            break;

        default:
            error = "Invalid Encoding";
            return this;
    }

    if (getLoopEndFrame() <= getLoopStartFrame())
    {
        error = "Invalid loop start/end";
        return this;
    }

    if (getLoopEndFrame() < 1)
    {
        error = "No data to write";
        return this;
    }

    if (getChannels().isEmpty())
    {
        error = "Wave File must have at least 1 channel";
        return this;
    }

    if (getChannels().size() > 2)
    {
        error = "Wave File can only have up to 2 channels";
        return this;
    }

    return nullptr;
}

void FillAdpcmInfo(ADPCMINFO* adpcmInfo, const snd::DspAdpcmParam& param, const snd::internal::DspAdpcmLoopParam& loopParam)
{
    SEAD_ASSERT(adpcmInfo);

    adpcmInfo->coef[0]  = param.coef[0][0];
    adpcmInfo->coef[1]  = param.coef[0][1];
    adpcmInfo->coef[2]  = param.coef[1][0];
    adpcmInfo->coef[3]  = param.coef[1][1];
    adpcmInfo->coef[4]  = param.coef[2][0];
    adpcmInfo->coef[5]  = param.coef[2][1];
    adpcmInfo->coef[6]  = param.coef[3][0];
    adpcmInfo->coef[7]  = param.coef[3][1];
    adpcmInfo->coef[8]  = param.coef[4][0];
    adpcmInfo->coef[9]  = param.coef[4][1];
    adpcmInfo->coef[10] = param.coef[5][0];
    adpcmInfo->coef[11] = param.coef[5][1];
    adpcmInfo->coef[12] = param.coef[6][0];
    adpcmInfo->coef[13] = param.coef[6][1];
    adpcmInfo->coef[14] = param.coef[7][0];
    adpcmInfo->coef[15] = param.coef[7][1];

    adpcmInfo->pred_scale = param.predScale;
    adpcmInfo->yn1 = param.yn1;
    adpcmInfo->yn2 = param.yn2;

    adpcmInfo->loop_pred_scale = loopParam.loopPredScale;
    adpcmInfo->loop_yn1 = loopParam.loopYn1;
    adpcmInfo->loop_yn2 = loopParam.loopYn2;
}

void FillAdpcmParam(snd::DspAdpcmParam* param, snd::internal::DspAdpcmLoopParam* loopParam, const ADPCMINFO& adpcmInfo)
{
    SEAD_ASSERT(param);
    SEAD_ASSERT(loopParam);

    param->coef[0][0] = adpcmInfo.coef[0];
    param->coef[0][1] = adpcmInfo.coef[1];
    param->coef[1][0] = adpcmInfo.coef[2];
    param->coef[1][1] = adpcmInfo.coef[3];
    param->coef[2][0] = adpcmInfo.coef[4];
    param->coef[2][1] = adpcmInfo.coef[5];
    param->coef[3][0] = adpcmInfo.coef[6];
    param->coef[3][1] = adpcmInfo.coef[7];
    param->coef[4][0] = adpcmInfo.coef[8];
    param->coef[4][1] = adpcmInfo.coef[9];
    param->coef[5][0] = adpcmInfo.coef[10];
    param->coef[5][1] = adpcmInfo.coef[11];
    param->coef[6][0] = adpcmInfo.coef[12];
    param->coef[6][1] = adpcmInfo.coef[13];
    param->coef[7][0] = adpcmInfo.coef[14];
    param->coef[7][1] = adpcmInfo.coef[15];

    param->predScale = adpcmInfo.pred_scale;
    param->yn1 = adpcmInfo.yn1;
    param->yn2 = adpcmInfo.yn2;

    loopParam->loopPredScale = adpcmInfo.loop_pred_scale;
    loopParam->loopYn1 = adpcmInfo.loop_yn1;
    loopParam->loopYn2 = adpcmInfo.loop_yn2;
}

void FillAdpcmParam(snd::AdpcmParam* param, const ADPCMINFO& adpcmInfo)
{
    SEAD_ASSERT(param);

    param->coef[0][0] = adpcmInfo.coef[0];
    param->coef[0][1] = adpcmInfo.coef[1];
    param->coef[1][0] = adpcmInfo.coef[2];
    param->coef[1][1] = adpcmInfo.coef[3];
    param->coef[2][0] = adpcmInfo.coef[4];
    param->coef[2][1] = adpcmInfo.coef[5];
    param->coef[3][0] = adpcmInfo.coef[6];
    param->coef[3][1] = adpcmInfo.coef[7];
    param->coef[4][0] = adpcmInfo.coef[8];
    param->coef[4][1] = adpcmInfo.coef[9];
    param->coef[5][0] = adpcmInfo.coef[10];
    param->coef[5][1] = adpcmInfo.coef[11];
    param->coef[6][0] = adpcmInfo.coef[12];
    param->coef[6][1] = adpcmInfo.coef[13];
    param->coef[7][0] = adpcmInfo.coef[14];
    param->coef[7][1] = adpcmInfo.coef[15];
}

void WaveFile::drawUI()
{
    InnerFile::drawUI();

    ImGui::SeparatorText("");

    const ImU32 cStepU32 = 1;

    static Encoding sEncoding = Encoding::DspAdpcm;

    {
        u32 encoding = (u32)mEncoding;
        if (ImGui::Combo("Encoding", (s32*)&encoding, sEncodingTypes, IM_ARRAYSIZE(sEncodingTypes)))
        {
            if (encoding != (u32)mEncoding)
            {
                sEncoding = Encoding(encoding);
                ImGui::OpenPopup("###ENCODING");
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

        u32 sampleCount = mSampleCount;
        if (ImGui::InputScalar("Sample Count", ImGuiDataType_U32, &sampleCount, &cStepU32))
        {
            //mSampleCount = sampleCount;
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

            updateLoopInfo_(false);
        }

        //ImGui::Text("Loop Start: %u", (mLoopStartFrame / 14) * 14);
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

        //ImGui::Text("Loop End: %u", (mLoopEndFrame / 14) * 14);
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

    if (ImGui::BeginPopupModal(ICON_LC_ALERT_TRIANGLE " Warning###ENCODING", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Altering the wave file encoding can result in quality loss and noise.\nConvert anyway ?");
        ImGui::Separator();

        ImVec2 buttonSize((ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().WindowPadding.x * 2.0f) / 2.0f, 0.0f);

        if (ImGui::Button("Convert", buttonSize))
        {
            {
                snd::internal::driver::SoundThreadLock lock;

                for (u32 i = 0; i < mChannels.size(); i++)
                {
                    Channel* channel = mChannels.nth(i);
                    channel->dispose_();
                }
            }

            for (u32 i = 0; i < mChannels.size(); i++)
            {
                Channel* channel = mChannels.nth(i);

                channel->freeSeekInfo_();

                if (mEncoding == Encoding::Pcm16 || mEncoding == Encoding::Pcm8 &&
                    sEncoding == Encoding::DspAdpcm)
                {
                    buildSeekTable_(
                        channel->getData(), getSampleCount(),
                        mEncoding == Encoding::Pcm16 ? snd::SampleFormat::PcmS16 : snd::SampleFormat::PcmS8,
                        *channel
                    );
                }

                if (mUseOriginalData)
                {
                    const void* src = channel->getData();
                    channel->mData = new u8[channel->getDataSize()];
                    sead::MemUtil::copy(const_cast<void*>(channel->mData), src, channel->getDataSize());
                }

                u32 dataSize = 0;
                ADPCMINFO adpcmInfo;
                sead::MemUtil::fillZero(&adpcmInfo, sizeof(adpcmInfo));
                ADPCMINFO adpcmInfoStream;
                sead::MemUtil::fillZero(&adpcmInfoStream, sizeof(adpcmInfoStream));

                if (mEncoding == Encoding::DspAdpcm)
                {
                    FillAdpcmInfo(&adpcmInfo, channel->getAdpcmParam(), channel->getAdpcmLoopParam());
                    FillAdpcmInfo(&adpcmInfoStream, channel->getAdpcmParam(true), channel->getAdpcmLoopParam(true));
                }

                channel->mOwnsData = true;
                channel->mData = WaveFile::convert_(
                    const_cast<void*>(channel->getData()), mDataEndian, mSampleCount,
                    mEncoding, sEncoding,
                    &dataSize, &adpcmInfo, &adpcmInfoStream,
                    getLoopStartFrame(false), getLoopStartFrame(true)
                );
                channel->mDataSize = dataSize;

                if (sEncoding == Encoding::DspAdpcm)
                {
                    FillAdpcmParam(&channel->mAdpcmParam, &channel->mAdpcmLoopParam, adpcmInfo);
                    FillAdpcmParam(&channel->mAdpcmParamStream, &channel->mAdpcmLoopParamStream, adpcmInfoStream);
                }
                else if (sEncoding == Encoding::Pcm16)
                {
                    mDataEndian = sead::Endian::getHostEndian();
                }
            }

            invalidateOriginalData_();
            mEncoding = sEncoding;

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", buttonSize))
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
    mOriginalLoopEndFrame = mLoopEndFrame;

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
        stream->writeU32(isOriginalLoopAvailable(mVersion) ? getOriginalLoopStartFrame() : 0);

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

    sead::Endian::Types endian = sead::Endian::eLittle; // RIFF wave files are always little endian... or they should

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
    mLoopEndFrame = mSampleCount;
    mOriginalLoopStartFrame = mLoopStartFrame;
    mOriginalLoopEndFrame = mLoopEndFrame;

    for (u32 i = 0; i < numChannels; i++)
    {
        Channel* channel = mChannels.birthBack();
        SEAD_ASSERT(channel);

        u32 dataSize = 0;
        ADPCMINFO adpcmInfo;
        sead::MemUtil::fillZero(&adpcmInfo, sizeof(adpcmInfo));
        ADPCMINFO adpcmInfoStream;
        sead::MemUtil::fillZero(&adpcmInfoStream, sizeof(adpcmInfoStream));

        if (encoding == Encoding::DspAdpcm)
        {
            buildSeekTable_(channels[i], sampleCount, sampleFormat, *channel);
        }

        channel->mOwnsData = true;
        channel->mData = WaveFile::convert_(
            channels[i], endian, mSampleCount,
            sampleFormat == snd::SampleFormat::PcmS8 ? Encoding::Pcm8 : Encoding::Pcm16, encoding,
            &dataSize, &adpcmInfo, &adpcmInfoStream,
            getLoopStartFrame(false), getLoopStartFrame(true)
        );

        if (dataSize == 0)
        {
            dataSize = sampleBytes / numChannels;
        }

        channel->mDataSize = dataSize;

        if (encoding == Encoding::DspAdpcm)
        {
            FillAdpcmParam(&channel->mAdpcmParam, &channel->mAdpcmLoopParam, adpcmInfo);
            FillAdpcmParam(&channel->mAdpcmParamStream, &channel->mAdpcmLoopParamStream, adpcmInfoStream);
        }
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
    stream.writeU32(0); // File Size, updated at the end
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
            const WaveFile::Channel* channel = mChannels.nth(channelIdx == -1 ? ch : channelIdx);

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
            const WaveFile::Channel* channel = mChannels.nth(channelIdx == -1 ? ch : channelIdx);

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
            const WaveFile::Channel* channel = mChannels.nth(channelIdx == -1 ? ch : channelIdx);

            ADPCMINFO adpcmInfo;
            sead::MemUtil::fillZero(&adpcmInfo, sizeof(adpcmInfo));

            FillAdpcmInfo(&adpcmInfo, channel->getAdpcmParam(), channel->getAdpcmLoopParam());

            s16* data = new s16[mSampleCount];
            decode((u8*)channel->getData(), data, &adpcmInfo, mSampleCount);

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

void WaveFile::buildSeekTable_(const void* samples, u32 sampleCount, snd::SampleFormat sampleFormat, Channel& channel)
{
    const u32 cDefaultSamplesPerBlock = 0x3800;
    const u32 cSamplesPerBlock = cDefaultSamplesPerBlock;

    channel.freeSeekInfo_();

    u32 blockNum = ceil(sampleCount / (f32)cSamplesPerBlock);
    channel.mSeekInfo = new Channel::SeekInfo[blockNum];
    channel.mSeekInfoBlocks = blockNum;

    for (u32 blockNo = 0; blockNo < blockNum; blockNo++)
    {
        Channel::SeekInfo& seekInfo = channel.mSeekInfo[blockNo];
        if (blockNo == 0) //? First block is always 0
        {
            seekInfo.yn1 = 0;
            seekInfo.yn2 = 0;
            continue;
        }

        if (sampleFormat == snd::SampleFormat::PcmS8)
        {
            // TODO
        }
        else if (sampleFormat == snd::SampleFormat::PcmS16)
        {
            const s16* samples16 = (const s16*)samples;

            seekInfo.yn1 = samples16[blockNo * cSamplesPerBlock - 1];
            seekInfo.yn2 = samples16[blockNo * cSamplesPerBlock - 2];
        }
    }
}

void WaveFile::updateLoopInfo_(bool skipStream)
{
    if (mEncoding != Encoding::DspAdpcm)
    {
        return;
    }

    for (u32 i = 0; i < mChannels.size(); i++)
    {
        Channel* channel = mChannels.nth(i);

        ADPCMINFO adpcmInfo;
        sead::MemUtil::fillZero(&adpcmInfo, sizeof(adpcmInfo));

        FillAdpcmInfo(&adpcmInfo, channel->getAdpcmParam(), channel->getAdpcmLoopParam());
        getLoopContext(static_cast<u8*>(const_cast<void*>(channel->getData())), &adpcmInfo, getLoopStartFrame(false));

        channel->mAdpcmLoopParam.loopPredScale = adpcmInfo.loop_pred_scale;
        channel->mAdpcmLoopParam.loopYn1 = adpcmInfo.loop_yn1;
        channel->mAdpcmLoopParam.loopYn2 = adpcmInfo.loop_yn2;

        if (!skipStream)
        {
            FillAdpcmInfo(&adpcmInfo, channel->getAdpcmParam(true), channel->getAdpcmLoopParam(true));
            getLoopContext(static_cast<u8*>(const_cast<void*>(channel->getData())), &adpcmInfo, getLoopStartFrame(true));

            channel->mAdpcmLoopParamStream.loopPredScale = adpcmInfo.loop_pred_scale;
            channel->mAdpcmLoopParamStream.loopYn1 = adpcmInfo.loop_yn1;
            channel->mAdpcmLoopParamStream.loopYn2 = adpcmInfo.loop_yn2;
        }
    }
}

void* WaveFile::convert_(
    void* data, sead::Endian::Types dataEndian, u32 samples,
    Encoding from, Encoding to, u32* size,
    ADPCMINFO* adpcmInfo, ADPCMINFO* adpcmInfoStream,
    u32 loopSample, u32 loopSampleStream)
{
    SEAD_ASSERT(size);

    if (from == to)
    {
        // TODO: Set size
        return data;
    }

    auto Pcm8ToPcm16 = [&](void* dataSrc) -> void*
    {
        s8* src = static_cast<s8*>(dataSrc);
        s16* dst = new s16[samples];

        for (u32 i = 0; i < samples; i++)
        {
            dst[i] = s16(src[i] << 8);
        }

        delete[] src;

        *size = sizeof(s16) * samples;
        return dst;
    };

    auto Pcm8ToAdpcm = [&](void* dataSrc) -> void*
    {
        SEAD_ASSERT(adpcmInfo);

        u32 bufferSize = getBytesForAdpcmBuffer(samples);

        s16* src = static_cast<s16*>(Pcm8ToPcm16(dataSrc));
        u8* dst = new u8[bufferSize];

        encode(src, dst, adpcmInfo, samples);
        getLoopContext(dst, adpcmInfo, loopSample);

        sead::MemUtil::copy(adpcmInfoStream, adpcmInfo, sizeof(ADPCMINFO));
        getLoopContext(dst, adpcmInfoStream, loopSampleStream);

        delete[] src;

        *size = bufferSize;
        return dst;
    };

    auto Pcm16ToPcm8 = [&](void* dataSrc) -> void*
    {
        s16* src = static_cast<s16*>(dataSrc);
        s8* dst = new s8[samples];

        for (u32 i = 0; i < samples; i++)
        {
            dst[i] = s8(sead::Endian::toHostS16(dataEndian, src[i]) >> 8);
        }

        delete[] src;

        *size = sizeof(s8) * samples;
        return dst;
    };

    auto Pcm16ToAdpcm = [&](void* dataSrc) -> void*
    {
        SEAD_ASSERT(adpcmInfo);

        u32 bufferSize = getBytesForAdpcmBuffer(samples);

        s16* src = static_cast<s16*>(dataSrc);
        u8* dst = new u8[bufferSize];

        if (dataEndian != sead::Endian::getHostEndian())
        {            
            for (u32 i = 0; i < samples; i++)
            {
                src[i] = sead::Endian::toHostS16(dataEndian, src[i]);
            }
        }

        encode(src, dst, adpcmInfo, samples);
        getLoopContext(dst, adpcmInfo, loopSample);

        sead::MemUtil::copy(adpcmInfoStream, adpcmInfo, sizeof(ADPCMINFO));
        getLoopContext(dst, adpcmInfoStream, loopSampleStream);

        delete[] src;

        *size = bufferSize;
        return dst;
    };

    auto AdpcmToPcm16 = [&](void* dataSrc) -> void*
    {
        SEAD_ASSERT(adpcmInfo);

        u8* src = static_cast<u8*>(dataSrc);
        s16* dst = new s16[samples];

        decode(src, dst, adpcmInfo, samples);

        delete[] src;

        *size = sizeof(s16) * samples;
        return dst;
    };

    auto AdpcmToPcm8 = [&](void* dataSrc) -> void*
    {
        SEAD_ASSERT(adpcmInfo);

        s8* dst = static_cast<s8*>(Pcm16ToPcm8(AdpcmToPcm16(dataSrc)));

        *size = sizeof(s8) * samples;
        return dst;
    };

    switch (from)
    {
        case Encoding::Pcm8:
        {
            switch (to)
            {
                case Encoding::Pcm16:
                    return Pcm8ToPcm16(data);

                case Encoding::DspAdpcm:
                    return Pcm8ToAdpcm(data);
            }

            break;
        }

        case Encoding::Pcm16:
        {
            switch (to)
            {
                case Encoding::Pcm8:
                    return Pcm16ToPcm8(data);

                case Encoding::DspAdpcm:
                    return Pcm16ToAdpcm(data);
            }

            break;
        }

        case Encoding::DspAdpcm:
        {
            switch (to)
            {
                case Encoding::Pcm8:
                    return AdpcmToPcm8(data);

                case Encoding::Pcm16:
                    return AdpcmToPcm16(data);
            }

            break;
        }
    }

    SEAD_ASSERT_MSG(false, "Invalid Encoding");
    return data;
}
