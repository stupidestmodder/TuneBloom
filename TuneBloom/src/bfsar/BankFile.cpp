#include <bfsar/BankFile.h>

#include <bfsar/SeqCommand.h>

#include <ui/UI.h>

#include <VectorSet.h>

#include <algorithm>
#include <functional>
#include <vector>

static bool KeyPresed[128] = { false };

static bool KeyboardFunc(void* UserData, s32 Msg, s32 Key, f32 Vel)
{
    if (Key < 0 || Key >= 128)
    {
        return false; // Midi max keys
    }

    Item* item = static_cast<Item*>(UserData);
    if (!item || item->getItemType() != Item::ItemType::BankFileInstrument)
    {
        return false;
    }

    BankFile::Instrument* instrument = static_cast<BankFile::Instrument*>(item);

    switch (Msg)
    {
        case NoteGetStatus:
        {
            return KeyPresed[Key];
        }

        case NoteOn:
        {
            KeyPresed[Key] = true;

            const BankFile::KeyRegion* keyRegion = instrument->getKeyRegion(Key);
            if (!keyRegion)
            {
                return false;
            }

            u8 vel = static_cast<u8>(Vel * 127.0f);

            const BankFile::VelocityRegion* velocityRegion = keyRegion->getVelocityRegion(vel);
            SEAD_ASSERT(velocityRegion);

            sSoundPlayer.playBankNote(static_cast<u8>(Key), vel, *velocityRegion);

            break;
        }

        case NoteOff:
        {
            KeyPresed[Key] = false;

            sSoundPlayer.stopAllPlayers(false);

            break;
        }
    }

    return false;
}

void BankFile::VelocityRegion::read(const nw::snd::internal::BankFile::VelocityRegion* velocityRegionInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable)
{
    const nw::snd::internal::Util::WaveId* waveId = waveIdTable.GetWaveId(velocityRegionInfo->waveIdTableIndex);
    SEAD_ASSERT(waveId);

    //? waveId->waveIndex is patched with global wave index already
    SEAD_ASSERT(waveId->waveArchiveId == 0);
    WaveFile* waveFile = static_cast<WaveFile*>(sBfsar.getItem(waveId->waveIndex, sBfsar.getWaveFileList()));
    mWaveFileRef.attach(waveFile);

    mOriginalKey = velocityRegionInfo->GetOriginalKey();
    mVolume = velocityRegionInfo->GetVolume();
    mPan = velocityRegionInfo->GetPan();
    mPitch = velocityRegionInfo->GetPitch();
    mIsIgnoreNoteOff = velocityRegionInfo->IsIgnoreNoteOff();
    mKeyGroup = velocityRegionInfo->GetKeyGroup();
    mInterpolationType = velocityRegionInfo->GetInterpolationType();

    const nw::snd::AdshrCurve& adshrCurveInfo = velocityRegionInfo->GetAdshrCurve();
    mAdshrCurve.attack = adshrCurveInfo.attack;
    mAdshrCurve.decay = adshrCurveInfo.decay;
    mAdshrCurve.sustain = adshrCurveInfo.sustain;
    mAdshrCurve.hold = adshrCurveInfo.hold;
    mAdshrCurve.release = adshrCurveInfo.release;
}

void BankFile::VelocityRegion::drawUI()
{
    static const ImU8 cStepU8 = 1;

    if (sSelectedItem && sSelectedItem->getItemType() == Item::ItemType::BankFileInstrument)
    {
        Instrument* instr = static_cast<Instrument*>(sSelectedItem);
        instr->drawUI();

        ImGui::SeparatorText("");
    }

    {
        Item* waveFile = getWaveFileRef().getItem();
        if (ItemSelector("Wave File", sBfsar.getWaveFileList(), &waveFile))
        {
            getWaveFileRef().attach(waveFile);
        }
    }

    {
        u8 originalKey = getOriginalKey();
        if (ImGui::InputScalar("Original Key", ImGuiDataType_U8, &originalKey, &cStepU8))
        {
            setOriginalKey(originalKey);
        }
    }

    {
        u8 volume = getVolume();
        if (ImGui::InputScalar(sead::FormatFixedSafeString<32>("Volume (%.3f)###vol", static_cast<f32>(volume) / 127.0f).cstr(), ImGuiDataType_U8, &volume, &cStepU8))
        {
            setVolume(volume);
        }
    }

    {
        u8 pan = getPan();
        if (ImGui::InputScalar("Pan", ImGuiDataType_U8, &pan, &cStepU8))
        {
            setPan(pan);
        }
    }

    {
        f32 pitch = getPitch();
        if (ImGui::SliderFloat("Pitch", &pitch, 0.0f, 8.0f))
        {
            setPitch(pitch);
        }
    }

    {
        bool ignoreNoteOff = getIsIgnoreNoteOff();
        if (ImGui::Checkbox("Ignore Note Off (Percussion Mode)", &ignoreNoteOff))
        {
            setIsIgnoreNoteOff(ignoreNoteOff);
        }
    }

    {
        u8 keyGroup = getKeyGroup();
        if (ImGui::InputScalar("Key Group", ImGuiDataType_U8, &keyGroup, &cStepU8))
        {
            setKeyGroup(keyGroup);
        }
    }

    {
        static const char* sInterpolationTypes[] = { 
          //"Polyphase (4-point) interpolation",
            "Polyphase (4-point)",
          //"Linear interpolation",
            "Linear",
        };

        u32 interpolationType = getInterpolationType();
        if (ImGui::Combo("Interpolation Type", (s32*)&interpolationType, sInterpolationTypes, IM_ARRAYSIZE(sInterpolationTypes)))
        {
            setInterpolationType(interpolationType);
        }
    }

    {
        snd::AdshrCurve adshrCurve = getAdshrCurve();

        bool edited = false;
        if (ImGui::InputScalar("Attack", ImGuiDataType_U8, &adshrCurve.attack, &cStepU8))
        {
            edited = true;
        }

        if (ImGui::InputScalar("Decay", ImGuiDataType_U8, &adshrCurve.decay, &cStepU8))
        {
            edited = true;
        }

        if (ImGui::InputScalar("Sustain", ImGuiDataType_U8, &adshrCurve.sustain, &cStepU8))
        {
            edited = true;
        }

        if (ImGui::InputScalar("Hold", ImGuiDataType_U8, &adshrCurve.hold, &cStepU8))
        {
            edited = true;
        }

        if (ImGui::InputScalar("Release", ImGuiDataType_U8, &adshrCurve.release, &cStepU8))
        {
            edited = true;
        }

        if (edited)
        {
            setAdshrCurve(adshrCurve);
        }
    }
}

void BankFile::KeyRegion::read(const nw::snd::internal::BankFile::KeyRegion* keyRegionInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable)
{
    SEAD_ASSERT(keyRegionInfo);

    switch (nw::snd::internal::GetRegionType(keyRegionInfo->toVelocityRegionChunk.typeId))
    {
        case nw::snd::internal::REGION_TYPE_DIRECT:
        {
            const nw::snd::internal::DirectChunk& directChunk = keyRegionInfo->GetDirectChunk();
            SEAD_ASSERT(directChunk.toRegion.typeId == nw::snd::internal::ElementType_BankFile_VelocityRegionInfo);
            SEAD_ASSERT(directChunk.toRegion.offset != nw::snd::internal::Util::Reference::INVALID_OFFSET);

            const nw::snd::internal::BankFile::VelocityRegion* velocityRegionInfo = static_cast<const nw::snd::internal::BankFile::VelocityRegion*>(directChunk.GetRegion());

            VelocityRegion* velocityRegion = new VelocityRegion(0, 127);
            velocityRegion->mId = 0;

            velocityRegion->mEnableName = true;
            velocityRegion->mName = "VelocityRegion";

            velocityRegion->read(velocityRegionInfo, waveIdTable);

            mVelocityRegionList.pushBack(velocityRegion);
            break;
        }

        case nw::snd::internal::REGION_TYPE_RANGE:
        {
            const nw::snd::internal::RangeChunk& rangeChunk = keyRegionInfo->GetRangeChunk();

            u32 velocityRegionCount = rangeChunk.borderTable.count;

            u8 velocityMin = 0;
            for (u32 i = 0; i < velocityRegionCount; i++)
            {
                const nw::snd::internal::Util::Reference* velocityRegionRef = static_cast<const nw::snd::internal::Util::Reference*>(rangeChunk.GetRegionRef(velocityMin));
                SEAD_ASSERT(velocityRegionRef);
                SEAD_ASSERT(velocityRegionRef->typeId == nw::snd::internal::ElementType_BankFile_VelocityRegionInfo);
                SEAD_ASSERT(velocityRegionRef->offset != nw::snd::internal::Util::Reference::INVALID_OFFSET);

                const nw::snd::internal::BankFile::VelocityRegion* velocityRegionInfo = static_cast<const nw::snd::internal::BankFile::VelocityRegion*>(rangeChunk.GetRegion(velocityMin));
                SEAD_ASSERT(velocityRegionInfo);

                u8 velocityMax = rangeChunk.borderTable.item[i];

                VelocityRegion* velocityRegion = new VelocityRegion(velocityMin, velocityMax);
                velocityRegion->mId = mVelocityRegionList.size();

                velocityRegion->mEnableName = true;
                velocityRegion->mName = "VelocityRegion";

                velocityRegion->read(velocityRegionInfo, waveIdTable);

                mVelocityRegionList.pushBack(velocityRegion);

                velocityMin = velocityMax + 1;
            }

            break;
        }

        case nw::snd::internal::REGION_TYPE_INDEX:
        {
            const nw::snd::internal::IndexChunk& indexChunk = keyRegionInfo->GetIndexChunk();

            std::vector<u8> borders;

            s32 prevOffset = nw::snd::internal::Util::Reference::INVALID_OFFSET;
            for (u32 i = indexChunk.min; i <= indexChunk.max; i++)
            {
                const nw::snd::internal::Util::Reference* velocityRegionRef = static_cast<const nw::snd::internal::Util::Reference*>(indexChunk.GetRegionRef(i));
                SEAD_ASSERT(velocityRegionRef);

                if (velocityRegionRef->offset != prevOffset)
                {
                    if (i != 0)
                    {
                        borders.push_back(i - 1);
                    }
                }

                prevOffset = velocityRegionRef->offset;
            }

            borders.push_back(indexChunk.max);

            u8 velocityMin = 0;
            for (u32 i = 0; i < borders.size(); i++)
            {
                const nw::snd::internal::Util::Reference* velocityRegionRef = static_cast<const nw::snd::internal::Util::Reference*>(indexChunk.GetRegionRef(velocityMin));
                SEAD_ASSERT(velocityRegionRef);
                SEAD_ASSERT(velocityRegionRef->typeId == nw::snd::internal::ElementType_BankFile_VelocityRegionInfo);
                SEAD_ASSERT(velocityRegionRef->offset != nw::snd::internal::Util::Reference::INVALID_OFFSET);

                const nw::snd::internal::BankFile::VelocityRegion* velocityRegionInfo = static_cast<const nw::snd::internal::BankFile::VelocityRegion*>(indexChunk.GetRegion(velocityMin));
                SEAD_ASSERT(velocityRegionInfo);

                u8 velocityMax = borders[i];

                VelocityRegion* velocityRegion = new VelocityRegion(velocityMin, velocityMax);
                velocityRegion->mId = mVelocityRegionList.size();

                velocityRegion->mEnableName = true;
                velocityRegion->mName = "VelocityRegion";

                velocityRegion->read(velocityRegionInfo, waveIdTable);

                mVelocityRegionList.pushBack(velocityRegion);

                velocityMin = velocityMax + 1;
            }
        }

        default:
            SEAD_ASSERT(false);
            break;
    }

    SEAD_ASSERT(mVelocityRegionList.size() > 0);
}

void BankFile::Instrument::read(const nw::snd::internal::BankFile::Instrument* instrumentInfo, const nw::snd::internal::Util::WaveIdTable& waveIdTable)
{
    SEAD_ASSERT(instrumentInfo);

    switch (nw::snd::internal::GetRegionType(instrumentInfo->toKeyRegionChunk.typeId))
    {
        case nw::snd::internal::REGION_TYPE_DIRECT:
        {
            const nw::snd::internal::DirectChunk& directChunk = instrumentInfo->GetDirectChunk();
            SEAD_ASSERT(directChunk.toRegion.typeId == nw::snd::internal::ElementType_BankFile_KeyRegionInfo);
            SEAD_ASSERT(directChunk.toRegion.offset != nw::snd::internal::Util::Reference::INVALID_OFFSET);

            const nw::snd::internal::BankFile::KeyRegion* keyRegionInfo = static_cast<const nw::snd::internal::BankFile::KeyRegion*>(directChunk.GetRegion());

            KeyRegion* keyRegion = new KeyRegion(0, 127);
            keyRegion->mId = 0;

            keyRegion->mEnableName = true;
            keyRegion->mName = "KeyRegion";

            keyRegion->read(keyRegionInfo, waveIdTable);

            mKeyRegionList.pushBack(keyRegion);
            break;
        }

        case nw::snd::internal::REGION_TYPE_RANGE:
        {
            const nw::snd::internal::RangeChunk& rangeChunk = instrumentInfo->GetRangeChunk();

            u32 keyRegionCount = rangeChunk.borderTable.count;

            u8 keyMin = 0;
            for (u32 i = 0; i < keyRegionCount; i++)
            {
                const nw::snd::internal::Util::Reference* keyRegionRef = static_cast<const nw::snd::internal::Util::Reference*>(rangeChunk.GetRegionRef(keyMin));
                SEAD_ASSERT(keyRegionRef);

                u8 keyMax = rangeChunk.borderTable.item[i];

                if (keyRegionRef->offset == nw::snd::internal::Util::Reference::INVALID_OFFSET)
                {
                    SEAD_ASSERT(keyRegionRef->typeId == nw::snd::internal::ElementType_BankFile_NullInfo); //? BFSARs saved with Citric break this assertion...

                    keyMin = keyMax + 1;
                    continue;
                }
                else
                {
                    SEAD_ASSERT(keyRegionRef->typeId == nw::snd::internal::ElementType_BankFile_KeyRegionInfo);
                }

                const nw::snd::internal::BankFile::KeyRegion* keyRegionInfo = static_cast<const nw::snd::internal::BankFile::KeyRegion*>(rangeChunk.GetRegion(keyMin));
                SEAD_ASSERT(keyRegionInfo);

                KeyRegion* keyRegion = new KeyRegion(keyMin, keyMax);
                keyRegion->mId = mKeyRegionList.size();

                keyRegion->mEnableName = true;
                keyRegion->mName = "KeyRegion";

                keyRegion->read(keyRegionInfo, waveIdTable);

                mKeyRegionList.pushBack(keyRegion);

                keyMin = keyMax + 1;
            }

            break;
        }

        case nw::snd::internal::REGION_TYPE_INDEX:
        {
            const nw::snd::internal::IndexChunk& indexChunk = instrumentInfo->GetIndexChunk();

            std::vector<u8> borders;

            s32 prevOffset = nw::snd::internal::Util::Reference::INVALID_OFFSET;
            for (u32 i = indexChunk.min; i <= indexChunk.max; i++)
            {
                const nw::snd::internal::Util::Reference* keyRegionRef = static_cast<const nw::snd::internal::Util::Reference*>(indexChunk.GetRegionRef(i));
                SEAD_ASSERT(keyRegionRef);

                if (keyRegionRef->offset != prevOffset)
                {
                    if (i != 0)
                    {
                        borders.push_back(i - 1);
                    }
                }

                prevOffset = keyRegionRef->offset;
            }

            borders.push_back(indexChunk.max);

            u8 keyMin = 0;
            for (u32 i = 0; i < borders.size(); i++)
            {
                const nw::snd::internal::Util::Reference* keyRegionRef = static_cast<const nw::snd::internal::Util::Reference*>(indexChunk.GetRegionRef(keyMin));
                SEAD_ASSERT(keyRegionRef);

                u8 keyMax = borders[i];

                if (keyRegionRef->offset == nw::snd::internal::Util::Reference::INVALID_OFFSET)
                {
                    SEAD_ASSERT(keyRegionRef->typeId == nw::snd::internal::ElementType_BankFile_NullInfo);

                    keyMin = keyMax + 1;
                    continue;
                }
                else
                {
                    SEAD_ASSERT(keyRegionRef->typeId == nw::snd::internal::ElementType_BankFile_KeyRegionInfo);
                }

                const nw::snd::internal::BankFile::KeyRegion* keyRegionInfo = static_cast<const nw::snd::internal::BankFile::KeyRegion*>(indexChunk.GetRegion(keyMin));
                SEAD_ASSERT(keyRegionInfo);

                KeyRegion* keyRegion = new KeyRegion(keyMin, keyMax);
                keyRegion->mId = mKeyRegionList.size();

                keyRegion->mEnableName = true;
                keyRegion->mName = "KeyRegion";

                keyRegion->read(keyRegionInfo, waveIdTable);

                mKeyRegionList.pushBack(keyRegion);

                keyMin = keyMax + 1;
            }

            break;
        }

        default:
            SEAD_ASSERT(false);
            break;
    }

    SEAD_ASSERT(mKeyRegionList.size() > 0);
}

void BankFile::Instrument::drawUI()
{
    static ImS16 cStepS16 = 1;
    {
        s16 program = getProgramNo();
        if (ImGui::InputScalar("Program", ImGuiDataType_S16, &program, &cStepS16))
        {
            setProgramNo(program);
        }
    }
}

BankFile::~BankFile()
{
    if (sSoundPlayer.isCurrentPlayerSequence() && sSoundPlayer.isActive())
    {
        sSoundPlayer.invalidateBankFile(*this);
    }
}

const Item* BankFile::validate(sead::BufferedSafeString& error) const
{
    std::unordered_set<s16> programNos;

    u32 i = 0;
    for (const Item* instrumentItem : getInstrumentList())
    {
        SEAD_ASSERT(instrumentItem->getItemType() == Item::ItemType::BankFileInstrument);
        const BankFile::Instrument* instrument = static_cast<const BankFile::Instrument*>(instrumentItem);

        if (instrument->getProgramNo() < 0)
        {
            error.format("Instrument %u has a invalid program number", i);
            return instrument;
        }

        if (programNos.count(instrument->getProgramNo()) > 0)
        {
            error.format("Instrument %u: Program number '%d' already exists", i, instrument->getProgramNo());
            return instrument;
        }

        programNos.insert(instrument->getProgramNo());

        for (const Item* keyRegionItem : instrument->getKeyRegionList())
        {
            SEAD_ASSERT(keyRegionItem->getItemType() == Item::ItemType::BankFileKeyRegion);
            const BankFile::KeyRegion* keyRegion = static_cast<const BankFile::KeyRegion*>(keyRegionItem);

            for (const Item* velocityRegionItem : keyRegion->getVelocityRegionList())
            {
                SEAD_ASSERT(velocityRegionItem->getItemType() == Item::ItemType::BankFileVelocityRegion);
                const BankFile::VelocityRegion* velocityRegion = static_cast<const BankFile::VelocityRegion*>(velocityRegionItem);

                if (!velocityRegion->getWaveFileRef().isAttached())
                {
                    error.format("Instrument %u: Velocity Region has no Wave File attached", i);
                    return instrument;
                }
            }
        }

        i++;
    }

    return nullptr;
}

void BankFile::drawUI()
{
    mVersion = sBfsar.getVersionForBfbnk();
    mEndian = sBfsar.getEndian();

    HelpMarker("Those are derived from the BFSAR");

    ImGui::BeginDisabled();
    InnerFile::drawUI();
    ImGui::EndDisabled();
}

static BankFile::KeyRegion* sContextKeyRegion = nullptr;

void SelectVelocity(BankFile::KeyRegion* keyRegion, BankFile::VelocityRegion* velocityRegion)
{
    sSubSelectedItem = velocityRegion;
    sSelectedItemIsSubWindow = true;
    sContextKeyRegion = keyRegion;
}

void DeselectVelocity()
{
    sSubSelectedItem = nullptr;
    sSelectedItemIsSubWindow = false;
    sContextKeyRegion = nullptr;
}

void DeleteVeloctity(BankFile::KeyRegion* keyRegion, BankFile::VelocityRegion* velocityRegion)
{
    DeselectVelocity();

    snd::internal::driver::SoundThreadLock lock;

    if (keyRegion->getVelocityRegionList().size() == 1) //? Deleted last VelocityRegion, this KeyRegion can die now
    {
        delete keyRegion;
    }
    else
    {
        //? VelocityRegions can't contain gaps, so handle that

        if (velocityRegion == keyRegion->getVelocityRegionList().front()) //? VelocityRegion is the bottom one, extend the one above to cover it
        {
            velocityRegion->getNext(*keyRegion)->setVelocityMin(0);
        }
        else if (velocityRegion == keyRegion->getVelocityRegionList().back()) //? VelocityRegion is the top one, extend the one below to cover it
        {
            velocityRegion->getPrev(*keyRegion)->setVelocityMax(127);
        }
        else //? VelocityRegion is sandwiched, extend the one below to cover it
        {
            velocityRegion->getPrev(*keyRegion)->setVelocityMax(velocityRegion->getVelocityMax());
        }

        delete velocityRegion;
    }
}

void VelocityContextMenu(BankFile::Instrument* instrument, BankFile::KeyRegion* keyRegion, BankFile::VelocityRegion* velocityRegion)
{
    auto copyVel = [](BankFile::VelocityRegion* dst, BankFile::VelocityRegion* src)
    {
        dst->getWaveFileRef().attach(src->getWaveFileRef().getItem());
        dst->setOriginalKey(src->getOriginalKey());
        dst->setVolume(src->getVolume());
        dst->setPan(src->getPan());
        dst->setPitch(src->getPitch());
        dst->setIsIgnoreNoteOff(src->getIsIgnoreNoteOff());
        dst->setKeyGroup(src->getKeyGroup());
        dst->setInterpolationType(src->getInterpolationType());
        dst->setAdshrCurve(src->getAdshrCurve());
    };

    if (ImGui::BeginPopup("VelocityRegionContextMenu"))
    {
        bool disable = false;
        if (keyRegion->getVelocityRegionList().size() > 1)
        {
            disable = true;
            ImGui::BeginDisabled();
        }

        if (ImGui::MenuItem("Split Key Region") && keyRegion->getKeyNum() > 1)
        {
            snd::internal::driver::SoundThreadLock lock;

            u8 newKeyMax = keyRegion->getKeyMax();
            u8 newKeyMin = (keyRegion->getKeyMax() + keyRegion->getKeyMin()) / 2 + 1;

            keyRegion->setKeyMax(newKeyMin - 1, *instrument);

            BankFile::KeyRegion* newKeyRegion = new BankFile::KeyRegion(newKeyMin, newKeyMax);
            newKeyRegion->setId(0);
            newKeyRegion->setEnableName(true);
            newKeyRegion->getName() = "KeyRegion";

            keyRegion->insertBack(newKeyRegion);

            sBfsar.updateList(instrument->getKeyRegionList());

            BankFile::VelocityRegion* newVelRegion = new BankFile::VelocityRegion(0, 127);
            newVelRegion->setId(0);
            newVelRegion->setEnableName(true);
            newVelRegion->getName() = "VelocityRegion";
            copyVel(newVelRegion, velocityRegion);

            newKeyRegion->getVelocityRegionList().pushBack(newVelRegion);
        }

        if (disable)
        {
            ImGui::EndDisabled();
        }

        if (ImGui::MenuItem("Split Velocity Region") && velocityRegion->getVelocityNum() > 1)
        {
            snd::internal::driver::SoundThreadLock lock;

            u8 newVelMax = velocityRegion->getVelocityMax();
            u8 newVelMin = (velocityRegion->getVelocityMax() + velocityRegion->getVelocityMin()) / 2 + 1;

            velocityRegion->setVelocityMax(newVelMin - 1);

            BankFile::VelocityRegion* newVelRegion = new BankFile::VelocityRegion(newVelMin, newVelMax);
            newVelRegion->setId(0);
            newVelRegion->setEnableName(true);
            newVelRegion->getName() = "VelocityRegion";
            copyVel(newVelRegion, velocityRegion);

            velocityRegion->insertBack(newVelRegion);

            sBfsar.updateList(keyRegion->getVelocityRegionList());
        }

        if (ImGui::MenuItem("Delete"))
        {
            DeleteVeloctity(keyRegion, velocityRegion);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// YEAHH i clanked it so what

enum class DragMode
{
    None,
    ResizeL,
    ResizeR
};

struct DragState
{
    DragMode mode = DragMode::None;
    BankFile::KeyRegion* region = nullptr;
    BankFile::KeyRegion* prev = nullptr;
    BankFile::KeyRegion* next = nullptr;
    bool onLeftEdge = false;
    bool onRightEdge = false;
    ImVec2 r0;
    ImVec2 r1;

    ImVec2 initialCanvasPos;
};

static DragState sDrag;

static const s32 NoteIsDark[12] = { 0,1,0,1,0,0,1,0,1,0,1,0 };
static const s32 NoteLightNumber[12] = { 1,1,2,2,3,4,4,5,5,6,6,7 };
static const f32 NoteDarkOffset[12] = {
    0.0f, -2.0f/3.0f, 0.0f, -1.0f/3.0f, 0.0f, 0.0f,
    -2.0f/3.0f, 0.0f, -0.5f, 0.0f, -1.0f/3.0f, 0.0f
};

#define IM_ROUND(x) ((f32)(s32)((x) + 0.5f))

void DrawKeyboardWithRegions(
    f32 width,
    f32 keyboardHeight,
    f32 regionHeight,
    s32 beginNote,
    s32 endNote,
    BankFile::Instrument* instrument
)
{
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize(width, regionHeight + keyboardHeight);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    if (sDrag.mode != DragMode::None)
    {
        canvasPos = sDrag.initialCanvasPos;
    }

    s32 fixedBegin = beginNote;
    if (NoteIsDark[fixedBegin % 12] > 0)
        fixedBegin++;

    s32 fixedEnd = endNote;
    if (NoteIsDark[fixedEnd % 12] > 0)
        fixedEnd--;

    s32 countWhite = 0;
    for (s32 i = fixedBegin; i <= fixedEnd; i++)
    {
        if (NoteIsDark[i % 12] == 0)
            countWhite++;
    }
    
    f32 noteWidth = width / (f32)countWhite;
    f32 noteWidth2 = noteWidth * 0.6f;

    auto GetWhiteIndex = [&](s32 key) -> s32
    {
        s32 absWhite = (key / 12) * 7 + NoteLightNumber[key % 12] - 1;
        s32 baseAbsWhite = (fixedBegin / 12) * 7 + NoteLightNumber[fixedBegin % 12] - 1;
        return absWhite - baseAbsWhite;
    };

    auto GetKeyRect = [&](s32 key, f32& outMin, f32& outMax)
    {
        s32 note = key % 12;
        if (NoteIsDark[note])
        {
            f32 baseX = IM_ROUND(GetWhiteIndex(key + 1) * noteWidth);
            f32 offset = NoteDarkOffset[note] * noteWidth2;
            outMin = IM_ROUND(baseX + offset);
            outMax = IM_ROUND(baseX + offset + noteWidth2);
        }
        else
        {
            s32 wIndex = GetWhiteIndex(key);
            outMin = IM_ROUND(wIndex * noteWidth);
            outMax = IM_ROUND((wIndex + 1) * noteWidth);
        }
    };

    auto XToKey = [&](f32 x) -> s32
    {
        f32 local = x - canvasPos.x;
        s32 bestKey = beginNote;
        f32 minDist = 99999.0f;
        for (s32 k = fixedBegin; k <= fixedEnd; k++)
        {
            f32 kMin, kMax;
            GetKeyRect(k, kMin, kMax);

            f32 center = (kMin + kMax) * 0.5f;
            f32 d = fabsf(local - center);
            if (d < minDist)
            {
                minDist = d; bestKey = k;
            }
        }

        return bestKey;
    };

    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    // BG
    draw->AddRectFilled(
        canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(20, 20, 30, 255)
    );

    if (instrument)
    {
        f32 edgeSize = 6.0f;
        static f32 edgeOffset = 0.0f;

        ImDrawListSplitter splitter;
        splitter.Split(draw, 2);

        enum class AddMode
        {
            None,
            Front,
            Back,
            After
        };

        AddMode addMode = AddMode::None;
        BankFile::KeyRegion* addNode = nullptr;
        ImU32 emptyAreaColor = IM_COL32(255, 0, 0, 255);

        if (instrument->getKeyRegionList().size() == 0)
        {
            ImVec2 n0(canvasPos.x, canvasPos.y);
            ImVec2 n1(canvasPos.x + canvasSize.x, canvasPos.y + regionHeight);

            draw->AddRect(n0, n1, emptyAreaColor);
            ImGui::ItemAdd(ImRect(n0, n1), ImGui::GetID(instrument));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                addMode = AddMode::Front;
                addNode = nullptr;
            }
            else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                DeselectVelocity();
            }
            else if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            {
                ImGui::SetTooltip("Double click to add");
            }
        }

        bool drawGrabBar = sDrag.mode != DragMode::None;
        for (Item* keyRegionItem : instrument->getKeyRegionList())
        {
            splitter.SetCurrentChannel(draw, 0);

            BankFile::KeyRegion* keyRegion = static_cast<BankFile::KeyRegion*>(keyRegionItem);

            BankFile::KeyRegion* prev = keyRegion->getPrevNeighbor(*instrument);
            BankFile::KeyRegion* next = keyRegion->getNextNeighbor(*instrument);

            f32 x0, ignored_max;
            GetKeyRect(keyRegion->getKeyMin(), x0, ignored_max);

            f32 ignored_min, x1;
            if (keyRegion->getKeyMax() == 127)
            {
                x1 = canvasSize.x; //? Special case for the last key to fill to the end
            }
            else
            {
                GetKeyRect(keyRegion->getKeyMax() + 1, x1, ignored_min);
            }

            ImVec2 r0(canvasPos.x + x0, canvasPos.y);
            ImVec2 r1(canvasPos.x + x1, canvasPos.y + regionHeight);

            if (keyRegion == instrument->getKeyRegionList().front())
            {
                if (keyRegion->getKeyMin() != 0)
                {
                    ImVec2 n0(canvasPos.x, canvasPos.y);
                    ImVec2 n1(canvasPos.x + x0, canvasPos.y + regionHeight);

                    draw->AddRect(n0, n1, emptyAreaColor);
                    ImGui::ItemAdd(ImRect(n0, n1), ImGui::GetID(keyRegion));
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        addMode = AddMode::Front;
                        addNode = keyRegion;
                    }
                    else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        DeselectVelocity();
                    }
                    else if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                    {
                        ImGui::SetTooltip("Double click to add");
                    }
                }
            }

            if (keyRegion == instrument->getKeyRegionList().back())
            {
                if (keyRegion->getKeyMax() != 127)
                {
                    ImVec2 n0(canvasPos.x + x1, canvasPos.y);
                    ImVec2 n1(canvasPos.x + canvasSize.x, canvasPos.y + regionHeight);

                    draw->AddRect(n0, n1, emptyAreaColor);
                    ImGui::ItemAdd(ImRect(n0, n1), ImGui::GetID(keyRegion));
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        addMode = AddMode::Back;
                        addNode = keyRegion;
                    }
                    else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        DeselectVelocity();
                    }
                    else if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                    {
                        ImGui::SetTooltip("Double click to add");
                    }
                }
            }

            {
                BankFile::KeyRegion* next = keyRegion->getNext(*instrument);
                if (next && keyRegion->getKeyMax() + 1 != next->getKeyMin())
                {
                    f32 next_x0;
                    GetKeyRect(next->getKeyMin(), next_x0, ignored_max);

                    ImVec2 n0(canvasPos.x + x1, canvasPos.y);
                    ImVec2 n1(canvasPos.x + next_x0, canvasPos.y + regionHeight);

                    draw->AddRect(n0, n1, emptyAreaColor);
                    ImGui::ItemAdd(ImRect(n0, n1), ImGui::GetID(keyRegion));
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        addMode = AddMode::After;
                        addNode = keyRegion;
                    }
                    else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        DeselectVelocity();
                    }
                    else if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                    {
                        ImGui::SetTooltip("Double click to add");
                    }
                }
            }

            bool hoveredRegionY = mouse.y >= r0.y && mouse.y <= r1.y;

            bool onLeftEdge  = hoveredRegionY && (mouse.x <= r0.x + edgeSize) && (mouse.x >= r0.x - edgeSize);
            bool onRightEdge = hoveredRegionY && (mouse.x >= r1.x - edgeSize) && (mouse.x <= r1.x + edgeSize);

            if (keyRegion == sDrag.region)
            {
                sDrag.r0 = r0;
                sDrag.r1 = r1;
            }

            for (Item* velRegionItem : keyRegion->getVelocityRegionList())
            {
                BankFile::VelocityRegion* velRegion = static_cast<BankFile::VelocityRegion*>(velRegionItem);

                s32 velMin = velRegion->getVelocityMin();
                s32 velMax = velRegion->getVelocityMax();

                //? Map velocity (0–127) to Y
                f32 y0 = regionHeight * (1.0f - (velMax + 1) / 128.0f);
                f32 y1 = regionHeight * (1.0f - velMin / 128.0f);

                ImVec2 p0 = ImVec2(canvasPos.x + x0, canvasPos.y + y0);
                ImVec2 p1 = ImVec2(canvasPos.x + x1, canvasPos.y + y1);

                ImGui::ItemAdd(ImRect(p0, p1), ImGui::GetID(velRegion));

                if (ImGui::IsItemHovered())
                {
                    drawGrabBar = true;
                }

                ImU32 color = IM_COL32(140, 140, 140, 255);
                if (sSubSelectedItem == velRegion)
                {
                    color = ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Header));
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    SelectVelocity(keyRegion, velRegion);

                    if (sDrag.mode == DragMode::None)
                    {
                        if (onLeftEdge)
                        {
                            sDrag.mode = DragMode::ResizeL;
                            sDrag.onLeftEdge = true;
                            edgeOffset = r0.x - mouse.x;
                        }
                        else if (onRightEdge)
                        {
                            sDrag.mode = DragMode::ResizeR;
                            sDrag.onRightEdge = true;
                            edgeOffset = r1.x - mouse.x;
                        }

                        sDrag.region = keyRegion;
                        sDrag.prev = prev;
                        sDrag.next = next;
                        sDrag.initialCanvasPos = canvasPos;
                    }
                }
                else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                {
                    ImGui::OpenPopup("VelocityRegionContextMenu");
                    SelectVelocity(keyRegion, velRegion);
                }

                draw->AddRectFilled(p0, p1, color);
                draw->AddRect(p0, p1, IM_COL32(0,0,0,255));

                const char* name = "(null)";
                if (velRegion->getWaveFileRef().isAttached())
                {
                    name = velRegion->getWaveFileRef().getItem()->getName().cstr();
                }

                draw->PushClipRect(p0, p1, true);
                draw->AddText(
                    nullptr,
                    0,
                    ImVec2(p0.x + 3, p0.y + 2),
                    IM_COL32(255, 255, 255, 255),
                    name,
                    nullptr,
                    p1.x - p0.x - 6
                );
                draw->PopClipRect();
            }

            splitter.SetCurrentChannel(draw, 1);

            if (drawGrabBar)
            {
                auto drawGrabLeft = [&draw](ImVec2 r0, ImVec2 r1)
                {
                    draw->AddLine(
                        ImVec2(r0.x, r0.y),
                        ImVec2(r0.x, r1.y),
                        IM_COL32(255, 255, 0, 255),
                        2.0f
                    );

                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                };

                auto drawGrabRight = [&draw](ImVec2 r0, ImVec2 r1)
                {
                    draw->AddLine(
                        ImVec2(r1.x, r0.y),
                        ImVec2(r1.x, r1.y),
                        IM_COL32(255, 255, 0, 255),
                        2.0f
                    );

                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                };

                if (sDrag.mode == DragMode::None)
                {
                    if (onLeftEdge)
                    {
                        drawGrabLeft(r0, r1);
                    }
                    else if (onRightEdge)
                    {
                        drawGrabRight(r0, r1);
                    }
                }
                else if (sDrag.mode == DragMode::ResizeL || sDrag.mode == DragMode::ResizeR)
                {
                    if (sDrag.onLeftEdge)
                    {
                        drawGrabLeft(sDrag.r0, sDrag.r1);
                    }
                    else if (sDrag.onRightEdge)
                    {
                        drawGrabRight(sDrag.r0, sDrag.r1);
                    }
                }
            }
        }

        splitter.Merge(draw);

        if (addMode != AddMode::None)
        {
            snd::internal::driver::SoundThreadLock lock;

            BankFile::KeyRegion* newRegion = nullptr;
            if (addMode == AddMode::Front)
            {
                if (addNode)
                {
                    newRegion = new BankFile::KeyRegion(0, addNode->getKeyMin() - 1);
                    addNode->insertFront(newRegion);
                }
                else
                {
                    newRegion = new BankFile::KeyRegion(0, 127);
                    instrument->getKeyRegionList().pushBack(newRegion);
                }
            }
            else if (addMode == AddMode::Back)
            {
                SEAD_ASSERT(addNode);
                newRegion = new BankFile::KeyRegion(addNode->getKeyMax() + 1, 127);
                addNode->insertBack(newRegion);
            }
            else if (addMode == AddMode::After)
            {
                SEAD_ASSERT(addNode);
                BankFile::KeyRegion* next = addNode->getNext(*instrument);
                u8 newMin = addNode->getKeyMax() + 1;
                u8 newMax = next ? next->getKeyMin() - 1 : 127;

                newRegion = new BankFile::KeyRegion(newMin, newMax);
                addNode->insertBack(newRegion);
            }

            newRegion->setId(0);
            newRegion->setEnableName(true);
            newRegion->getName() = "KeyRegion";

            sBfsar.updateList(instrument->getKeyRegionList());

            BankFile::VelocityRegion* newVelRegion = new BankFile::VelocityRegion(0, 127);
            newVelRegion->setId(0);
            newVelRegion->setEnableName(true);
            newVelRegion->getName() = "VelocityRegion";

            newRegion->getVelocityRegionList().pushBack(newVelRegion);

            SelectVelocity(newRegion, newVelRegion);
        }

        if (sDrag.mode == DragMode::None && sSubSelectedItem && sSubSelectedItem->getItemType() == Item::ItemType::BankFileVelocityRegion && ImGui::IsWindowFocused())
        {
            BankFile::VelocityRegion* velRegion = static_cast<BankFile::VelocityRegion*>(sSubSelectedItem);
            BankFile::KeyRegion* keyRegion = sContextKeyRegion;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            {
                BankFile::KeyRegion* prev = keyRegion->getPrev(*instrument);
                if (prev && !prev->getVelocityRegionList().isEmpty())
                {
                    SelectVelocity(prev, static_cast<BankFile::VelocityRegion*>(prev->getVelocityRegionList().back()->val()));
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            {
                BankFile::KeyRegion* next = keyRegion->getNext(*instrument);
                if (next && !next->getVelocityRegionList().isEmpty())
                {
                    SelectVelocity(next, static_cast<BankFile::VelocityRegion*>(next->getVelocityRegionList().back()->val()));
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                BankFile::VelocityRegion* prev = velRegion->getPrev(*keyRegion);
                if (prev)
                {
                    SelectVelocity(keyRegion, prev);
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                BankFile::VelocityRegion* next = velRegion->getNext(*keyRegion);
                if (next)
                {
                    SelectVelocity(keyRegion, next);
                }
            }
        }

        static s32 mouseKey = 0;
        if (sDrag.mode != DragMode::None && mouseDown && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            mouseKey = XToKey(mouse.x + edgeOffset + (sDrag.mode == DragMode::ResizeL ? 1.0f : -1.0f));
            // draw->AddLine(
            //     ImVec2(mouse.x + edgeOffset, canvasPos.y),
            //     ImVec2(mouse.x + edgeOffset, canvasPos.y + regionHeight),
            //     IM_COL32(255, 0, 0, 255),
            //     1.0f
            // );

            BankFile::KeyRegion* region = sDrag.region;

            if (sDrag.mode == DragMode::ResizeL)
            {
                if (mouseKey > region->getKeyMax())
                {
                    mouseKey = region->getKeyMax();
                }

                if (region->getPrev(*instrument))
                {
                    BankFile::KeyRegion* prev = region->getPrev(*instrument);
                    if (mouseKey < prev->getKeyMin() + 1)
                    {
                        mouseKey = prev->getKeyMin() + 1;
                    }
                }

                if (sDrag.prev == nullptr && region->getPrev(*instrument))
                {
                    mouseKey = std::clamp(mouseKey, region->getPrev(*instrument)->getKeyMax() + 1, (s32)region->getKeyMax());
                }
            }
            else if (sDrag.mode == DragMode::ResizeR)
            {
                if (mouseKey < region->getKeyMin())
                {
                    mouseKey = region->getKeyMin();
                }

                if (region->getNext(*instrument))
                {
                    BankFile::KeyRegion* next = region->getNext(*instrument);
                    if (mouseKey > next->getKeyMax() - 1)
                    {
                        mouseKey = next->getKeyMax() - 1;
                    }
                }

                if (sDrag.next == nullptr && region->getNext(*instrument))
                {
                    mouseKey = std::clamp(mouseKey, (s32)region->getKeyMin(), region->getNext(*instrument)->getKeyMin() - 1);
                }
            }

            if (sDrag.mode == DragMode::ResizeL)
            {
                snd::internal::driver::SoundThreadLock lock;

                s32 newMin = mouseKey;

                if (sDrag.prev)
                {
                    sDrag.prev->setKeyMax(newMin - 1, *instrument);
                }

                region->setKeyMin(newMin, *instrument);

                if (sDrag.prev)
                {
                    sDrag.prev->setKeyMax(region->getKeyMin() - 1, *instrument);
                }
            }
            else if (sDrag.mode == DragMode::ResizeR)
            {
                snd::internal::driver::SoundThreadLock lock;

                s32 newMax = mouseKey;

                if (sDrag.next)
                {
                    sDrag.next->setKeyMin(newMax + 1, *instrument);
                }

                region->setKeyMax(newMax, *instrument);

                if (sDrag.next)
                {
                    sDrag.next->setKeyMin(region->getKeyMax() + 1, *instrument);
                }
            }

            // f32 xEdge, dummy;
            // if (sDrag.mode == DragMode::ResizeL)
            //     GetKeyRect(mouseKey, xEdge, dummy);
            // else
            //     GetKeyRect(mouseKey + 1, xEdge, dummy);

            // if (sDrag.mode != DragMode::ResizeL && mouseKey >= 127)
            // {
            //     xEdge = canvasSize.x; //? Special case for the last key to fill to the end
            // }
            
            // f32 lx = canvasPos.x + xEdge;
            // draw->AddLine(ImVec2(lx, canvasPos.y), ImVec2(lx, canvasPos.y + regionHeight), IM_COL32(255, 255, 0, 255), 2.0f);
            // ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        if (sDrag.mode != DragMode::None && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            sDrag = {};
        }

        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete) && sSubSelectedItem && sSubSelectedItem->getItemType() == Item::ItemType::BankFileVelocityRegion)
        {
            BankFile::VelocityRegion* velRegion = static_cast<BankFile::VelocityRegion*>(sSubSelectedItem);
            BankFile::KeyRegion* keyRegion = sContextKeyRegion;
            DeleteVeloctity(keyRegion, velRegion);
        }

        if (ImGui::IsPopupOpen("VelocityRegionContextMenu") && sSubSelectedItem && sSubSelectedItem->getItemType() == Item::ItemType::BankFileVelocityRegion)
        {
            BankFile::VelocityRegion* velRegion = static_cast<BankFile::VelocityRegion*>(sSubSelectedItem);
            BankFile::KeyRegion* keyRegion = sContextKeyRegion;
            VelocityContextMenu(instrument, keyRegion, velRegion);
        }
    }
    else
    {
        const char* text = "Select an Instrument";
        ImVec2 ts = ImGui::CalcTextSize(text);

        f32 x = canvasPos.x + canvasSize.x / 2.0f - ts.x / 2.0f;
        f32 y = canvasPos.y + regionHeight / 2.0f - ts.y / 2.0f;

        draw->AddText(ImVec2(x, y), IM_COL32_WHITE, text);
    }

    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + regionHeight));

    static s32 sPrevNote = -1;

    s32 originalKey = -1;
    if (sSubSelectedItem && sSubSelectedItem->getItemType() == Item::ItemType::BankFileVelocityRegion)
    {
        BankFile::VelocityRegion* velRegion = static_cast<BankFile::VelocityRegion*>(sSubSelectedItem);
        originalKey = velRegion->getOriginalKey();
    }

    ImGui_PianoKeyboard(
        "Keyboard",
        ImVec2(width, keyboardHeight),
        &sPrevNote,
        beginNote,
        endNote,
        &KeyboardFunc,
        instrument,
        nullptr,
        originalKey
    );

    s32 key[2] = { -1, -1 };
    s32 vel[2] = { -1, -1 };
    if (sSubSelectedItem && sSubSelectedItem->getItemType() == Item::ItemType::BankFileVelocityRegion)
    {
        BankFile::VelocityRegion* velRegion = static_cast<BankFile::VelocityRegion*>(sSubSelectedItem);
        BankFile::KeyRegion* keyRegion = sContextKeyRegion;
        key[0] = keyRegion->getKeyMin();
        key[1] = keyRegion->getKeyMax();
        vel[0] = velRegion->getVelocityMin();
        vel[1] = velRegion->getVelocityMax();
    }

    ImGui::Text("Key         ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);

    ImGui::BeginDisabled();
    ImGui::InputInt2("###Key", key);
    ImGui::EndDisabled();

    ImGui::Text("Velocity    ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);

    ImGui::BeginDisabled();
    ImGui::InputInt2("###Velocity", vel);
    ImGui::EndDisabled();

    sead::FixedSafeString<16> origKey;
    if (originalKey >= 0 && originalKey < MmlCommandNote::sKeysNum)
    {
        origKey = MmlCommandNote::sKeys[originalKey];
    }

    ImGui::Text("Original Key");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);

    ImGui::BeginDisabled();
    ImGui::InputText("###Orig", origKey.getBuffer(), origKey.getBufferSize());
    ImGui::EndDisabled();
}

InstanciateItemCallback CreateInstrumentFunc(bool clear)
{
    auto doCreate = []() -> Item*
    {
        BankFile::Instrument* instr = new BankFile::Instrument();
        instr->setEnableName(true);
        instr->getName() = "Instrument";

        BankFile::KeyRegion* keyRegion = new BankFile::KeyRegion(0, 127);
        keyRegion->setId(0);
        keyRegion->setEnableName(true);
        keyRegion->getName() = "KeyRegion";

        instr->getKeyRegionList().pushBack(keyRegion);

        BankFile::VelocityRegion* velRegion = new BankFile::VelocityRegion(0, 127);
        velRegion->setId(0);
        velRegion->setEnableName(true);
        velRegion->getName() = "VelocityRegion";

        keyRegion->getVelocityRegionList().pushBack(velRegion);

        return instr;
    };

    return doCreate;
}

void BankFile::drawFileUI()
{
    static f32 sKeyboardHeight = 200.0f; // initial guess
    f32 totalHeight = ImGui::GetContentRegionAvail().y;

    f32 topHeight = totalHeight - sKeyboardHeight;
    if (topHeight < 0.0f)
    {
        topHeight = 0.0f;
    }

    if (ImGui::BeginChild("Instruments", ImVec2(0.0f, topHeight), ImGuiChildFlags_Border))
    {
        DrawAllItemsUI("Instrument", mInstrumentList, &CreateInstrumentFunc, nullptr, nullptr, nullptr, true);
    }
    ImGui::EndChild();

    f32 startY = ImGui::GetCursorScreenPos().y;
    if (ImGui::BeginChild("Keyboard", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border))
    {
        f32 width = ImGui::GetContentRegionAvail().x;

        DrawKeyboardWithRegions(
            width,
            70.0f, // keyboard height
            128.0f,  // region height
            0,
            127,
            (sSelectedItem && sSelectedItem->getItemType() == Item::ItemType::BankFileInstrument)
                ? static_cast<Instrument*>(sSelectedItem)
                : nullptr
        );

        f32 endY = ImGui::GetCursorScreenPos().y;
        sKeyboardHeight = endY - startY + ImGui::GetStyle().WindowPadding.y;
    }
    ImGui::EndChild();
}

void BankFile::doRead(const void* fileAddr)
{
    nw::snd::internal::BankFileReader reader(fileAddr);
    SEAD_ASSERT(reader.IsInitialized());

    using InstrumentItemPair = std::pair<s16, const nw::snd::internal::BankFile::Instrument*>;
    std::vector<InstrumentItemPair> instruments;

    for (s32 programNo = 0; programNo < reader.GetInstrumentCount(); programNo++)
    {
        const nw::snd::internal::BankFile::Instrument* instrumentInfo = reader.GetInstrument(programNo);
        if (!instrumentInfo)
            continue;

        instruments.emplace_back(s16(programNo), instrumentInfo);
    }

    std::sort(instruments.begin(), instruments.end(), [](const InstrumentItemPair& a, const InstrumentItemPair& b) -> bool
        {
            return a.second < b.second;
        }
    );

    for (const InstrumentItemPair& pair : instruments)
    {
        const s16& programNo = pair.first;
        const nw::snd::internal::BankFile::Instrument* const& instrumentInfo = pair.second;

        Instrument* instrument = new Instrument();
        instrument->mId = mInstrumentList.size();

        instrument->mEnableName = true;
        instrument->mName = "Instrument";

        instrument->mProgramNo = programNo;
        instrument->read(instrumentInfo, *reader.GetWaveIdTable());

        mInstrumentList.pushBack(instrument);
    }
}

u32 BankFile::doWrite(sead::FileHandle* handle, sead::WriteStream* stream, bool isLast) const
{
    SEAD_ASSERT(mBank);
    SEAD_ASSERT(mWaveArchive);

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

    for (const Item* instrumentItem : getInstrumentList())
    {
        SEAD_ASSERT(instrumentItem->getItemType() == Item::ItemType::BankFileInstrument);
        const BankFile::Instrument* instrument = static_cast<const BankFile::Instrument*>(instrumentItem);

        for (const Item* keyRegionItem : instrument->getKeyRegionList())
        {
            SEAD_ASSERT(keyRegionItem->getItemType() == Item::ItemType::BankFileKeyRegion);
            const BankFile::KeyRegion* keyRegion = static_cast<const BankFile::KeyRegion*>(keyRegionItem);

            for (const Item* velocityRegionItem : keyRegion->getVelocityRegionList())
            {
                SEAD_ASSERT(velocityRegionItem->getItemType() == Item::ItemType::BankFileVelocityRegion);
                const BankFile::VelocityRegion* velocityRegion = static_cast<const BankFile::VelocityRegion*>(velocityRegionItem);

                const Item* waveFileItem = velocityRegion->getWaveFileRef().getItem();
                SEAD_ASSERT(waveFileItem);
                SEAD_ASSERT(waveFileItem->getItemType() == Item::ItemType::WaveFile);

                const WaveFile* waveFile = static_cast<const WaveFile*>(waveFileItem);

                if (!waveIdIndexes.contains(waveFile))
                {
                    waveIdIndexes[waveFile] = waveIds.size();

                    SEAD_ASSERT(mWaveArchiveWaveFilesIndexes);

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
        }
    }

    struct KeyRegionInfo
    {
        KeyRegionInfo(u8 keyMin_, u8 keyMax_)
            : isNull(true)
            , keyMin(keyMin_)
            , keyMax(keyMax_)
        {
        }

        KeyRegionInfo(const KeyRegion* keyRegion)
            : isNull(false)
            , keyMin(keyRegion->getKeyMin())
            , keyMax(keyRegion->getKeyMax())
        {
            std::vector<const VelocityRegion*> velocityRegions;
            for (const Item* velocityRegionItem : keyRegion->getVelocityRegionList())
            {
                velocityRegions.push_back(static_cast<const VelocityRegion*>(velocityRegionItem));
            }

            std::sort(velocityRegions.begin(), velocityRegions.end(), [](const VelocityRegion* velocityRegionA, const VelocityRegion* velocityRegionB) -> bool
                {
                    return velocityRegionA->getVelocityMin() < velocityRegionB->getVelocityMin();
                }
            );

            u8 velocityMin = 0;
            for (const VelocityRegion* velocityRegion : velocityRegions)
            {
                SEAD_ASSERT(velocityRegion->getVelocityMin() <= velocityMin && velocityMin <= velocityRegion->getVelocityMax());

                sortedVelocityRegions.emplace_back(velocityRegion);
                velocityMin = velocityRegion->getVelocityMax() + 1;
            }

            if (sortedVelocityRegions.size() == 1 && sortedVelocityRegions[0]->getVelocityMin() == 0 && sortedVelocityRegions[0]->getVelocityMax() == 127)
            {
                tableType = nw::snd::internal::ElementType_BankFile_DirectReferenceTable;
                return;
            }

            s32 num = (sortedVelocityRegions[0]->getVelocityMin() > 0) ? (sortedVelocityRegions.size() + 1) : sortedVelocityRegions.size();
            if (num <= 11)
            {
                SEAD_ASSERT(sortedVelocityRegions[0]->getVelocityMin() == 0);

                s32 rangeMax = sortedVelocityRegions[sortedVelocityRegions.size() - 1]->getVelocityMax();
                SEAD_ASSERT(rangeMax >= 127);

                tableType = nw::snd::internal::ElementType_BankFile_RangeReferenceTable;
                return;
            }

            const auto copy = sortedVelocityRegions;
            sortedVelocityRegions.clear();
            SEAD_ASSERT(copy.size() > 0);

            s32 rangeMin = copy[0]->getVelocityMin();
            s32 rangeMax = copy[copy.size() - 1]->getVelocityMax();
            SEAD_ASSERT_MSG(rangeMax - rangeMin + 1 >= 11, "IndexReferenceTable cannot apply.");

            num = 0;
            for (s32 i = rangeMin; i <= rangeMax; i++)
            {
                while (copy[num]->getVelocityMax() < i)
                {
                    num++;
                    SEAD_ASSERT_MSG(num < copy.size(), "failed to create index reference table.");
                }

                SEAD_ASSERT_MSG(i >= copy[num]->getVelocityMin(), "null object not found.");

                sortedVelocityRegions.emplace_back(copy[num]);
            }

            tableType = nw::snd::internal::ElementType_BankFile_IndexReferenceTable;
        }

        bool isNull;
        u8 keyMin;
        u8 keyMax;
        std::vector<const VelocityRegion*> sortedVelocityRegions;
        u16 tableType;
    };

    struct InstrumentInfo
    {
        InstrumentInfo(const Instrument* instrument)
            : programNo(-1)
            , isNull(instrument == nullptr)
            , tableType(0)
        {
            if (!instrument)
            {
                return;
            }

            programNo = instrument->getProgramNo();

            std::vector<const KeyRegion*> keyRegions;
            for (const Item* keyRegionItem : instrument->getKeyRegionList())
            {
                keyRegions.push_back(static_cast<const KeyRegion*>(keyRegionItem));
            }

            std::sort(keyRegions.begin(), keyRegions.end(), [](const KeyRegion* keyRegionA, const KeyRegion* keyRegionB) -> bool
                {
                    return keyRegionA->getKeyMin() < keyRegionB->getKeyMin();
                }
            );

            u8 keyMin = 0;
            for (const KeyRegion* keyRegion : keyRegions)
            {
                SEAD_ASSERT(keyMin <= keyRegion->getKeyMax());
                if (keyMin < keyRegion->getKeyMin())
                {
                    sortedKeyRegions.emplace_back(keyMin, keyRegion->getKeyMin() - 1);
                }

                sortedKeyRegions.emplace_back(keyRegion);
                keyMin = keyRegion->getKeyMax() + 1;
            }

            if (sortedKeyRegions.size() == 1 && sortedKeyRegions[0].keyMin == 0 && sortedKeyRegions[0].keyMax == 127)
            {
                tableType = nw::snd::internal::ElementType_BankFile_DirectReferenceTable;
                return;
            }

            s32 num = (sortedKeyRegions[0].keyMin > 0) ? (sortedKeyRegions.size() + 1) : sortedKeyRegions.size();
            if (num <= 11)
            {
                if (sortedKeyRegions[0].keyMin > 0)
                {
                    sortedKeyRegions.insert(sortedKeyRegions.begin(), KeyRegionInfo(0, sortedKeyRegions[0].keyMin - 1));
                }

                s32 rangeMax = sortedKeyRegions[sortedKeyRegions.size() - 1].keyMax;
                if (rangeMax < 127)
                {
                    sortedKeyRegions.emplace_back(rangeMax + 1, 127);
                }

                tableType = nw::snd::internal::ElementType_BankFile_RangeReferenceTable;
                return;
            }

            const auto copy = sortedKeyRegions;
            sortedKeyRegions.clear();
            SEAD_ASSERT(copy.size() > 0);

            s32 rangeMin = copy[0].keyMin;
            s32 rangeMax = copy[copy.size() - 1].keyMax;
            SEAD_ASSERT_MSG(rangeMax - rangeMin + 1 >= 11, "IndexReferenceTable cannot apply.");

            num = 0;
            for (s32 i = rangeMin; i <= rangeMax; i++)
            {
                while (copy[num].keyMax < i)
                {
                    num++;
                    SEAD_ASSERT_MSG(num < copy.size(), "failed to create index reference table.");
                }

                SEAD_ASSERT_MSG(i >= copy[num].keyMin, "null object not found.");

                sortedKeyRegions.emplace_back(copy[num]);
            }

            tableType = nw::snd::internal::ElementType_BankFile_IndexReferenceTable;
        }

        s16 programNo;
        bool isNull;
        std::vector<KeyRegionInfo> sortedKeyRegions;
        u16 tableType;
    };

    std::vector<InstrumentInfo> instruments;

    {
        std::unordered_map<u32, const Instrument*> instrumentMap;

        for (const Item* instrumentItem : getInstrumentList())
        {
            SEAD_ASSERT(instrumentItem->getItemType() == Item::ItemType::BankFileInstrument);
            const BankFile::Instrument* instrument = static_cast<const BankFile::Instrument*>(instrumentItem);

            if (instrumentMap.contains(instrument->getProgramNo()))
            {
                SEAD_ASSERT_MSG(false, "'%s': program no %u already exists", getFormattedName().cstr(), instrument->getProgramNo());
                continue;
            }

            instrumentMap[instrument->getProgramNo()] = instrument;
        }

        u32 validInstrumentCount = 0;
        if (instrumentMap.size() > 0)
        {
            for (u32 i = 0; i < 32767; i++)
            {
                if (!instrumentMap.contains(i))
                {
                    instruments.push_back(InstrumentInfo(nullptr));
                    continue;
                }

                instruments.push_back(InstrumentInfo(instrumentMap[i]));
                validInstrumentCount++;

                if (validInstrumentCount >= instrumentMap.size())
                {
                    break;
                }
            }
        }
    }

    FileWriter writer(handle, stream);
    writer.openFile("FBNK", 1, mVersion);

    auto writeVelocityRegion = [&](const VelocityRegion& velocityRegion)
    {
        const Item* waveFileItem = velocityRegion.getWaveFileRef().getItem();
        SEAD_ASSERT(waveFileItem);
        SEAD_ASSERT(waveFileItem->getItemType() == Item::ItemType::WaveFile);

        const WaveFile* waveFile = static_cast<const WaveFile*>(waveFileItem);

        stream->writeU32(waveIdIndexes[waveFile]);

        stream->writeU32(nw::snd::internal::VelocityRegionBitFlag::VELOCITY_REGION_BASIC_PARAM_FLAG);
        stream->writeU32(velocityRegion.getOriginalKey());
        stream->writeU32(velocityRegion.getVolume());
        stream->writeU32(velocityRegion.getPan());
        stream->writeF32(velocityRegion.getPitch());
        stream->writeU32(velocityRegion.getIsIgnoreNoteOff() | (velocityRegion.getKeyGroup() << 8) | (velocityRegion.getInterpolationType() << 16));
        stream->writeU32(0x20); // Envelope

        stream->writeU32(0x0); // AdshrCurveRef type
        stream->writeU32(0x8); // AdshrCurveRef offset

        const snd::AdshrCurve& adshrCurve = velocityRegion.getAdshrCurve();
        stream->writeU8(adshrCurve.attack);
        stream->writeU8(adshrCurve.decay);
        stream->writeU8(adshrCurve.sustain);
        stream->writeU8(adshrCurve.hold);
        stream->writeU8(adshrCurve.release);

        writer.align(0x4);
    };

    auto writeKeyRegion = [&](const KeyRegionInfo& keyRegion)
    {
        writer.openReference("VelocityRegionChunk");

        if (keyRegion.tableType == 0)
        {
            writer.closeNullReference("VelocityRegionChunk");
            return;
        }

        writer.closeReference("VelocityRegionChunk", keyRegion.tableType);

        writer.pushOffsetBase();
        {
            switch (keyRegion.tableType)
            {
                case nw::snd::internal::ElementType_BankFile_DirectReferenceTable:
                {
                    writer.openReference("VelocityRegion");

                    writer.closeReference("VelocityRegion", nw::snd::internal::ElementType_BankFile_VelocityRegionInfo);

                    const std::vector<const VelocityRegion*>& velocityRegions = keyRegion.sortedVelocityRegions;
                    SEAD_ASSERT(velocityRegions.size() == 1);
                    writer.pushOffsetBase();
                    {
                        writeVelocityRegion(*velocityRegions[0]);
                    }
                    writer.popOffsetBase();
                    break;
                }

                case nw::snd::internal::ElementType_BankFile_RangeReferenceTable:
                {
                    const std::vector<const VelocityRegion*>& velocityRegions = keyRegion.sortedVelocityRegions;
                    SEAD_ASSERT(velocityRegions.size() > 0);

                    stream->writeU32(velocityRegions.size()); // BorderTable size

                    for (const VelocityRegion* velocityRegion : velocityRegions)
                    {
                        stream->writeU8(velocityRegion->getVelocityMax());
                    }

                    writer.align(0x4);

                    for (u32 i = 0; i < velocityRegions.size(); i++)
                    {
                        writer.openReference(sead::FormatFixedSafeString<32>("VelocityRegion%u", i));
                    }

                    for (u32 i = 0; i < velocityRegions.size(); i++)
                    {
                        const VelocityRegion& velocityRegion = *velocityRegions[i];

                        writer.closeReference(sead::FormatFixedSafeString<32>("VelocityRegion%u", i), nw::snd::internal::ElementType_BankFile_VelocityRegionInfo);

                        writer.pushOffsetBase();
                        {
                            writeVelocityRegion(velocityRegion);
                        }
                        writer.popOffsetBase();
                    }
                    break;
                }

                case nw::snd::internal::ElementType_BankFile_IndexReferenceTable:
                    break;
            }
        }
        writer.popOffsetBase();
    };

    //? Info Block
    {
        writer.openBlock(nw::snd::internal::ElementType_BankFile_InfoBlock, "INFO");

        writer.openReference("WaveIdTable");
        writer.openReference("InstrumentTableRef");

        writer.closeReference("InstrumentTableRef", nw::snd::internal::ElementType_Table_ReferenceTable);
        writer.pushOffsetBase();
        {
            stream->writeU32(instruments.size());

            for (const InstrumentInfo& instrument : instruments)
            {
                sead::FormatFixedSafeString<32> refName("Instrument%i", instrument.programNo);
                writer.openReference(refName);

                if (instrument.isNull)
                {
                    writer.closeReference(refName, nw::snd::internal::ElementType_BankFile_NullInfo, -1);
                }
            }

            for (const Item* instrItem : getInstrumentList())
            {
                const Instrument* instr = static_cast<const Instrument*>(instrItem);
                const InstrumentInfo& instrument = instruments[instr->getProgramNo()];

                writer.closeReference(sead::FormatFixedSafeString<32>("Instrument%i", instr->getProgramNo()), nw::snd::internal::ElementType_BankFile_InstrumentInfo);

                writer.pushOffsetBase();
                {
                    writer.openReference("KeyRegionChunk");

                    if (instrument.tableType == 0)
                    {
                        writer.closeNullReference("KeyRegionChunk");
                    }
                    else
                    {
                        writer.closeReference("KeyRegionChunk", instrument.tableType);

                        writer.pushOffsetBase();
                        {
                            switch (instrument.tableType)
                            {
                                case nw::snd::internal::ElementType_BankFile_DirectReferenceTable:
                                {
                                    writer.openReference("KeyRegion");

                                    writer.closeReference("KeyRegion", nw::snd::internal::ElementType_BankFile_KeyRegionInfo);

                                    const std::vector<KeyRegionInfo>& keyRegions = instrument.sortedKeyRegions;
                                    SEAD_ASSERT(keyRegions.size() == 1);
                                    writer.pushOffsetBase();
                                    {
                                        writeKeyRegion(keyRegions[0]);
                                    }
                                    writer.popOffsetBase();
                                    break;
                                }

                                case nw::snd::internal::ElementType_BankFile_RangeReferenceTable:
                                {
                                    const std::vector<KeyRegionInfo>& keyRegions = instrument.sortedKeyRegions;
                                    SEAD_ASSERT(keyRegions.size() > 0);

                                    stream->writeU32(keyRegions.size()); // BorderTable size

                                    for (const KeyRegionInfo& keyRegion : keyRegions)
                                    {
                                        stream->writeU8(keyRegion.keyMax);
                                    }

                                    writer.align(0x4);

                                    for (u32 i = 0; i < keyRegions.size(); i++)
                                    {
                                        writer.openReference(sead::FormatFixedSafeString<32>("KeyRegion%u", i));
                                    }

                                    for (u32 i = 0; i < keyRegions.size(); i++)
                                    {
                                        const KeyRegionInfo& keyRegion = keyRegions[i];

                                        sead::FormatFixedSafeString<32> refName("KeyRegion%u", i);
                                        if (keyRegion.isNull)
                                        {
                                            writer.closeReference(refName, nw::snd::internal::ElementType_BankFile_NullInfo, nw::snd::internal::Util::Reference::INVALID_OFFSET);
                                            continue;
                                        }

                                        writer.closeReference(refName, nw::snd::internal::ElementType_BankFile_KeyRegionInfo);

                                        writer.pushOffsetBase();
                                        {
                                            writeKeyRegion(keyRegion);
                                        }
                                        writer.popOffsetBase();
                                    }
                                    break;
                                }

                                case nw::snd::internal::ElementType_BankFile_IndexReferenceTable:
                                {
                                    const std::vector<KeyRegionInfo>& keyRegions = instrument.sortedKeyRegions;
                                    SEAD_ASSERT(keyRegions.size() > 0);

                                    u32 startPos = writer.getPosition();

                                    stream->writeU8(keyRegions[0].keyMin);
                                    stream->writeU8(keyRegions[keyRegions.size() - 1].keyMax);
                                    stream->writeU16(0); // Padding

                                    for (u32 i = 0; i < keyRegions.size(); i++)
                                    {
                                        writer.openReference(sead::FormatFixedSafeString<32>("KeyRegion%u", i));
                                    }

                                    s32 prevKeyRegionPos = 0;
                                    for (u32 i = 0; i < keyRegions.size(); i++)
                                    {
                                        const KeyRegionInfo& keyRegion = keyRegions[i];

                                        sead::FormatFixedSafeString<32> refName("KeyRegion%u", i);
                                        if (keyRegion.isNull)
                                        {
                                            writer.closeReference(refName, nw::snd::internal::ElementType_BankFile_NullInfo, nw::snd::internal::Util::Reference::INVALID_OFFSET);
                                            continue;
                                        }

                                        if (i != 0)
                                        {
                                            if (keyRegion.keyMin == keyRegions[i - 1].keyMin)
                                            {
                                                writer.closeReference(refName, nw::snd::internal::ElementType_BankFile_KeyRegionInfo, prevKeyRegionPos - startPos);
                                                continue;
                                            }
                                        }

                                        writer.closeReference(refName, nw::snd::internal::ElementType_BankFile_KeyRegionInfo);

                                        prevKeyRegionPos = writer.getPosition();

                                        writer.pushOffsetBase();
                                        {
                                            writeKeyRegion(keyRegion);
                                        }
                                        writer.popOffsetBase();
                                    }

                                    break;
                                }
                            }
                        }
                        writer.popOffsetBase();
                    }
                }
                writer.popOffsetBase();
            }
        }
        writer.popOffsetBase();

        writer.closeReference("WaveIdTable", nw::snd::internal::ElementType_Table_EmbeddingTable);

        stream->writeU32(waveIds.size());

        for (const WaveId& waveId : waveIds)
        {
            stream->writeU32(waveId.warcId);
            stream->writeU32(waveId.waveIdx);
        }

        writer.closeBlock();
    }

    u32 fileSize = writer.getPosition();

    writer.closeFile();

    mBank = nullptr;
    mWaveArchive = nullptr;
    mWaveArchiveWaveFilesIndexes = nullptr;

    return fileSize;
}
