#include <bfsar/BfwsdFile.h>

#include <bfsar/writer/FlagParameters.h>

#include <ui/UI.h>

#include <snd/snd_WaveSoundFile.h>

#include <bit>

u32 BfwsdFile::doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const
{
    SEAD_ASSERT(mSoundSet);
    SEAD_ASSERT(mWaveArchive);
    SEAD_ASSERT(mWaveArchiveWaveFilesIndexes);

    struct WaveId
    {
        WaveId(u32 warcId_, u32 waveIdx_)
            : warcId(warcId_)
            , waveIdx(waveIdx_)
        {
        }

        u32 warcId;
        u32 waveIdx;
    };

    std::unordered_map<const WaveFile*, u32> waveIdIndexes;
    std::vector<WaveId> waveIds;

    u32 waveSoundCount = 0;
    for (const Item::ListNode* itemNode = sBfsar.getItem(mSoundSet->getStartId(), sBfsar.getSoundList()); itemNode && itemNode->val()->getId() <= mSoundSet->getEndId(); itemNode = sBfsar.getSoundList().next(itemNode))
    {
        SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
        const Sound* sound = static_cast<const Sound*>(itemNode->val());

        if (sound->getSoundType() != Sound::SoundType::Wave)
        {
            continue;
        }

        waveSoundCount++;

        const Item* waveFileItem = sound->getWaveSoundInfo().getWaveFileRef().getItem();
        SEAD_ASSERT(waveFileItem);
        SEAD_ASSERT(waveFileItem->getItemType() == Item::ItemType::WaveFile);

        const WaveFile* waveFile = static_cast<const WaveFile*>(waveFileItem);

        if (!waveIdIndexes.contains(waveFile))
        {
            waveIdIndexes[waveFile] = waveIds.size();

            const auto& it = mWaveArchiveWaveFilesIndexes->find(waveFile);
            SEAD_ASSERT(it != mWaveArchiveWaveFilesIndexes->end());

            waveIds.push_back(
                WaveId(
                    nw::snd::internal::Util::GetMaskedItemId(mWaveArchive->getId(), nw::snd::internal::ItemType_WaveArchive),
                    it->second
                )
            );
        }
    }

    struct NoteEvent
    {
        NoteEvent()
            : position(0.0f)
            , length(0.0f)
            , noteIdx(0)
            , reserved(0)
        {
        }

        f32 position;
        f32 length;
        u32 noteIdx;
        u32 reserved;
    };

    struct TrackInfo
    {
        std::vector<NoteEvent> noteEvents;
    };

    struct NoteInfo
    {
        NoteInfo(u32 waveIdIdx_)
            : waveIdIdx(waveIdIdx_)
            , optionParam(0)
        {
        }

        u32 waveIdIdx;
        u32 optionParam;
    };

    FileWriter writer(handle, stream);
    writer.openFile("FWSD", 1, mVersion);

    //? Info Block
    {
        writer.openBlock(nw::snd::internal::ElementType_WaveSoundFile_InfoBlock, "INFO");

        writer.openReference("WaveIdTable");
        writer.openReference("WaveSoundDataTableRef");

        writer.closeReference("WaveIdTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

        stream->writeU32(waveIds.size());

        for (const WaveId& waveId : waveIds)
        {
            stream->writeU32(waveId.warcId);
            stream->writeU32(waveId.waveIdx);
        }

        writer.closeReference("WaveSoundDataTableRef", nw::snd::internal::ElementType_Table_ReferenceTable);
        writer.pushOffsetBase();
        {
            writer.openReferenceTable("WaveSoundDataTable", waveSoundCount);

            for (const Item::ListNode* itemNode = sBfsar.getItem(mSoundSet->getStartId(), sBfsar.getSoundList()); itemNode && itemNode->val()->getId() <= mSoundSet->getEndId(); itemNode = sBfsar.getSoundList().next(itemNode))
            {
                SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
                const Sound* sound = static_cast<const Sound*>(itemNode->val());

                if (sound->getSoundType() != Sound::SoundType::Wave)
                {
                    continue;
                }

                writer.addReferenceTableReference("WaveSoundDataTable", nw::snd::internal::ElementType_WaveSoundFile_WaveSoundMetaData);

                writer.pushOffsetBase();
                {
                    writer.openReference("WaveSoundInfo");
                    writer.openReference("TrackInfoTableRef");
                    writer.openReference("NoteInfoTableRef");

                    writer.closeReference("WaveSoundInfo", nw::snd::internal::ElementType_WaveSoundFile_WaveSoundInfo);
                    writer.pushOffsetBase();
                    {
                        u32 pos = writer.getPosition();

                        const Sound::WaveSoundInfo& waveSoundInfo = sound->getWaveSoundInfo();

                        u32 sendParamOffset = writer.getPosition() + 4;
                        u32 envelopeParamOffset = writer.getPosition() + 4;

                        std::unordered_map<u32, u32> flags;
                        if (waveSoundInfo.isEnablePan())
                        {
                            u32 flag = waveSoundInfo.getPan() | (waveSoundInfo.getSurroundPan() << 8);
                            flags[nw::snd::internal::WAVE_SOUND_INFO_PAN] = flag;

                            sendParamOffset += 4;
                            envelopeParamOffset += 4;
                        }

                        if (waveSoundInfo.isEnablePitch())
                        {
                            u32 flag = std::bit_cast<u32>(waveSoundInfo.getPitch());
                            flags[nw::snd::internal::WAVE_SOUND_INFO_PITCH] = flag;

                            sendParamOffset += 4;
                            envelopeParamOffset += 4;
                        }

                        if (isFilterSupportedVersion(mVersion))
                        {
                            if (waveSoundInfo.isEnableFilter())
                            {
                                u32 flag = waveSoundInfo.getLpfFreq() | (waveSoundInfo.getBiquadType() << 8) | (waveSoundInfo.getBiquadValue() << 16);
                                flags[nw::snd::internal::WAVE_SOUND_INFO_FILTER] = flag;

                                sendParamOffset += 4;
                                envelopeParamOffset += 4;
                            }
                        }

                        if (waveSoundInfo.isEnableSend())
                        {
                            flags[nw::snd::internal::WAVE_SOUND_INFO_SEND] = 0;

                            envelopeParamOffset += 4;
                        }

                        if (waveSoundInfo.isEnableEnvelope())
                        {
                            flags[nw::snd::internal::WAVE_SOUND_INFO_ENVELOPE] = 0;
                        }

                        FlagParameters params(flags);
                        params.write(writer);

                        if (waveSoundInfo.isEnableSend())
                        {
                            u32 sendPos = writer.getPosition();
                            writer.seek(sendParamOffset);
                            stream->writeU32(sendPos - pos);
                            writer.seek(sendPos);

                            stream->writeU8(waveSoundInfo.getMainSend());

                            u8 fxSendCount = nw::snd::AUX_BUS_NUM;
                            stream->writeU8(fxSendCount);

                            for (u32 i = 0; i < fxSendCount; i++)
                            {
                                stream->writeU8(waveSoundInfo.getFxSend(i));
                            }

                            writer.align(4);
                        }

                        if (waveSoundInfo.isEnableEnvelope())
                        {
                            u32 envelopePos = writer.getPosition();
                            writer.seek(envelopeParamOffset);
                            stream->writeU32(envelopePos - pos);
                            writer.seek(envelopePos);

                            writer.pushOffsetBase();
                            {
                                writer.openReference("AdshrCurve");

                                writer.closeReference("AdshrCurve", 0);
                                const snd::AdshrCurve& adshrCurve = waveSoundInfo.getAdshrCurve();

                                stream->writeU8(adshrCurve.attack);
                                stream->writeU8(adshrCurve.decay);
                                stream->writeU8(adshrCurve.sustain);
                                stream->writeU8(adshrCurve.hold);
                                stream->writeU8(adshrCurve.release);

                                writer.align(4);
                            }
                            writer.popOffsetBase();
                        }
                    }
                    writer.popOffsetBase();

                    writer.closeReference("TrackInfoTableRef", nw::snd::internal::ElementType_Table_ReferenceTable);
                    writer.pushOffsetBase();
                    {
                        std::vector<TrackInfo> tracks;

                        {
                            TrackInfo trackInfo;
                            trackInfo.noteEvents.push_back(NoteEvent());

                            tracks.push_back(trackInfo);
                        }

                        writer.openReferenceTable("TrackInfoTable", tracks.size());

                        for (const TrackInfo& trackInfo : tracks)
                        {
                            writer.addReferenceTableReference("TrackInfoTable", nw::snd::internal::ElementType_WaveSoundFile_TrackInfo);

                            writer.pushOffsetBase();
                            {
                                writer.openReference("NoteEventTableRef");

                                writer.closeReference("NoteEventTableRef", 0);
                                writer.pushOffsetBase();
                                {
                                    writer.openReferenceTable("NoteEventTable", trackInfo.noteEvents.size());

                                    for (const NoteEvent& noteEvent : trackInfo.noteEvents)
                                    {
                                        writer.addReferenceTableReference("NoteEventTable", nw::snd::internal::ElementType_WaveSoundFile_NoteEvent);

                                        stream->writeF32(noteEvent.position);
                                        stream->writeF32(noteEvent.length);
                                        stream->writeU32(noteEvent.noteIdx);
                                        stream->writeU32(noteEvent.reserved);
                                    }

                                    writer.closeReferenceTable("NoteEventTable");
                                }
                                writer.popOffsetBase();
                            }
                            writer.popOffsetBase();
                        }

                        writer.closeReferenceTable("TrackInfoTable");
                    }
                    writer.popOffsetBase();

                    writer.closeReference("NoteInfoTableRef", nw::snd::internal::ElementType_Table_ReferenceTable);
                    writer.pushOffsetBase();
                    {
                        std::vector<NoteInfo> notes;

                        {
                            const Item* waveFileItem = sound->getWaveSoundInfo().getWaveFileRef().getItem();
                            SEAD_ASSERT(waveFileItem);
                            SEAD_ASSERT(waveFileItem->getItemType() == Item::ItemType::WaveFile);

                            const WaveFile* waveFile = static_cast<const WaveFile*>(waveFileItem);

                            NoteInfo noteInfo(waveIdIndexes[waveFile]);

                            notes.push_back(noteInfo);
                        }

                        writer.openReferenceTable("NoteInfoTable", notes.size());

                        for (const NoteInfo& noteInfo : notes)
                        {
                            writer.addReferenceTableReference("NoteInfoTable", nw::snd::internal::ElementType_WaveSoundFile_NoteInfo);

                            stream->writeU32(noteInfo.waveIdIdx);
                            stream->writeU32(noteInfo.optionParam);
                        }

                        writer.closeReferenceTable("NoteInfoTable");

                    }
                    writer.popOffsetBase();
                }
                writer.popOffsetBase();
            }

            writer.closeReferenceTable("WaveSoundDataTable");
        }
        writer.popOffsetBase();

        writer.closeBlock();
    }

    u32 fileSize = writer.getPosition();

    writer.closeFile();

    mSoundSet = nullptr;
    mWaveArchive = nullptr;
    mWaveArchiveWaveFilesIndexes = nullptr;

    return fileSize;
}
