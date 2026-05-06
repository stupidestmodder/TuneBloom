#include <ui/UI.h>

const Item* SoundSet::validate(sead::BufferedSafeString& error) const
{
    if (!Item::validateName(error))
    {
        return this;
    }

    switch (getSoundSetType())
    {
        case SoundSetType::Wave:
            switch (getWaveArchiveType())
            {
                case WaveArchiveType::AutomaticShared:
                case WaveArchiveType::AutomaticIndividual:
                    break;

                case WaveArchiveType::Explicit:
                    if (getWaveArchiveRef().isAttached())
                    {
                        break;
                    }

                //! Fallthrough

                default:
                    error = "Invalid Wave Archive";
                    return this;
            }

            break;

        case SoundSetType::Seq:
            break;

        default:
            error = "Invalid Sound Type";
            return this;
    }

    if (getIsEmpty())
    {
        return nullptr;
    }

    if (getEndId() < getStartId())
    {
        error = "Invalid Start and End ids";
        return this;
    }

    if (getStartId() >= sBfsar.getSoundList().size())
    {
        error = "Start Id exceeds sound count";
        return this;
    }

    if (getEndId() >= sBfsar.getSoundList().size())
    {
        error = "End Id exceeds sound count";
        return this;
    }

    for (const Item::ListNode* itemNode = sBfsar.getItem(getStartId(), sBfsar.getSoundList()); itemNode && itemNode->val()->getId() <= getEndId(); itemNode = sBfsar.getSoundList().next(itemNode))
    {
        SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
        const Sound* sound = static_cast<const Sound*>(itemNode->val());

        if (sound->mOwnerSet)
        {
            error.format("Sound '%s' is already in Sound Set '%s'", sound->getFormattedName().cstr(), sound->mOwnerSet->getFormattedName().cstr());
            return this;
        }

        sound->mOwnerSet = this;
    }

    return nullptr;
}

void DrawSoundSetPropertiesUI()
{
    SoundSet* soundSet = static_cast<SoundSet*>(sSelectedItem);

    const ImU32 cStepU32 = 1;

    {
        static const char* sSoundSetTypes[] = { "Wave", "Sequence" };

        SoundSet::SoundSetType soundSetType = soundSet->getSoundSetType();
        if (ImGui::Combo("Sound Type", (s32*)&soundSetType, sSoundSetTypes, IM_ARRAYSIZE(sSoundSetTypes)))
        {
            soundSet->setSoundSetType(soundSetType);
        }
    }

    {
        bool enableWaveArchives = soundSet->getSoundSetType() == SoundSet::SoundSetType::Wave;
        if (!enableWaveArchives)
        {
            ImGui::BeginDisabled();
        }

        Item* warc = soundSet->getWaveArchiveRef().getItem();
        WaveArchiveType warcType = soundSet->getWaveArchiveType();
        if (WaveArchiveSelector("Wave Archive", &warcType, &warc, sBfsar.getWaveArchiveList()))
        {
            soundSet->getWaveArchiveRef().attach(warc);
            soundSet->setWaveArchiveType(warcType);
        }

        if (!enableWaveArchives)
        {
            ImGui::EndDisabled();
        }
    }

    {
        bool isEmpty = soundSet->getIsEmpty();
        if (ImGui::Checkbox("Is Empty", &isEmpty))
        {
            soundSet->setIsEmpty(isEmpty);
        }

        if (isEmpty)
        {
            ImGui::BeginDisabled();
        }

        u32 startId = soundSet->getStartId();
        if (ImGui::InputScalar("Start Id", ImGuiDataType_U32, &startId, &cStepU32))
        {
            if (startId <= soundSet->getEndId())
                soundSet->setStartId(startId);
        }

        u32 endId = soundSet->getEndId();
        if (ImGui::InputScalar("End Id", ImGuiDataType_U32, &endId, &cStepU32))
        {
            if (endId >= soundSet->getStartId())
                soundSet->setEndId(endId);

            //if (endId >= sBfsar.getSoundList().size())
            //    soundSet->setEndId(sBfsar.getSoundList().size() - 1);
        }

        if (isEmpty)
        {
            ImGui::EndDisabled();
        }
    }

    if (ImGui::BeginChild("ChildSounds", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border))
    {
        for (Item::ListNode* itemNode = sBfsar.getItem(soundSet->getStartId(), sBfsar.getSoundList()); itemNode && itemNode->val()->getId() <= soundSet->getEndId(); itemNode = sBfsar.getSoundList().next(itemNode))
        {
            SEAD_ASSERT(itemNode->val()->getItemType() == Item::ItemType::Sound);
            Sound* sound = static_cast<Sound*>(itemNode->val());

            if (ImGui::Button(sead::FormatFixedSafeString<32>(ICON_LC_PLAY "###%u", sound->getId()).cstr()))
            {
                sSoundPlayer.playSound(sound);
                sSelectedItem = soundSet;
            }

            ImGui::SameLine();

            bool isError =
                (sound->getSoundType() == Sound::SoundType::Seq  && soundSet->getSoundSetType() != SoundSet::SoundSetType::Seq) ||
                (sound->getSoundType() == Sound::SoundType::Wave && soundSet->getSoundSetType() != SoundSet::SoundSetType::Wave) ||
                (sound->getSoundType() == Sound::SoundType::Strm);

            sead::FixedSafeString<516> name(sound->getFormattedName().cstr());
            if (isError)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                name.format("%s (?)", sound->getFormattedName().cstr());
            }

            if (ImGui::Selectable(name.cstr()))
            {
            }

            if (isError)
            {
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                {
                    ImGui::SetTooltip(
                        "The type of this Sound does not match the Sound Set type.\n"
                        "The tool allows this case, except for Wave Sounds.\n"
                        "However, please note that if your game tries to explicitly\n"
                        "load this Sound Set, then all following Sounds will NOT load,\n"
                        "including this one !"
                    );
                }
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem(ICON_LC_EXTERNAL_LINK " Go To"))
                {
                    SelectItem(sound);
                }

                ImGui::End();
            }
        }
    }
    ImGui::EndChild();
}
