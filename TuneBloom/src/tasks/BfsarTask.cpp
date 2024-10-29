#include "tasks/BfsarTask.h"

#include <filedevice/seadFileDeviceMgr.h>
#include <heap/seadExpHeap.h>

#include "imgui/imgui.h"
#include "imgui_memory_editor/imgui_memory_editor.h"

#include "snd/snd_MemorySoundArchive.h"
#include "snd/snd_BankFileReader.h"
#include "snd/snd_SequenceSoundFileReader.h"
#include "snd/snd_WaveArchiveFileReader.h"
#include "snd/snd_WaveFileReader.h"
#include "snd/snd_WaveSoundFileReader.h"

#include "snd-ply/snd/SoundSystem.h"
#include "snd-ply/snd/WavePlayer.h"

#include "icons/IconsLucide.h"
#include "ui/UI.h"

BfsarTask::BfsarTask(const sead::TaskConstructArg& arg)
    : sead::Task(arg, "BfsarTask")
{
}

BfsarTask::~BfsarTask()
{
    exit();
}

enum RegionType
{
    REGION_TYPE_DIRECT,
    REGION_TYPE_RANGE,
    REGION_TYPE_INDEX,
    REGION_TYPE_UNKNOWN
};

const char* sRegionTypes[] = {
    "Direct",
    "Range",
    "Index",
    "Unknown"
};

RegionType GetRegionType(u16 typeId)
{
    switch (typeId)
    {
        case nw::snd::internal::ElementType_BankFile_DirectReferenceTable:
            return REGION_TYPE_DIRECT;

        case nw::snd::internal::ElementType_BankFile_RangeReferenceTable:
            return REGION_TYPE_RANGE;

        case nw::snd::internal::ElementType_BankFile_IndexReferenceTable:
            return REGION_TYPE_INDEX;

        default:
            return REGION_TYPE_UNKNOWN;
    }
}

const char* GetRegionTypeStr(u16 typeId)
{
    return sRegionTypes[GetRegionType(typeId)];
}

struct RegionIndex
{
    RegionIndex()
        : min(-1)
        , max(-1)
        , type(nullptr)
    {
    }

    s32 getCount() const
    {
        if (max == -1 && min == -1)
            return 0;

        return max - min + 1;
    }

    s32 min;
    s32 max;
    const char* type;
};

struct Instrument
{
    Instrument()
        : instrumentRes(nullptr)
        , velocityRegionIndecies(nullptr)
    {
    }

    const nw::snd::internal::BankFile::Instrument* instrumentRes;
    RegionIndex keyRegionIndex;
    RegionIndex* velocityRegionIndecies; // For each KeyRegion
};

extern const nw::snd::MemorySoundArchive* sSoundArchive;
Instrument** bankInstruments = nullptr;
s32* bankInstrumentCount = nullptr;

nw::snd::internal::BankFileReader* bankFileReaders = nullptr;

static void LoadWav(const sead::SafeString& file, snd::WaveBuffer* leftBuffer, snd::WaveBuffer* rightBuffer, snd::SampleFormat* outFormat, u32* outSampleRate, u32* outChannels, sead::Heap* heap);

snd::WavePlayer sWavePlayerOld;

void PlayWave(const nw::snd::internal::WaveInfo& waveInfo)
{
    sWavePlayerOld.finalize();

    sWavePlayerOld.initialize(waveInfo.channelCount, snd::WavePlayer::cPriorityNoDrop, nullptr, nullptr);

    snd::internal::WaveInfo waveInfoS;
    nw::snd::internal::ConvertWaveInfo(&waveInfoS, waveInfo);

    sWavePlayerOld.start(&waveInfoS);
}

void BfsarTask::prepare()
{
    //adjustHeapAll();

    {
        snd::SoundSystem::SoundSystemParam param;
        param.enable_visualization = true;
        param.voice_synthesize_buffer_count = 10;

        sead::ExpHeap* heap = sead::ExpHeap::create(0, "SoundHeap", nullptr);

        snd::SoundSystem::initialize(param, heap);
        //snd::SoundSystem::initialize(param, sead::HeapMgr::getUnboundHeap());

        heap->adjust();
    }

/*
    {
        static snd::WaveBuffer waveBuffers[2];

        snd::SampleFormat format;
        u32 sampleRate;
        u32 channels;
        LoadWav("main://audio/smm1.wav", &waveBuffers[0], &waveBuffers[1], &format, &sampleRate, &channels, nullptr);

        sWavePlayerOld.initialize(channels, snd::WavePlayer::cPriorityNoDrop, nullptr, nullptr);
        sWavePlayerOld.setWaveInfo(format, sampleRate, 0);

        for (u32 i = 0; i < channels; i++)
        {
            sWavePlayerOld.appendWaveBuffer(i, &waveBuffers[i], true);
        }

        //sWavePlayerOld.start();
    }
    sead::FileDevice::LoadArg arg;
    arg.path = "main://nsmbu.bfsar";
    //arg.path = "main://nsmbu-stream.bfsar";
    //arg.path = "main://nsmb2.bcsar";
    u8* bfsarFile = sead::FileDeviceMgr::instance()->load(arg);

    sSoundArchive = new nw::snd::MemorySoundArchive();

    bool success = sSoundArchive->Initialize(bfsarFile);
    SEAD_ASSERT(success);

    u32 soundCount = sSoundArchive->GetSoundCount();
    for (u32 i = 0; i < soundCount; i++)
    {
        nw::snd::SoundArchive::ItemId soundId = sSoundArchive->GetSoundIdFromIndex(i);
        const nw::snd::internal::SoundArchiveFile::SoundInfo* soundInfo = sSoundArchive->GetSoundInfo(soundId);

        SEAD_ASSERT(soundInfo);
        if (soundInfo->GetSoundType() != nw::snd::SoundArchive::SOUND_TYPE_SEQ)
            continue;

        const char* soundLabel = sSoundArchive->GetItemLabel(soundId);
        if (sead::SafeString(soundLabel).findIndex("BGM") == -1)
            continue;

        const nw::snd::internal::SoundArchiveFile::SequenceSoundInfo* seqSoundInfo = &soundInfo->GetSequenceSoundInfo();
        u32 fileId = soundInfo->fileId;

        const void* file = sSoundArchive->detail_GetFileAddress(fileId);
        SEAD_ASSERT(file);

        nw::snd::internal::SequenceSoundFileReader reader(file);

        s32 labelCount = reader.GetLabelCount();
        if (labelCount > 0)
        {
            SEAD_PRINT("0x%08X: %s\n", soundId, soundLabel);

            u32 offsetPtr = 0;
            SEAD_ASSERT(!reader.GetOffsetByLabel("aaaaaaaaaaaaaaa", &offsetPtr));

            SEAD_ASSERT(!reader.GetLabelByOffset(0xDEADBEEF));

            for (s32 j = 0; j < labelCount; j++)
            {
                const char* label = reader.GetLabel(j);

                u32 offset;
                bool ret = reader.GetOffsetByLabel(label, &offset);
                SEAD_ASSERT(ret);

                //SEAD_ASSERT(reader.GetLabelByOffset(offset) == label); // Some labels share the same offset

                SEAD_PRINT("    Label (%03d, 0x%08X): %s\n", j, offset, label);
            }
        }
    }
*/

    adjustHeapAll();
}

void BfsarTask::enter()
{
}

void BfsarTask::exit()
{
    sead::CurrentHeapSetter chs(sead::HeapMgr::getUnboundHeap());

    CloseFile();

    snd::SoundSystem::finalize();
}

void BfsarTask::calc()
{
    sead::CurrentHeapSetter chs(sead::HeapMgr::getUnboundHeap());

    DrawMenuBar();
    DrawUI();

    //? Code for printing all banks instruments info

    static bool sDo = false;
    if (!sDo)
    {
        return;
    }

    if (!sSoundArchive)
    {
        return;
    }

    sDo = false;

    u32 bankCount = sSoundArchive->GetBankCount();

    bankInstrumentCount = new s32[bankCount];
    bankInstruments = new Instrument*[bankCount];
    bankFileReaders = new nw::snd::internal::BankFileReader[bankCount];

    for (u32 i = 0; i < bankCount; i++)
    {
        nw::snd::SoundArchive::ItemId bankId = sSoundArchive->GetBankIdFromIndex(i);
        const nw::snd::internal::SoundArchiveFile::BankInfo* bankInfo = sSoundArchive->GetBankInfo(bankId);

        u32 stringId = bankInfo->GetStringId();
        const char* string = sSoundArchive->GetString(stringId);

        const nw::snd::internal::Util::Table<nw::ut::ResU32>* waveArchiveItemIdTable = bankInfo->GetWaveArchiveItemIdTable();
        u32 waveArchiveItemIdCount = waveArchiveItemIdTable->count;

        u32* itemIds = new u32[waveArchiveItemIdCount];
        for (u32 i = 0; i < waveArchiveItemIdCount; i++)
        {
            itemIds[i] = waveArchiveItemIdTable->item[i];
        }

        u32 fileId = bankInfo->fileId;

        const void* file = sSoundArchive->detail_GetFileAddress(fileId);
        SEAD_ASSERT(file);

        nw::snd::internal::BankFileReader& reader = bankFileReaders[i];
        reader.Initialize(file);
        s32& instrumentCount = bankInstrumentCount[i];
        instrumentCount = reader.GetInstrumentCount();

        Instrument*& instruments = bankInstruments[i];
        instruments = new Instrument[instrumentCount];
        for (s32 programNo = 0; programNo < instrumentCount; programNo++)
        {
            Instrument& instrument = instruments[programNo];
            instrument.instrumentRes = reader.GetInstrument(programNo);

            if (!instrument.instrumentRes)
                continue;

            instrument.instrumentRes->GetKeyRegionCount(&instrument.keyRegionIndex.min, &instrument.keyRegionIndex.max);
            instrument.keyRegionIndex.type = GetRegionTypeStr(instrument.instrumentRes->toKeyRegionChunk.typeId);

            s32 keyRegionCount = instrument.keyRegionIndex.getCount();
            if (keyRegionCount == 0)
                continue;

            instrument.velocityRegionIndecies = new RegionIndex[keyRegionCount];
            for (s32 j = 0; j < keyRegionCount; j++)
            {
                const nw::snd::internal::BankFile::KeyRegion* keyRegion = instrument.instrumentRes->GetKeyRegion(j + instrument.keyRegionIndex.min);

                RegionIndex& index = instrument.velocityRegionIndecies[j];
                keyRegion->GetVelocityRegionCount(&index.min, &index.max);
                index.type = GetRegionTypeStr(keyRegion->toVelocityRegionChunk.typeId);
            }
        }

        SEAD_PRINT("\n0x%08X: %s\n", bankId, string);

        if (waveArchiveItemIdCount > 0)
        {
            SEAD_PRINT("Wave Archive Item IDs:");

            for (u32 j = 0; j < waveArchiveItemIdCount; j++)
            {
                u32 itemId = itemIds[j];
                SEAD_PRINT("    0x%08X\n", itemId);
            }
        }

        //SEAD_PRINT("File ID: 0x%08X\n", fileId);

        for (s32 programNo = 0; programNo < instrumentCount; programNo++)
        {
            //SEAD_PRINT("    Instrument %d:\n", programNo);
            Instrument& instrument = instruments[programNo];

            for (s32 j = 0; j < instrument.keyRegionIndex.getCount(); j++)
            {
                s32 key = j + instrument.keyRegionIndex.min;
                //SEAD_PRINT("        KeyRegion %d:\n", key);
                const nw::snd::internal::BankFile::KeyRegion* keyRegion = instrument.instrumentRes->GetKeyRegion(key);
                RegionIndex& velocityRegionIndex = instrument.velocityRegionIndecies[j];

                for (s32 k = 0; k < velocityRegionIndex.getCount(); k++)
                {
                    s32 velocity = k + velocityRegionIndex.min;
                    //const nw::snd::internal::BankFile::VelocityRegion* velocityRegion = keyRegion->GetVelocityRegion(velocity);

                    nw::snd::internal::VelocityRegionInfo regionInfo;
                    if (!reader.ReadVelocityRegionInfo(&regionInfo, programNo, key, velocity))
                    {
                        SEAD_ASSERT_MSG(false, "Invalid code smh");
                        continue;
                    }

                    //SEAD_ASSERT(velocityRegion->optionParameter.bitFlag == 0x0000021F);

                    // SEAD_PRINT("            VelocityRegion %d:\n", velocity);
                    // SEAD_PRINT("                WaveArchiveId: 0x%08X\n", regionInfo.waveArchiveId);
                    // SEAD_PRINT("                WaveIndex: 0x%08X\n", regionInfo.waveIndex);
                    // SEAD_PRINT("                RegionParameter.originalKey: %d\n", regionInfo.originalKey);
                    // SEAD_PRINT("                RegionParameter.volume: %d\n", regionInfo.volume);
                    // SEAD_PRINT("                RegionParameter.pan: %d\n", regionInfo.pan);
                    // SEAD_PRINT("                RegionParameter.pitch: %f\n", regionInfo.pitch);
                    // SEAD_PRINT("                RegionParameter.isIgnoreNoteOff: %d\n", regionInfo.isIgnoreNoteOff);
                    // SEAD_PRINT("                RegionParameter.keyGroup: %d\n", regionInfo.keyGroup);
                    // SEAD_PRINT("                RegionParameter.interpolationType: %d\n", regionInfo.interpolationType);
                    // SEAD_PRINT("                RegionParameter.adshrCurve.attack: %d\n", regionInfo.adshrCurve.attack);
                    // SEAD_PRINT("                RegionParameter.adshrCurve.decay: %d\n", regionInfo.adshrCurve.decay);
                    // SEAD_PRINT("                RegionParameter.adshrCurve.sustain: %d\n", regionInfo.adshrCurve.sustain);
                    // SEAD_PRINT("                RegionParameter.adshrCurve.hold: %d\n", regionInfo.adshrCurve.hold);
                    // SEAD_PRINT("                RegionParameter.adshrCurve.release: %d\n", regionInfo.adshrCurve.release);
                }
            }
        }
    }

    SEAD_PRINT("\n");
}

void BfsarTask::draw()
{
    //? Leftover code from when i started this... fun times
    return;

    if (!sSoundArchive)
        return;

    if (ImGui::Begin("BFSAR"))
    {
        if (ImGui::TreeNode("SoundArchivePlayer Info"))
        {
            const nw::snd::internal::SoundArchiveFile::SoundArchivePlayerInfo* playerInfo = sSoundArchive->mInfoBlockBody->GetSoundArchivePlayerInfo();
            if (playerInfo)
            {
                ImGui::Text(" - sequenceSoundMax: %d", (u16)playerInfo->sequenceSoundMax);
                ImGui::Text(" - sequenceTrackMax: %d", (u16)playerInfo->sequenceTrackMax);
                ImGui::Text(" - streamSoundMax: %d", (u16)playerInfo->streamSoundMax);
                ImGui::Text(" - streamTrackMax: %d", (u16)playerInfo->streamTrackMax);
                ImGui::Text(" - streamChannelMax: %d", (u16)playerInfo->streamChannelMax);
                ImGui::Text(" - waveSoundMax: %d", (u16)playerInfo->waveSoundMax);
                ImGui::Text(" - waveTrackMax: %d", (u16)playerInfo->waveTrackMax);
                ImGui::Text(" - padding: (%d, %d)", playerInfo->streamBufferTimes, playerInfo->padding);
                ImGui::Text(" - options: 0x%08X", (u32)playerInfo->options);
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("All Sounds"))
        {
            for (u32 i = 0; i < sSoundArchive->GetSoundCount(); i++)
            {
                const nw::snd::internal::SoundArchiveFile::SoundInfo* sound = sSoundArchive->GetSoundInfo(i + 0x01000000);
                if (!sound)
                    continue;

                if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("%s: %u", sSoundArchive->GetString(sound->GetStringId()), i).cstr()))
                {
                    const char* soundTypes[] = {
                        "SOUND_TYPE_INVALID",
                        "SOUND_TYPE_SEQ",
                        "SOUND_TYPE_STRM",
                        "SOUND_TYPE_WAVE"
                    };

                    ImGui::Text(" - fileId: 0x%08X", (u32)sound->fileId);
                    ImGui::Text(" - playerId: 0x%08X", (u32)sound->playerId);
                    ImGui::Text(" - volume: %u", sound->volume);
                    ImGui::Text(" - remoteFilter: 0x%02X", sound->remoteFilter);
                    ImGui::Text(" - padding1: 0x%02X", sound->padding1);
                    ImGui::Text(" - SoundType: %s", soundTypes[sound->GetSoundType()]);
                    ImGui::Text(" - StringId: 0x%08X", sound->GetStringId());
                    ImGui::Text(" - PanMode: %d", sound->GetPanMode());
                    ImGui::Text(" - PanCurve: %d", sound->GetPanCurve());
                    ImGui::Text(" - PlayerPriority: %d", sound->GetPlayerPriority());
                    ImGui::Text(" - ActorPlayerId: 0x%08X", sound->GetActorPlayerId());

                    for (u32 j = 0; j < nw::snd::internal::USER_PARAM_COUNT; j++)
                    {
                        u32 userParam;
                        bool success = sound->ReadUserParam(j, userParam);
                        if (!success)
                            continue;

                        ImGui::Text(" - UserParam%d: 0x%08X", j, userParam);
                    }

                    ImGui::Text(" - IsFrontBypass: %d", sound->IsFrontBypass());

                    const nw::snd::internal::SoundArchiveFile::Sound3DInfo* sound3DInfo = sound->GetSound3DInfo();
                    if (sound3DInfo)
                    {
                        if (ImGui::TreeNode("Sound3DInfo"))
                        {
                            ImGui::Text("   - flags: 0x%08X", (u32)sound3DInfo->flags);
                            ImGui::Text("   - decayRatio: %f", (f32)sound3DInfo->decayRatio);
                            ImGui::Text("   - decayCurve: %d", sound3DInfo->decayCurve);
                            ImGui::Text("   - dopplerFactor: %d", sound3DInfo->dopplerFactor);
                            ImGui::Text("   - padding: (%d, %d)", sound3DInfo->padding[0], sound3DInfo->padding[1]);

                            ImGui::TreePop();
                        }
                    }

                    switch (sound->GetSoundType())
                    {
                        case nw::snd::SoundArchive::SOUND_TYPE_SEQ:
                        {
                            const nw::snd::internal::SoundArchiveFile::SequenceSoundInfo& seqInfo = sound->GetSequenceSoundInfo();

                            ImGui::Text("---------------");
                            ImGui::Text(" - allocateTrackFlags: 0x%08X", (u32)seqInfo.allocateTrackFlags);

                            u32 bankCount = seqInfo.GetBankIdTable().count;
                            ImGui::Text("BankIdsCount: %u", bankCount);
                            if (ImGui::TreeNode("BankIds"))
                            {
                                u32 banksIds[nw::snd::SoundArchive::SEQ_BANK_MAX] = {
                                    nw::snd::SoundArchive::INVALID_ID,
                                    nw::snd::SoundArchive::INVALID_ID,
                                    nw::snd::SoundArchive::INVALID_ID,
                                    nw::snd::SoundArchive::INVALID_ID
                                };

                                seqInfo.GetBankIds(banksIds);
                                for (u32 j = 0; j < nw::snd::SoundArchive::SEQ_BANK_MAX; j++)
                                {
                                    ImGui::Text("   - %d: 0x%08X", j, banksIds[j]);
                                }

                                ImGui::TreePop();
                            }

                            ImGui::Text(" - StartOffset: 0x%08X", seqInfo.GetStartOffset());
                            ImGui::Text(" - ChannelPriority: %d", seqInfo.GetChannelPriority());
                            ImGui::Text(" - IsReleasePriorityFix: %d", seqInfo.IsReleasePriorityFix());

                            break;
                        }

                        case nw::snd::SoundArchive::SOUND_TYPE_STRM:
                        {
                            const nw::snd::internal::SoundArchiveFile::StreamSoundInfo& streamInfo = sound->GetStreamSoundInfo();

                            ImGui::Text("---------------");
                            ImGui::Text(" - allocateTrackFlags: 0x%04X", (u16)streamInfo.allocateTrackFlags);
                            ImGui::Text(" - allocateChannelCount: 0x%04X", (u16)streamInfo.allocateChannelCount);

                            const nw::snd::internal::SoundArchiveFile::StreamTrackInfoTable* trackTable = streamInfo.GetTrackInfoTable();
                            if (trackTable)
                                ImGui::Text(" - TrackCount: %d", trackTable->GetTrackCount());

                            break;
                        }

                        case nw::snd::SoundArchive::SOUND_TYPE_WAVE:
                        {
                            const nw::snd::internal::SoundArchiveFile::WaveSoundInfo& waveInfo = sound->GetWaveSoundInfo();

                            ImGui::Text("---------------");
                            ImGui::Text(" - index: %d", (u32)waveInfo.index);
                            ImGui::Text(" - allocateTrackCount: %d", (u32)waveInfo.allocateTrackCount);
                            ImGui::Text(" - ChannelPriority: %d", waveInfo.GetChannelPriority());
                            ImGui::Text(" - IsReleasePriorityFix: %d", waveInfo.GetIsReleasePriorityFix());

                            const nw::snd::internal::Util::Table<nw::ut::ResU32>* warcIdTable = sSoundArchive->detail_GetWaveArchiveIdTable(i + 0x01000000);
                            SEAD_ASSERT(warcIdTable);

                            ImGui::Text("WarcIdTable count: %d", (u32)warcIdTable->count);
                            for (u32 j = 0; j < warcIdTable->count; j++)
                            {
                                ImGui::Text("WarcId%d: 0x%08X", j, (u32)warcIdTable->item[j]);
                            }

                            if (ImGui::TreeNode("Reader"))
                            {
                                nw::snd::internal::WaveSoundFileReader reader(sSoundArchive->detail_GetFileAddress(sound->fileId));

                                ImGui::Text("WaveId count: %u", reader.mInfoBlockBody->GetWaveIdCount());

                                if (ImGui::TreeNode("WaveIds"))
                                {
                                    for (u32 j = 0; j < reader.mInfoBlockBody->GetWaveIdCount(); j++)
                                    {
                                        if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("WaveId: %u", j).cstr()))
                                        {
                                            const nw::snd::internal::Util::WaveId* waveId = reader.mInfoBlockBody->GetWaveId(j);

                                            ImGui::Text("WaveArchive: 0x%08X, WaveIndex: %u", (u32)waveId->waveArchiveId, (u32)waveId->waveIndex);

                                            ImGui::TreePop();
                                        }
                                    }

                                    ImGui::TreePop();
                                }

                                ImGui::Text("WaveSoundData count: %u", reader.GetWaveSoundCount());

                                if (ImGui::TreeNode("WaveSounds"))
                                {
                                    for (u32 j = 0; j < reader.GetWaveSoundCount(); j++)
                                    {
                                        if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("WaveSound: %d", j).cstr()))
                                        {
                                            if (ImGui::TreeNode("WaveSoundInfo"))
                                            {
                                                nw::snd::internal::WaveSoundInfo waveSoundInfo;
                                                bool ret = reader.ReadWaveSoundInfo(&waveSoundInfo, j);
                                                SEAD_ASSERT(ret);

                                                ImGui::Text("pitch: %f", waveSoundInfo.pitch);
                                                ImGui::Text("pan: %d", waveSoundInfo.pan);
                                                ImGui::Text("surroundPan: %d", waveSoundInfo.surroundPan);
                                                ImGui::Text("mainSend: %d", waveSoundInfo.mainSend);
                                                for (u32 k = 0; k < nw::snd::AUX_BUS_NUM; k++)
                                                {
                                                    ImGui::Text("fxSend[%d]: %d", k, waveSoundInfo.fxSend[k]);
                                                }
                                                ImGui::Text("lpfFreq: %d", waveSoundInfo.lpfFreq);
                                                ImGui::Text("biquadType: %d", waveSoundInfo.biquadType);
                                                ImGui::Text("biquadValue: %d", waveSoundInfo.biquadValue);

                                                if (ImGui::TreeNode("adshrCurve"))
                                                {
                                                    ImGui::Text("attack: %d", waveSoundInfo.adshr.attack);
                                                    ImGui::Text("decay: %d", waveSoundInfo.adshr.decay);
                                                    ImGui::Text("sustain: %d", waveSoundInfo.adshr.sustain);
                                                    ImGui::Text("hold: %d", waveSoundInfo.adshr.hold);
                                                    ImGui::Text("release: %d", waveSoundInfo.adshr.release);

                                                    ImGui::TreePop();
                                                }

                                                ImGui::TreePop();
                                            }

                                            ImGui::Text("NoteInfoCount: %d", reader.GetNoteInfoCount(j));

                                            if (ImGui::TreeNode("NoteInfos"))
                                            {
                                                for (u32 k = 0; k < reader.GetNoteInfoCount(j); k++)
                                                {
                                                    if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("NoteInfo: %d", k).cstr()))
                                                    {
                                                        //const nw::snd::internal::WaveSoundFile::NoteInfo& noteInfo = reader.mInfoBlockBody->GetWaveSoundData(j).GetNoteInfo(k);
                                                        nw::snd::internal::WaveSoundNoteInfo noteInfo;
                                                        bool ret = reader.ReadNoteInfo(&noteInfo, j, k);
                                                        SEAD_ASSERT(ret);

                                                        ImGui::Text("waveArchiveId: 0x%08X", noteInfo.waveArchiveId);
                                                        ImGui::Text("waveIndex: %d", noteInfo.waveIndex);
                                                        ImGui::Text("originalKey: %d", noteInfo.originalKey);
                                                        ImGui::Text("pan: %d", noteInfo.pan);
                                                        ImGui::Text("surroundPan: %d", noteInfo.surroundPan);
                                                        ImGui::Text("volume: %d", noteInfo.volume);
                                                        ImGui::Text("pitch: %f", noteInfo.pitch);

                                                        if (ImGui::TreeNode("adshrCurve"))
                                                        {
                                                            ImGui::Text("attack: %d", noteInfo.adshr.attack);
                                                            ImGui::Text("decay: %d", noteInfo.adshr.decay);
                                                            ImGui::Text("sustain: %d", noteInfo.adshr.sustain);
                                                            ImGui::Text("hold: %d", noteInfo.adshr.hold);
                                                            ImGui::Text("release: %d", noteInfo.adshr.release);

                                                            ImGui::TreePop();
                                                        }

                                                        if (ImGui::Button("Play"))
                                                        {
                                                            nw::snd::internal::WaveArchiveFileReader reader(sSoundArchive->detail_GetFileAddress(sSoundArchive->GetItemFileId(noteInfo.waveArchiveId)), false);
                                                            nw::snd::internal::WaveFileReader waveReader(reader.GetWaveFile(noteInfo.waveIndex));

                                                            nw::snd::internal::WaveInfo waveInfo;
                                                            bool ret = waveReader.ReadWaveInfo(&waveInfo);
                                                            SEAD_ASSERT(ret);

                                                            PlayWave(waveInfo);
                                                        }

                                                        ImGui::TreePop();
                                                    }
                                                }

                                                ImGui::TreePop();
                                            }

                                            ImGui::Text("TrackInfoCount: %d", reader.GetTrackInfoCount(j));

                                            if (ImGui::TreeNode("TrackInfos"))
                                            {
                                                for (u32 k = 0; k < reader.GetTrackInfoCount(j); k++)
                                                {
                                                    if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("TrackInfo: %d", k).cstr()))
                                                    {
                                                        const nw::snd::internal::WaveSoundFile::TrackInfo& trackInfo = reader.mInfoBlockBody->GetWaveSoundData(j).GetTrackInfo(k);

                                                        ImGui::Text("NoteEventCount: %d", trackInfo.GetNoteEventCount());
                                                        if (ImGui::TreeNode("NoteEvents"))
                                                        {
                                                            for (u32 note = 0; note < trackInfo.GetNoteEventCount(); note++)
                                                            {
                                                                if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("NoteEvent: %d", note).cstr()))
                                                                {
                                                                    const nw::snd::internal::WaveSoundFile::NoteEvent& noteEvent = trackInfo.GetNoteEvent(note);
                                                                    ImGui::Text("position: %f", noteEvent.position);
                                                                    ImGui::Text("length: %f", noteEvent.length);
                                                                    ImGui::Text("noteIndex: %u", noteEvent.noteIndex);
                                                                    ImGui::Text("reserved: %u", noteEvent.reserved);

                                                                    ImGui::TreePop();
                                                                }
                                                            }
                                                            ImGui::TreePop();
                                                        }

                                                        ImGui::TreePop();
                                                    }
                                                }

                                                ImGui::TreePop();
                                            }

                                            ImGui::TreePop();
                                        }
                                    }

                                    ImGui::TreePop();
                                }

                                ImGui::TreePop();
                            }

                            break;
                        }

                        default:
                            SEAD_ASSERT_MSG(false, "Not a valid SoundType(%d)", static_cast<s32>(sound->GetSoundType()));
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("All SoundGroups"))
        {
            for (u32 i = 0; i < sSoundArchive->mInfoBlockBody->GetSoundGroupCount(); i++)
            {
                const nw::snd::internal::SoundArchiveFile::SoundGroupInfo* group = sSoundArchive->mInfoBlockBody->GetSoundGroupInfo(i + 0x02000000);
                if (!group)
                    continue;

                if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("%s: %u", sSoundArchive->GetString(group->GetStringId()), i).cstr()))
                {
                    ImGui::Text(" - startId: 0x%08X", (u32)group->startId);
                    ImGui::Text(" - endId: 0x%08X", (u32)group->endId);
                    ImGui::Text(" - StringId: 0x%08X", group->GetStringId());

                    const nw::snd::internal::Util::Table<nw::ut::ResU32>* fileIdTable = group->GetFileIdTable();
                    ImGui::Text("FileIdTable count: %d", (u32)fileIdTable->count);
                    for (u32 j = 0; j < fileIdTable->count; j++)
                    {
                        ImGui::Text("FileId%d: 0x%08X", j, (u32)fileIdTable->item[j]);
                    }

                    if (group->GetWaveSoundGroupInfo())
                    {
                        const nw::snd::internal::Util::Table<nw::ut::ResU32>* warcIdTable = sSoundArchive->detail_GetWaveArchiveIdTable(i + 0x02000000);
                        SEAD_ASSERT(warcIdTable);

                        ImGui::Text("WarcIdTable count: %d", (u32)warcIdTable->count);
                        for (u32 j = 0; j < warcIdTable->count; j++)
                        {
                            ImGui::Text("WarcId%d: 0x%08X", j, (u32)warcIdTable->item[j]);
                        }
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("All Banks"))
        {
            for (u32 i = 0; i < sSoundArchive->mInfoBlockBody->GetBankCount(); i++)
            {
                const nw::snd::internal::SoundArchiveFile::BankInfo* bank = sSoundArchive->mInfoBlockBody->GetBankInfo(i + 0x03000000);
                if (!bank)
                    continue;

                if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("%s: %u", sSoundArchive->GetString(bank->GetStringId()), i).cstr()))
                {
                    ImGui::Text(" - fileId: 0x%08X", (u32)bank->fileId);
                    ImGui::Text(" - StringId: 0x%08X", bank->GetStringId());

                    const nw::snd::internal::Util::Table<nw::ut::ResU32>* warcIdTable = sSoundArchive->detail_GetWaveArchiveIdTable(i + 0x03000000);
                    SEAD_ASSERT(warcIdTable);

                    ImGui::Text("WarcIdTable count: %d", (u32)warcIdTable->count);
                    for (u32 j = 0; j < warcIdTable->count; j++)
                    {
                        ImGui::Text("WarcId%d: 0x%08X", j, (u32)warcIdTable->item[j]);
                    }

                    //ImGui::TreePop();
                    //continue;

                    const nw::snd::internal::BankFileReader& reader = bankFileReaders[i];

                    const s32 instrumentCount = bankInstrumentCount[i];
                    const Instrument* instruments = bankInstruments[i];

                    if (ImGui::TreeNode("Instruments", "Instruments (%d)", instrumentCount))
                    {
                        for (s32 programNo = 0; programNo < instrumentCount; programNo++)
                        {
                            const Instrument& instrument = instruments[programNo];
                            if (instrument.keyRegionIndex.getCount() == 0)
                            {
                                ImGui::Text(sead::FormatFixedSafeString<256>("Instrument: %d (null)", programNo).cstr());
                                continue;
                            }

                            //ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                            bool ret = ImGui::TreeNode(sead::FormatFixedSafeString<256>("Instrument: %d", programNo).cstr());
                            //ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                            if (ret)
                            {
                                if (ImGui::TreeNode("KeyRegions", "KeyRegions (%d) : %s", instrument.keyRegionIndex.getCount(), instrument.keyRegionIndex.type))
                                {
                                    for (s32 j = 0; j < instrument.keyRegionIndex.getCount(); j++)
                                    {
                                        s32 key = j + instrument.keyRegionIndex.min;
                                        RegionIndex& velocityRegionIndex = instrument.velocityRegionIndecies[j];
                                        if (velocityRegionIndex.getCount() == 0)
                                        {
                                            ImGui::Text(sead::FormatFixedSafeString<256>("KeyRegion: %d (null)", key).cstr());
                                            continue;
                                        }

                                        //ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                                        bool ret = ImGui::TreeNode(sead::FormatFixedSafeString<256>("KeyRegion: %d", key).cstr());
                                        //ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                                        if (ret)
                                        {
                                            if (ImGui::TreeNode("VelocityRegions", "VelocityRegions (%d) : %s", velocityRegionIndex.getCount(), velocityRegionIndex.type))
                                            {
                                                for (s32 k = 0; k < velocityRegionIndex.getCount(); k++)
                                                {
                                                    s32 velocity = k + velocityRegionIndex.min;
                                                    //ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                                                    bool ret = ImGui::TreeNode(sead::FormatFixedSafeString<256>("VelocityRegion: %d", velocity).cstr());
                                                    //ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                                                    if (ret)
                                                    {
                                                        nw::snd::internal::VelocityRegionInfo regionInfo;
                                                        if (!reader.ReadVelocityRegionInfo(&regionInfo, programNo, key, velocity))
                                                        {
                                                            SEAD_ASSERT_MSG(false, "Invalid code smh");
                                                            continue;
                                                        }

                                                        ImGui::Text("WaveArchiveId: 0x%08X", regionInfo.waveArchiveId);
                                                        ImGui::Text("WaveIndex: 0x%08X", regionInfo.waveIndex);
                                                        ImGui::Text("originalKey: %d", regionInfo.originalKey);
                                                        ImGui::Text("volume: %d", regionInfo.volume);
                                                        ImGui::Text("pan: %d", regionInfo.pan);
                                                        ImGui::Text("pitch: %f", regionInfo.pitch);
                                                        ImGui::Text("isIgnoreNoteOff: %d", regionInfo.isIgnoreNoteOff);
                                                        ImGui::Text("keyGroup: %d", regionInfo.keyGroup);
                                                        ImGui::Text("interpolationType: %d", regionInfo.interpolationType);

                                                        //ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                                                        bool ret = ImGui::TreeNode("ADSHR Curve");
                                                        //ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                                                        if (ret)
                                                        {
                                                            ImGui::Text("attack: %d", regionInfo.adshrCurve.attack);
                                                            ImGui::Text("decay: %d", regionInfo.adshrCurve.decay);
                                                            ImGui::Text("sustain: %d", regionInfo.adshrCurve.sustain);
                                                            ImGui::Text("hold: %d", regionInfo.adshrCurve.hold);
                                                            ImGui::Text("release: %d", regionInfo.adshrCurve.release);

                                                            ImGui::TreePop();
                                                        }

                                                        if (ImGui::Button("Play"))
                                                        {
                                                            const void* warcFile = sSoundArchive->detail_GetFileAddress(sSoundArchive->GetItemFileId(regionInfo.waveArchiveId));
                                                            SEAD_ASSERT(warcFile);

                                                            nw::snd::internal::WaveArchiveFileReader reader(warcFile, false);

                                                            const void* waveFile = reader.GetWaveFile(regionInfo.waveIndex);
                                                            SEAD_ASSERT(waveFile);

                                                            nw::snd::internal::WaveFileReader waveReader(waveFile);

                                                            nw::snd::internal::WaveInfo waveInfo;
                                                            bool ret = waveReader.ReadWaveInfo(&waveInfo);
                                                            SEAD_ASSERT(ret);

                                                            PlayWave(waveInfo);
                                                        }

                                                        ImGui::TreePop();
                                                    }
                                                }

                                                ImGui::TreePop();
                                            }
                                            ImGui::TreePop();
                                        }
                                    }
                                    ImGui::TreePop();
                                }
                                ImGui::TreePop();
                            }
                        }

                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("All WaveArchives"))
        {
            for (u32 i = 0; i < sSoundArchive->mInfoBlockBody->GetWaveArchiveCount(); i++)
            {
                const nw::snd::internal::SoundArchiveFile::WaveArchiveInfo* info = sSoundArchive->mInfoBlockBody->GetWaveArchiveInfo(i + 0x05000000);
                if (!info)
                    continue;

                if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("%s: %u", sSoundArchive->GetString(info->GetStringId()), i).cstr()))
                {
                    ImGui::Text(" - fileId: 0x%08X", (u32)info->fileId);
                    ImGui::Text(" - isLoadIndividual: %d", info->isLoadIndividual);
                    ImGui::Text(" - padding1: %d", info->padding1);
                    ImGui::Text(" - padding2: %d", info->padding2);
                    ImGui::Text(" - StringId: 0x%08X", info->GetStringId());
                    ImGui::Text(" - WaveCount: %d", info->GetWaveCount());

                    if (ImGui::TreeNode("Reader"))
                    {
                        nw::snd::internal::WaveArchiveFileReader reader(sSoundArchive->detail_GetFileAddress(info->fileId), info->isLoadIndividual);
                        //reader.InitializeFileTable();

                        ImGui::Text("WaveCount: %d", reader.GetWaveFileCount());
                        if (ImGui::TreeNode("Waves"))
                        {
                            for (u32 waveNum = 0; waveNum < reader.GetWaveFileCount(); waveNum++)
                            {
                                if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("Wave: %d", waveNum).cstr()))
                                {
                                    ImGui::Text("WaveFileSize: 0x%08X", reader.GetWaveFileSize(waveNum));
                                    ImGui::Text("WaveFileOffsetFromFileHead: 0x%08X", reader.GetWaveFileOffsetFromFileHead(waveNum));
                                    ImGui::Text("WaveFile: 0x%p", reader.GetWaveFile(waveNum));

                                    if (ImGui::TreeNode("Reader"))
                                    {
                                        nw::snd::internal::WaveFileReader waveReader(reader.GetWaveFile(waveNum));

                                        nw::snd::internal::WaveInfo waveInfo;
                                        bool ret = waveReader.ReadWaveInfo(&waveInfo);
                                        SEAD_ASSERT(ret);

                                        ImGui::Text("sampleFormat: %d", waveInfo.sampleFormat);
                                        ImGui::Text("loopFlag: %d", waveInfo.loopFlag);
                                        ImGui::Text("channelCount: %d", waveInfo.channelCount);
                                        ImGui::Text("sampleRate: %d", waveInfo.sampleRate);
                                        ImGui::Text("loopStartFrame: %u", waveInfo.loopStartFrame);
                                        ImGui::Text("loopEndFrame: %u", waveInfo.loopEndFrame);

                                        if (ImGui::Button("Play"))
                                        {
                                            PlayWave(waveInfo);
                                        }

                                        ImGui::TreePop();
                                    }

                                    ImGui::TreePop();
                                }
                            }

                            ImGui::TreePop();
                        }

                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("All Groups"))
        {
            for (u32 i = 0; i < sSoundArchive->mInfoBlockBody->GetGroupCount(); i++)
            {
                const nw::snd::internal::SoundArchiveFile::GroupInfo* group = sSoundArchive->mInfoBlockBody->GetGroupInfo(i + 0x06000000);
                if (!group)
                    continue;

                if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("%s: %u", sSoundArchive->GetString(group->GetStringId()), i).cstr()))
                {
                    ImGui::Text(" - fileId: 0x%08X", (u32)group->fileId);
                    ImGui::Text(" - StringId: 0x%08X", group->GetStringId());

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("All Players"))
        {
            for (u32 i = 0; i < sSoundArchive->mInfoBlockBody->GetPlayerCount(); i++)
            {
                const nw::snd::internal::SoundArchiveFile::PlayerInfo* player = sSoundArchive->mInfoBlockBody->GetPlayerInfo(i + 0x04000000);
                if (!player)
                    continue;

                if (ImGui::TreeNode(sead::FormatFixedSafeString<256>("%s: %u", sSoundArchive->GetString(player->GetStringId()), i).cstr()))
                {
                    ImGui::Text(" - playableSoundMax: %d", (u32)player->playableSoundMax);
                    ImGui::Text(" - StringId: 0x%08X", player->GetStringId());
                    ImGui::Text(" - PlayerHeapSize: 0x%08X", player->GetPlayerHeapSize());

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("All Files"))
        {
            for (u32 i = 0; i < sSoundArchive->mInfoBlockBody->GetFileCount(); i++)
            {
                const nw::snd::internal::SoundArchiveFile::FileInfo* file = sSoundArchive->mInfoBlockBody->GetFileInfo(i);
                if (!file)
                    continue;

                if (ImGui::TreeNode(sead::FormatFixedSafeString<256>(" - %u", i).cstr()))
                {
                    const char* fileTypes[] = {
                        "FILE_LOCATION_TYPE_INTERNAL",
                        "FILE_LOCATION_TYPE_EXTERNAL",
                        "FILE_LOCATION_TYPE_NONE"
                    };

                    ImGui::Text(" - Type: %s", fileTypes[file->GetFileLocationType()]);

                    switch (file->GetFileLocationType())
                    {
                        case nw::snd::internal::SoundArchiveFile::FILE_LOCATION_TYPE_INTERNAL:
                        {
                            const nw::snd::internal::SoundArchiveFile::InternalFileInfo* internalFile = file->GetInternalFileInfo();

                            ImGui::Text(" - FileSize: 0x%08X", internalFile->GetFileSize());
                            ImGui::Text(" - OffsetFromFileBlockHead: 0x%08X", internalFile->GetOffsetFromFileBlockHead());

                            const nw::snd::internal::Util::Table<nw::ut::ResU32>* groupTable = internalFile->GetAttachedGroupTable();
                            SEAD_ASSERT(groupTable);

                            ImGui::Text("GroupTable count: %d", (u32)groupTable->count);
                            for (u32 j = 0; j < groupTable->count; j++)
                            {
                                ImGui::Text("Group%d: 0x%08X", j, (u32)groupTable->item[j]);
                            }
                            break;
                        }

                        case nw::snd::internal::SoundArchiveFile::FILE_LOCATION_TYPE_EXTERNAL:
                        {
                            const nw::snd::internal::SoundArchiveFile::ExternalFileInfo* externalFile = file->GetExternalFileInfo();

                            ImGui::Text(" - filePath: %s", externalFile->filePath);
                            break;
                        }

                        default:
                            SEAD_ASSERT_MSG(false, "Not a valid FileLocationType(%d)", static_cast<s32>(file->GetFileLocationType()));
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        static u32 fileId = 0;
        ImGui::DragInt("fileId", (s32*)&fileId, 1.0f, 0, sSoundArchive->detail_GetFileCount() - 1);

        const nw::snd::internal::SoundArchiveFile::FileInfo* fileInfo = sSoundArchive->detail_GetFileInfo(fileId);
        if (fileInfo)
        {
            const nw::snd::internal::SoundArchiveFile::InternalFileInfo* internalInfo = fileInfo->GetInternalFileInfo();
            if (internalInfo)
            {
                u32 fileSize = internalInfo->GetFileSize();
                u32 offset = internalInfo->GetOffsetFromFileBlockHead();
                ImGui::Text("FileSize: 0x%08X", fileSize);
                ImGui::Text("Offset: 0x%08X", offset);

                const void* file = sSoundArchive->detail_GetFileAddress(fileId);
                if (file)
                {
                    if (fileSize == 0xFFFFFFFF)
                    {
                        const nw::ut::BinaryFileHeader* header = static_cast<const nw::ut::BinaryFileHeader*>(file);
                        fileSize = header->fileSize;
                    }

                    static sead::FixedSafeString<256> path;
                    ImGui::InputTextWithHint("Out Name", "path", path.getBuffer(), path.getBufferSize());

                    if (ImGui::Button("Export") && !path.isEmpty())
                    {
                        sead::FileDevice::SaveArg arg;
                        arg.path = path;
                        arg.buffer = (const u8*)file;
                        arg.buffer_size = fileSize;

                        sead::FileDeviceMgr::instance()->save(arg);
                    }

                    static MemoryEditor memEdit;
                    memEdit.DrawWindow("Memory Editor", (void*)file, fileSize);
                }
                else
                {
                    ImGui::Text("File is nullptr");
                }
            }
            else
            {
                ImGui::Text("InternalFileInfo not present");
            }
        }
        else
        {
            ImGui::Text("FileInfo not present");
        }
    }
    ImGui::End();
}

#include <stream/seadFileDeviceStream.h>

static void LoadWav(const sead::SafeString& file, snd::WaveBuffer* leftBuffer, snd::WaveBuffer* rightBuffer, snd::SampleFormat* outFormat, u32* outSampleRate, u32* outChannels, sead::Heap* heap)
{
    sead::FileHandle handle;
    sead::FileDeviceMgr::instance()->open(&handle, file, sead::FileDevice::FileOpenFlag::eReadOnly, 0);

    sead::FileDeviceReadStream stream(&handle, sead::Stream::Modes::eBinary);

    struct WaveRiffHeader
    {
        char groupId[4];
        s32 size;
        char riffType[4];
    };

    struct WaveFormat
    {
        char chunkId[4];
        s32 chunkSize;
        s16 wFormatTag;
        u16 wChannels;
        u32 dwSamplesPerSec;
        u32 dwAvgBytesPerSec;
        u16 wBlockAlign;
        u16 wBitsPerSample;
    };

    struct WaveData
    {
        char chunkId[4];
        s32 chunkSize;
        // u8 samples[];
    };

    WaveRiffHeader header;
    stream.readMemBlock(&header, sizeof(WaveRiffHeader));

    if (sead::MemUtil::compare(header.groupId, "RIFF", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "not a RIFF file");
        return;
    }

    if (sead::MemUtil::compare(header.riffType, "WAVE", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "not a WAVE format");
        return;
    }

    WaveFormat format;
    stream.readMemBlock(&format, sizeof(WaveFormat));

    if (sead::MemUtil::compare(format.chunkId, "fmt ", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "no fmt header");
        return;
    }

    if (format.wBitsPerSample == 8)
    {
        *outFormat = snd::SampleFormat::PcmS8;
    }
    else if (format.wBitsPerSample == 16)
    {
        *outFormat = snd::SampleFormat::PcmS16;
    }
    else if (format.wBitsPerSample == 32)
    {
        *outFormat = snd::SampleFormat::PcmS32;
    }
    else
    {
        SEAD_ASSERT_MSG(false, "invalid wBitsPerSample[%u]", format.wBitsPerSample);
        return;
    }

    *outSampleRate = format.dwSamplesPerSec;

    WaveData data;
    stream.readMemBlock(&data, sizeof(WaveData));

    if (sead::MemUtil::compare(data.chunkId, "data", 4) != 0)
    {
        SEAD_ASSERT_MSG(false, "no data header");
        return;
    }

    u32 numChannels = format.wChannels;
    SEAD_ASSERT_MSG(numChannels == 1 || numChannels == 2, "invalid wChannels[%u]", numChannels);

    *outChannels = numChannels;

    u32 numSamples = data.chunkSize;

    void* samples = new(heap) u8[numSamples];
    stream.readMemBlock(samples, numSamples);

    leftBuffer->initialize();
    rightBuffer->initialize();

    if (numChannels == 1)
    {
        leftBuffer->bufferAddress = samples;
        leftBuffer->sampleLength = numSamples / (format.wBitsPerSample / 8);
        leftBuffer->loopFlag = true;
        return;
    }

    u8* leftSamples = new(heap) u8[numSamples * (format.wBitsPerSample / 8) / 2];
    u8* rightSamples = new(heap) u8[numSamples * (format.wBitsPerSample / 8) / 2];

    for (u32 i = 0; i < numSamples / 2; i++)
    {
        if (*outFormat == snd::SampleFormat::PcmS8)
        {
            u8* samples8 = reinterpret_cast<u8*>(samples);
            u8* leftSamples8 = reinterpret_cast<u8*>(leftSamples);
            u8* rightSamples8 = reinterpret_cast<u8*>(rightSamples);

            leftSamples8[i] = samples8[i];
            rightSamples8[i] = samples8[i + 1];
        }
        else if (*outFormat == snd::SampleFormat::PcmS16)
        {
            u16* samples16 = reinterpret_cast<u16*>(samples);
            u16* leftSamples16 = reinterpret_cast<u16*>(leftSamples);
            u16* rightSamples16 = reinterpret_cast<u16*>(rightSamples);

            leftSamples16[i] = samples16[i];
            rightSamples16[i] = samples16[i + 1];
        }
        else if (*outFormat == snd::SampleFormat::PcmS32)
        {
            u32* samples32 = reinterpret_cast<u32*>(samples);
            u32* leftSamples32 = reinterpret_cast<u32*>(leftSamples);
            u32* rightSamples32 = reinterpret_cast<u32*>(rightSamples);

            leftSamples32[i] = samples32[i];
            rightSamples32[i] = samples32[i + 1];
        }
    }

    delete[] samples;

    leftBuffer->bufferAddress = leftSamples;
    leftBuffer->sampleLength = numSamples / 2;
    leftBuffer->loopFlag = true;

    rightBuffer->bufferAddress = rightSamples;
    rightBuffer->sampleLength = numSamples / 2;
    rightBuffer->loopFlag = true;
}
