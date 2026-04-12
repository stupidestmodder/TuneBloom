#include <ui/UI.h>

#include <bfsar/Bank.h>

// Groups

const Item* Group::validate(sead::BufferedSafeString& error) const
{
    if (!Item::validateName(error))
    {
        return this;
    }

    switch (getOutputType())
    {
        case OutputType::Embed:
        case OutputType::Link:
            break;

        case OutputType::External: // TODO
            error = "External Group saving is not supported yet";
            return this;

        default:
            error = "Invalid Output Type";
            return this;
    }

    sead::FixedSafeString<256> itemError;

    std::unordered_set<const Item*> itemsTarget;
    for (const Item* item : mItemInfoList)
    {
        const ItemInfo& itemInfo = *static_cast<const ItemInfo*>(item);

        if (!itemInfo.getIsDisabled())
        {
            if (itemInfo.validate(itemError))
            {
                error.format("'%s': %s", itemInfo.getFormattedName().cstr(), itemError.cstr());
                return &itemInfo;
            }

            const Item* target = itemInfo.getItemRef().getItem();
            if (itemsTarget.count(target) > 0)
            {
                error.format("Item '%s' is already in this Group", itemInfo.getFormattedName().cstr());
                return &itemInfo;
            }

            itemsTarget.insert(target);
        }

        itemError.clear();
    }

    return nullptr;
}

InstanciateItemCallback CreateGroupFunc(bool clear)
{
    return CreateItemFunc(clear, []() -> Item* { return new Group(); }, nullptr);
}

void DrawGroupsUI()
{
    DrawAllItemsUI("Group", sBfsar.getGroupList(), &CreateGroupFunc);
}

void DrawGroupPropertiesUI()
{
    Group* group = static_cast<Group*>(sSelectedItem);

    {
        static const char* sOutputTypes[] = {
            "Embed",
            "Link",
            "External"
        };

        u32 outputType = (u32)group->getOutputType();
        if (ImGui::Combo("Output Type", (s32*)&outputType, sOutputTypes, IM_ARRAYSIZE(sOutputTypes)))
        {
            group->setOutputType(static_cast<Group::OutputType>(outputType));
        }
    }
}

const char* Group::ItemInfo::sItemIdTypes[4] = {
    //"None",
    "Sound",
    "Sound Set",
    "Bank",
    "Wave Archive"
};

void Group::ItemInfo::drawUI()
{
    bool disabled = mIsDisabled;
    if (ImGui::Checkbox("Disable", &disabled))
    {
        mIsDisabled = disabled;
    }

    ImGui::Separator();

    if (disabled)
    {
        ImGui::BeginDisabled();
    }

    {
        u32 itemRefType = (u32)mItemRefType - 1;
        if (ImGui::Combo("Item Type", (s32*)&itemRefType, sItemIdTypes, IM_ARRAYSIZE(sItemIdTypes)))
        {
            mItemRefType = static_cast<ItemType>(itemRefType + 1);
            mItemRef.detach();

            mLoadItem = 0; // All
        }
    }

    {
        Item* oldItem = mItemRef.getItem();
        Item* item = oldItem;
        if (ItemSelector("Item", sBfsar.getItemList(mItemRefType), &item, false))
        {
            mItemRef.attach(item);

            if (oldItem && item)
            {
                switch (mItemRefType)
                {
                    case Item::ItemType::Sound:
                    {
                        Sound* oldSound = static_cast<Sound*>(oldItem);
                        Sound* sound = static_cast<Sound*>(item);
                        if (oldSound->getSoundType() != sound->getSoundType())
                        {
                            mLoadItem = 0; // All
                        }
                    }

                    case Item::ItemType::SoundSet:
                    {
                        SoundSet* oldSoundSet = static_cast<SoundSet*>(oldItem);
                        SoundSet* soundSet = static_cast<SoundSet*>(item);
                        if (oldSoundSet->getSoundSetType() != soundSet->getSoundSetType())
                        {
                            mLoadItem = 0; // All
                        }
                    }
                }
            }
        }
    }

    static sead::FixedSafeString<256> sError;

    {
        u32 itemCount = 0;
        const char** items = getLoadItems(&itemCount);

        bool enable = mItemRef.isAttached() && items;

        u32 oldLoadItem = mLoadItem;
        u32 loadItem = oldLoadItem;
        if (!enable)
        {
            ImGui::BeginDisabled();

            static const char* all = "All";

            itemCount = 1;
            items = &all;
            oldLoadItem = 0;
            loadItem = 0;
        }

        if (ImGui::Combo("Load Items", (s32*)&loadItem, items, itemCount))
        {
            mLoadItem = loadItem;

            if (validate(sError))
            {
                ImGui::OpenPopup("###LoadItem");
                mLoadItem = oldLoadItem;
            }
        }

        if (!enable)
        {
            ImGui::EndDisabled();
        }
    }

    WarningPopup("###LoadItem", sError.cstr());

    // {
    //     u32 loadFlag = getLoadFlag();

    //     CenteredTextX("Load Flags");

    //     if (ImGui::CheckboxFlagsT<u32>("Sequence", &loadFlag, LoadFlag::LoadSeq))
    //     {
    //     }

    //     ImGui::SameLine();

    //     if (ImGui::CheckboxFlagsT<u32>("Wave Sound", &loadFlag, LoadFlag::LoadWsd))
    //     {
    //     }

    //     ImGui::SameLine();

    //     if (ImGui::CheckboxFlagsT<u32>("Bank", &loadFlag, LoadFlag::LoadBank))
    //     {
    //     }

    //     ImGui::SameLine();

    //     if (ImGui::CheckboxFlagsT<u32>("Wave Archive", &loadFlag, LoadFlag::LoadWarc))
    //     {
    //     }
    // }

    if (disabled)
    {
        ImGui::EndDisabled();
    }
}

u32 Group::ItemInfo::getLoadFlag() const
{
    if (!mItemRef.isAttached())
    {
        return LoadFlag::LoadAll;
    }

    switch (mItemRefType)
    {
        case Item::ItemType::Sound:
        {
            const Sound* sound = static_cast<const Sound*>(mItemRef.getItem());
            if (sound->getSoundType() == Sound::SoundType::Seq)
            {
                return getLoadFlagForSequence_();
            }

            break;
        }

        case Item::ItemType::SoundSet:
        {
            const SoundSet* soundSet = static_cast<const SoundSet*>(mItemRef.getItem());
            if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Seq)
            {
                return getLoadFlagForSequence_();
            }
            else if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave)
            {
                return getLoadFlagForWaveSoundSet_();
            }

            break;
        }

        case Item::ItemType::Bank:
            return getLoadFlagForBank_();

        case Item::ItemType::WaveArchive:
            return getLoadFlagForWaveArchive_();
    }

    return LoadFlag::LoadAll;
}

u32 Group::ItemInfo::getLoadFlagForSequence_() const
{
    static u32 sFlags[(u32)SequenceItems::Num] = {
        LoadFlag::LoadAll,
        LoadFlag::LoadSeq  | LoadFlag::LoadBank,
        LoadFlag::LoadSeq  | LoadFlag::LoadWarc,
        LoadFlag::LoadBank | LoadFlag::LoadWarc,
        LoadFlag::LoadSeq,
        LoadFlag::LoadBank,
        LoadFlag::LoadWarc,
    };

    SEAD_ASSERT(mLoadItem < (u32)SequenceItems::Num);
    return sFlags[mLoadItem];
}

u32 Group::ItemInfo::getLoadFlagForWaveSoundSet_() const
{
    static u32 sFlags[(u32)WaveSoundSetItems::Num] = {
        LoadFlag::LoadAll,
        LoadFlag::LoadWsd,
        LoadFlag::LoadWarc,
    };

    SEAD_ASSERT(mLoadItem < (u32)WaveSoundSetItems::Num);
    return sFlags[mLoadItem];
}

u32 Group::ItemInfo::getLoadFlagForBank_() const
{
    static u32 sFlags[(u32)BankItems::Num] = {
        LoadFlag::LoadAll,
        LoadFlag::LoadBank,
        LoadFlag::LoadWarc,
    };

    SEAD_ASSERT(mLoadItem < (u32)BankItems::Num);
    return sFlags[mLoadItem];
}

u32 Group::ItemInfo::getLoadFlagForWaveArchive_() const
{
    static u32 sFlags[(u32)WaveArchiveItems::Num] = {
        LoadFlag::LoadAll
    };

    SEAD_ASSERT(mLoadItem < (u32)WaveArchiveItems::Num);
    return sFlags[mLoadItem];
}

const char** Group::ItemInfo::getLoadItems(u32* outCount) const
{
    return GetLoadItems(mItemRef.getItem(), mItemRefType, outCount);
}

const char** Group::ItemInfo::GetLoadItems(const Item* item, ItemType itemType, u32* outCount)
{
    SEAD_ASSERT(outCount);

    static const char* sSequenceSoundLoadItems[] = {
        "All",
        "Sequence and Bank",
        "Sequence and Wave Archive",
        "Bank and Wave Archive",
        "Sequence",
        "Bank",
        "Wave Archive"
    };

    static const char* sWaveSoundSetLoadItems[] = {
        "All",
        "Wave Sound Set",
        "Wave Archive"
    };

    static const char* sSequenceSetLoadItems[] = {
        "All",
        "Sequence Set and Bank",
        "Sequence Set and Wave Archive",
        "Bank and Wave Archive",
        "Sequence Set",
        "Bank",
        "Wave Archive"
    };

    static const char* sBankLoadItems[] = {
        "All",
        "Bank",
        "Wave Archive"
    };

    static const char* sWaveArchiveLoadItems[] = {
        "All",
    };

    if (!item)
    {
        *outCount = 0;
        return nullptr;
    }

    switch (itemType)
    {
        case Item::ItemType::Sound:
        {
            const Sound* sound = static_cast<const Sound*>(item);
            if (sound->getSoundType() == Sound::SoundType::Seq)
            {
                *outCount = IM_ARRAYSIZE(sSequenceSoundLoadItems);
                return sSequenceSoundLoadItems;
            }

            break;
        }

        case Item::ItemType::SoundSet:
        {
            const SoundSet* soundSet = static_cast<const SoundSet*>(item);
            if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Seq)
            {
                *outCount = IM_ARRAYSIZE(sSequenceSetLoadItems);
                return sSequenceSetLoadItems;
            }
            else if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave)
            {
                *outCount = IM_ARRAYSIZE(sWaveSoundSetLoadItems);
                return sWaveSoundSetLoadItems;
            }

            break;
        }

        case Item::ItemType::Bank:
            *outCount = IM_ARRAYSIZE(sBankLoadItems);
            return sBankLoadItems;

        case Item::ItemType::WaveArchive:
            *outCount = IM_ARRAYSIZE(sWaveArchiveLoadItems);
            return sWaveArchiveLoadItems;
    }

    *outCount = 0;
    return nullptr;
}

void Group::ItemInfo::setLoadItems(u32 loadFlags)
{
    switch (mItemRefType)
    {
        case Item::ItemType::Sound:
        {
            const Sound* sound = static_cast<const Sound*>(mItemRef.getItem());
            if (sound->getSoundType() == Sound::SoundType::Seq)
            {
                setLoadItemsForSequence_(loadFlags);
            }

            break;
        }

        case Item::ItemType::SoundSet:
        {
            const SoundSet* soundSet = static_cast<const SoundSet*>(mItemRef.getItem());
            if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Seq)
            {
                setLoadItemsForSequence_(loadFlags);
            }
            else if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave)
            {
                setLoadItemsForWaveSoundSet_(loadFlags);
            }

            break;
        }

        case Item::ItemType::Bank:
            setLoadItemsForBank_(loadFlags);
            break;

        case Item::ItemType::WaveArchive:
            setLoadItemsForWaveArchive_(loadFlags);
            break;
    }
}

void Group::ItemInfo::setLoadItemsForSequence_(u32 loadFlags)
{
    if (loadFlags == LoadFlag::LoadAll)
    {
        mLoadItem = (u32)SequenceItems::All;
    }
    else if (loadFlags == (LoadFlag::LoadSeq | LoadFlag::LoadBank))
    {
        mLoadItem = (u32)SequenceItems::SequenceAndBank;
    }
    else if (loadFlags == (LoadFlag::LoadSeq | LoadFlag::LoadWarc))
    {
        mLoadItem = (u32)SequenceItems::SequenceAndWarc;
    }
    else if (loadFlags == (LoadFlag::LoadBank | LoadFlag::LoadWarc))
    {
        mLoadItem = (u32)SequenceItems::BankAndWarc;
    }
    else if (loadFlags == LoadFlag::LoadSeq)
    {
        mLoadItem = (u32)SequenceItems::Sequence;
    }
    else if (loadFlags == LoadFlag::LoadBank)
    {
        mLoadItem = (u32)SequenceItems::Bank;
    }
    else if (loadFlags == LoadFlag::LoadWarc)
    {
        mLoadItem = (u32)SequenceItems::Warc;
    }
}

void Group::ItemInfo::setLoadItemsForWaveSoundSet_(u32 loadFlags)
{
    if (loadFlags == LoadFlag::LoadAll)
    {
        mLoadItem = (u32)WaveSoundSetItems::All;
    }
    else if (loadFlags == LoadFlag::LoadWsd)
    {
        mLoadItem = (u32)WaveSoundSetItems::Wsd;
    }
    else if (loadFlags == LoadFlag::LoadWarc)
    {
        mLoadItem = (u32)WaveSoundSetItems::Warc;
    }
}

void Group::ItemInfo::setLoadItemsForBank_(u32 loadFlags)
{
    if (loadFlags == LoadFlag::LoadAll)
    {
        mLoadItem = (u32)BankItems::All;
    }
    else if (loadFlags == LoadFlag::LoadBank)
    {
        mLoadItem = (u32)BankItems::Bank;
    }
    else if (loadFlags == LoadFlag::LoadWarc)
    {
        mLoadItem = (u32)BankItems::Warc;
    }
}

void Group::ItemInfo::setLoadItemsForWaveArchive_(u32 loadFlags)
{
    if (loadFlags == LoadFlag::LoadAll)
    {
        mLoadItem = (u32)WaveArchiveItems::All;
    }
}

const Item* Group::ItemInfo::validate(sead::BufferedSafeString& error) const
{
    if (mItemRefType != ItemType::Sound && mItemRefType != ItemType::SoundSet && mItemRefType != ItemType::Bank && mItemRefType != ItemType::WaveArchive)
    {
        error = "Invalid Item Type";
        return this;
    }

    if (!mItemRef.isAttached())
    {
        error = "Item not attached";
        return this;
    }

    switch (mItemRefType)
    {
        case ItemType::Sound:
        {
            const Sound* sound = static_cast<const Sound*>(mItemRef.getItem());
            if (sound->getSoundType() == Sound::SoundType::Seq)
            {
                if (validateSequence_(*sound, error))
                {
                    return nullptr;
                }
                else
                {
                    return this;
                }
            }
            else
            {
                error = "Sound must be a Sequence";
                return this;
            }

            break;
        }

        case ItemType::SoundSet:
        {
            const SoundSet* soundSet = static_cast<const SoundSet*>(mItemRef.getItem());
            if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Seq)
            {
                for (const Item::ListNode* itemNode = sBfsar.getItem(soundSet->getStartId(), sBfsar.getSoundList()); itemNode && itemNode->val()->getId() <= soundSet->getEndId(); itemNode = sBfsar.getSoundList().next(itemNode))
                {
                    SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
                    const Sound* sound = static_cast<const Sound*>(itemNode->val());

                    if (sound->getSoundType() == Sound::SoundType::Seq)
                    {
                        if (!validateSequence_(*sound, error))
                        {
                            error = "Must select 'All', 'Bank and Wave Archive' or 'Sequence Set'\nif the Wave Archive of the Bank referenced by any Sequence Sound in the set is either\n'Automatic (Shared)' or 'Automatic (Individual)'";
                            return this;
                        }
                    }
                }
            }
            else if (soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave)
            {
                if (validateWaveSoundSet_(*soundSet, error, getLoadFlag()))
                {
                    return nullptr;
                }
                else
                {
                    return this;
                }
            }

            break;
        }

        case ItemType::Bank:
        {
            const Bank* bank = static_cast<const Bank*>(mItemRef.getItem());
            if (validateBank_(*bank, error, getLoadFlag()))
            {
                return nullptr;
            }
            else
            {
                return this;
            }

            break;
        }

        case ItemType::WaveArchive:
        {
            if (getLoadFlag() != LoadFlag::LoadAll)
            {
                error = "Must select 'All' for Wave Archive";
                return this;
            }

            break;
        }
    }

    return nullptr;
}

bool Group::ItemInfo::validateSequence_(const Sound& sound, sead::BufferedSafeString& error) const
{
    SEAD_ASSERT(sound.getSoundType() == Sound::SoundType::Seq);

    if (mLoadItem == (u32)SequenceItems::Sequence)
    {
        return true;
    }

    for (u32 i = 0; i < 4; i++)
    {
        const Bank* bank = static_cast<const Bank*>(sound.getSequenceSoundInfo().getBankRef(i).getItem());
        if (!bank)
        {
            continue;
        }

        if (!validateBank_(*bank, error, getLoadFlag()))
        {
            error = "Must select 'All', 'Bank and Wave Archive' or 'Sequence'\nif the Wave Archive of the Bank referenced by the Sequence Sound is either\n'Automatic (Shared)' or 'Automatic (Individual)'";
            return false;
        }
    }

    return true;
}

bool Group::ItemInfo::validateWaveSoundSet_(const SoundSet& soundSet, sead::BufferedSafeString& error, u32 loadFlags) const
{
    if (soundSet.getWaveArchiveType() == WaveArchiveType::AutomaticShared || soundSet.getWaveArchiveType() == WaveArchiveType::AutomaticIndividual)
    {
        if ((loadFlags & (LoadFlag::LoadBank | LoadFlag::LoadWarc)) != (LoadFlag::LoadBank | LoadFlag::LoadWarc))
        {
            error = "Must select 'All' if the Wave Archive is either\n'Automatic (Shared)' or 'Automatic (Individual)'";
            return false;
        }
    }

    return true;
}

bool Group::ItemInfo::validateBank_(const Bank& bank, sead::BufferedSafeString& error, u32 loadFlags) const
{
    if (bank.getWaveArchiveType() == WaveArchiveType::AutomaticShared || bank.getWaveArchiveType() == WaveArchiveType::AutomaticIndividual)
    {
        if ((loadFlags & (LoadFlag::LoadBank | LoadFlag::LoadWarc)) != (LoadFlag::LoadBank | LoadFlag::LoadWarc))
        {
            error = "Must select 'All' if the Wave Archive is either\n'Automatic (Shared)' or 'Automatic (Individual)'";
            return false;
        }
    }

    return true;
}
