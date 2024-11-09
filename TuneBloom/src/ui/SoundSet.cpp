#include <ui/UI.h>

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
        u32 startId = soundSet->getStartId();
        if (ImGui::InputScalar("Start Id", ImGuiDataType_U32, &startId, &cStepU32))
        {
            if (startId <= soundSet->getEndId())
                soundSet->setStartId(startId);
        }
    }

    {
        u32 endId = soundSet->getEndId();
        if (ImGui::InputScalar("End Id", ImGuiDataType_U32, &endId, &cStepU32))
        {
            if (endId >= soundSet->getStartId())
                soundSet->setEndId(endId);

            //if (endId >= sBfsar.getSoundList().size())
            //    soundSet->setEndId(sBfsar.getSoundList().size() - 1);
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
                PlaySound(sound);
                sSelectedItem = soundSet;
            }

            ImGui::SameLine();

            if (ImGui::Selectable(sound->getFormattedName().cstr()))
            {
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
