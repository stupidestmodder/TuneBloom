#include <ui/UI.h>

bool sSelectedItemIsSubWindow = false;

static Item* sDeleteItem = nullptr;

InstanciateItemCallback CreateItemFunc(bool clear, InstanciateItemCallback instanciateItemCallback, ItemPropertiesCallback itemPropertiesCallback)
{
    SEAD_ASSERT(instanciateItemCallback);

    static bool sEnableName = true;
    static sead::FixedSafeString<256> sName;

    static InstanciateItemCallback sInstanciateItemCallback = nullptr;
    sInstanciateItemCallback = instanciateItemCallback;

    static ItemPropertiesCallback sItemPropertiesCallback = nullptr;
    sItemPropertiesCallback = itemPropertiesCallback;

    if (clear)
    {
        sEnableName = true;
        sName.clear();
    }

    ImGui::Checkbox("Enable Name", &sEnableName);

    if (!sEnableName)
    {
        ImGui::BeginDisabled();
    }

    ImGui::InputText("Name", sName.getBuffer(), sName.getBufferSize());

    if (!sEnableName)
    {
        ImGui::EndDisabled();
    }

    DupeNamePopup();

    if (sItemPropertiesCallback)
    {
        sItemPropertiesCallback(clear, nullptr, nullptr);
    }

    auto doCreate = []() -> Item*
    {
        SEAD_ASSERT(sInstanciateItemCallback);

        if (sEnableName && !sName.isEmpty() && !sBfsar.validateName(sName))
        {
            ImGui::OpenPopup("###Dupe");
            return nullptr;
        }

        if (sItemPropertiesCallback)
        {
            bool validate = true;
            sItemPropertiesCallback(false, nullptr, &validate);

            if (!validate)
            {
                return nullptr;
            }
        }

        Item* item = sInstanciateItemCallback();

        item->setEnableName(sEnableName);
        if (sEnableName)
        {
            item->getName() = sName;
        }

        if (sItemPropertiesCallback)
        {
            sItemPropertiesCallback(false, item, nullptr);
        }

        return item;
    };

    return doCreate;
}

void WarningPopup(const char* name, const char* content)
{
    SEAD_ASSERT(name);

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(sead::FormatFixedSafeString<32>(ICON_LC_ALERT_TRIANGLE " Warning%s", name).cstr(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text(content);
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().WindowPadding.x, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

static bool ItemContextMenu(Item* item, CreateItemCallback createCallback, ContextMenuCallback menuCallback, Item*& selectedItem)
{
    bool add = false;

    if (item ? ImGui::BeginPopupContextItem() : ImGui::BeginPopupContextWindow())
    {
        if (item)
        {
            selectedItem = item;

            if (&selectedItem == &sSubSelectedItem)
            {
                sSelectedItemIsSubWindow = true;
            }
            else
            {
                sSelectedItemIsSubWindow = false;
                sSubSelectedItem = nullptr;
            }
        }

        {
            bool disableAdd = createCallback == nullptr;
            if (disableAdd)
            {
                ImGui::BeginDisabled();
            }

            if (ImGui::MenuItem("Add"))
            {
                add = true;
            }

            if (disableAdd)
            {
                ImGui::EndDisabled();
            }
        }

        ImGui::Separator();

        {
            bool disableDelete = item == nullptr;
            if (disableDelete)
            {
                ImGui::BeginDisabled();
            }

            if (ImGui::MenuItem("Delete"))
            {
                sDeleteItem = item;
            }

            if (disableDelete)
            {
                ImGui::EndDisabled();
            }
        }

        if (menuCallback)
        {
            menuCallback(item);
        }

        ImGui::End();
    }

    return add;
}

void DrawAllItemsUI(const char* listName, Item::List& list, CreateItemCallback createCallback, ItemNamePrefixCallback nameCallback, ContextMenuCallback menuCallback, ItemFilterCallback filterCallback)
{
    const bool cUseChild = true;

    bool isSubWindow = false;

    if (cUseChild)
    {
        ImGuiChildFlags flags = 0;

        ImGuiWindow* currentWindow = ImGui::GetCurrentWindow();
        if (currentWindow == ImGui::FindWindowByName("###InfoWindow"))
        {
            flags = ImGuiChildFlags_AlwaysUseWindowPadding;
        }
        else if (currentWindow == ImGui::FindWindowByName("###SubInfoWindow"))
        {
            flags = ImGuiChildFlags_AlwaysUseWindowPadding;
            isSubWindow = true;
        }

        ImGui::BeginChild(sead::FormatFixedSafeString<64>("%sInnerWindow", listName).cstr(), ImVec2(0.0f, 0.0f), flags);
    }

    const bool canEdit = filterCallback == nullptr;

    bool reorder = false;
    Item* item1 = nullptr;
    Item* item2 = nullptr;

    bool add = false;

    static Item* sScrollItem = nullptr;

    Item*& selectedItem = isSubWindow ? sSubSelectedItem : sSelectedItem;

    //if (false)
    {
        if (ImGui::IsWindowFocused() && selectedItem && &list == selectedItem->list())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                Item::ListNode* prev = list.prev(selectedItem);
                if (prev)
                {
                    selectedItem = prev->val();
                    sScrollItem = selectedItem;
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                Item::ListNode* next = list.next(selectedItem);
                if (next)
                {
                    selectedItem = next->val();
                    sScrollItem = selectedItem;
                }
            }

            if (canEdit && ImGui::IsKeyPressed(ImGuiKey_Delete))
            {
                sDeleteItem = selectedItem;
            }
        }
    }

    if (canEdit)
    {
        if (ItemContextMenu(nullptr, createCallback, menuCallback, selectedItem))
        {
            add = true;
        }
    }

    if (list.size() == 0)
    {
        sead::FormatFixedSafeString<64> str("No %ss", listName);
        CenteredText(str.cstr());
    }

    bool hasItem = false;
    for (auto it = list.robustBegin(); it != list.robustEnd(); ++it)
    {
        Item* item = static_cast<Item*>((*it).val());
        if (!item || (filterCallback && !filterCallback(item)))
            continue;

        hasItem = true;

        const char* namePrefix = "";
        if (nameCallback)
        {
            namePrefix = nameCallback(item);

            if (!namePrefix)
                namePrefix = "";
        }

        sead::FixedSafeString<256> name = item->getFormattedName();

        bool popColor = false;
        if (!isSubWindow && sSelectedItemIsSubWindow)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(140.0f / 255.0f, 140.0f / 255.0f, 140.0f / 255.0f, 1.0f));
            popColor = true;
        }

        bool selected = selectedItem == item;
        if (ImGui::Selectable(sead::FormatFixedSafeString<256>("%s%s", namePrefix, name.cstr()).cstr(), selected))
        {
            selectedItem = item;

            if (isSubWindow)
            {
                sSelectedItemIsSubWindow = true;
            }
            else
            {
                sSelectedItemIsSubWindow = false;
                sSubSelectedItem = nullptr;
            }
        }

        if (popColor)
        {
            ImGui::PopStyleColor();
        }

        if (sScrollItem == item)
        {
            sScrollItem = nullptr;
            ImGui::SetScrollHereY();
        }

        if (canEdit)
        {
            if (ItemContextMenu(item, createCallback, menuCallback, selectedItem))
            {
                add = true;
            }

            if (selected)
            {
                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload("ITEM", &item, sizeof(item));

                    ImGui::Text("Moving '%s'", name.cstr());

                    ImGui::EndDragDropSource();
                }
            }

            if (ImGui::BeginDragDropTarget())
            {
                const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ITEM");
                if (payload)
                {
                    Item* data = *((Item**)payload->Data);
                    SEAD_ASSERT(data);
                    SEAD_ASSERT(data != item);

                    reorder = true;
                    item1 = data;
                    item2 = item;
                }

                ImGui::EndDragDropTarget();
            }
        }
    }

    if (list.size() > 0 && !hasItem)
    {
        sead::FormatFixedSafeString<64> str("No %ss", listName);
        CenteredText(str.cstr());
    }

    if (canEdit)
    {
        if (add)
        {
            ImGui::OpenPopup("###Add");
        }

        if (sDeleteItem)
        {
            if (sDeleteItem->getReferences().size() > 0)
            {
                ImGui::OpenPopup("###References");
            }
            else
            {
                delete sDeleteItem;
                selectedItem = nullptr;
                sBfsar.updateList(list);

                // if (isSubWindow)
                // {
                //     sSelectedItemIsSubWindow = false;
                // }

                sDeleteItem = nullptr;
            }
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(sead::FormatFixedSafeString<32>(ICON_LC_PLUS " Add %s###Add", listName).cstr(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            SEAD_ASSERT(createCallback);
            InstanciateItemCallback instanciateItemCallback = createCallback(ImGui::IsWindowAppearing());

            ImGui::Separator();

            ImVec2 buttonSize((ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().WindowPadding.x * 2.0f) / 2.0f, 0.0f);

            if (ImGui::Button("Add", buttonSize))
            {
                SEAD_ASSERT(instanciateItemCallback);
                Item* addedItem = instanciateItemCallback();

                if (addedItem)
                {
                    sScrollItem = addedItem;
                    selectedItem = addedItem;

                    if (!isSubWindow)
                    {
                        sSelectedItemIsSubWindow = false;
                        sSubSelectedItem = nullptr;
                    }
                    else
                    {
                        sSelectedItemIsSubWindow = true;
                    }

                    list.pushBack(addedItem);

                    sBfsar.updateList(list);

                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", buttonSize))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(ICON_LC_ALERT_TRIANGLE " Warning###References", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("The item '%s' is referenced by other items\nDelete anyway ?", sDeleteItem->getFormattedName().cstr());
            ImGui::Separator();

            ImVec2 buttonSize((ImGui::GetWindowContentRegionMax().x - ImGui::GetStyle().WindowPadding.x * 2.0f) / 2.0f, 0.0f);

            if (ImGui::Button("Delete", buttonSize))
            {
                delete sDeleteItem;
                selectedItem = nullptr;
                sBfsar.updateList(list);

                sDeleteItem = nullptr;

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", buttonSize))
            {
                sDeleteItem = nullptr;

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (reorder)
        {
            item1->erase();

            if (item1->getId() < item2->getId())
                item2->insertBack(item1);
            else
                item2->insertFront(item1);

            sBfsar.updateList(list);
        }
    }

    if (cUseChild)
    {
        ImGui::EndChild();
    }
}

void DrawItemPropertiesUI()
{
    Item* item = sSelectedItem;

    //ImGui::Text("Id: %u", item->getId());

    // {
    //     for (const ItemReference* ref : item->getReferences())
    //     {
    //         const Item* owner = ref->getOwner();
    //         if (owner->getItemType() != Item::ItemType::Sound)
    //             continue;

    //         const Sound* sound = static_cast<const Sound*>(owner);
    //         if (sound->getSoundType() != Sound::SoundType::Seq)
    //             continue;

    //         for (const Item* soundSetItem : sBfsar.getSoundSetList())
    //         {
    //             const SoundSet* soundSet = static_cast<const SoundSet*>(soundSetItem);
    //           //if (soundSet->getSoundSetType() != SoundSet::SoundSetType::Seq)
    //           //    continue;

    //             if (!(soundSet->getStartId() <= sound->getId() && sound->getId() <= soundSet->getEndId()))
    //                 continue;

    //             for (const ItemReference* seqRef : soundSet->getReferences())
    //             {
    //                 const Item* seqOwner = seqRef->getOwner();
    //                 if (seqOwner->getItemType() == Item::ItemType::Group)
    //                 {
    //                     const Group* group = static_cast<const Group*>(seqOwner);
    //                     if (group->getId() == 5)
    //                     {
    //                         ImGui::Text("%s", sound->getFormattedName().cstr());
    //                         break;
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }

    bool enableName = item->isEnableName();
    if (ImGui::Checkbox("Enable Name", &enableName))
    {
        item->setEnableName(enableName);
    }

    if (!enableName)
        ImGui::BeginDisabled();

    sead::FixedSafeString<256> name(item->getName());
    if (ImGui::InputText("Name", name.getBuffer(), name.getBufferSize(), ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit())
    {
        if (name != item->getName())
        {
            if (!name.isEmpty())
            {
                if (sBfsar.validateName(name))
                {
                    item->getName() = name;
                }
                else
                {
                    ImGui::OpenPopup("###Dupe");
                }
            }
            else
            {
                item->getName() = name;
            }
        }
    }

    if (!enableName)
        ImGui::EndDisabled();

    DupeNamePopup();
}

bool ItemSelector(const char* name, const Item::List& list, Item** itemPtr, bool allowNone)
{
    SEAD_ASSERT(name);
    SEAD_ASSERT(itemPtr);

    sead::FixedSafeString<256> itemName;
    {
        Item* item = *itemPtr;

        if (item)
        {
            itemName = item->getFormattedName();
        }
        else
        {
            itemName.copy("[-] (none)");
        }
    }

    bool ret = false;

    ImGuiStyle& style = ImGui::GetStyle();
    f32 w = ImGui::CalcItemWidth();
    f32 spacing = style.ItemInnerSpacing.x;
    f32 buttonSize = ImGui::GetFrameHeight();

    ImGui::PushItemWidth(w - spacing * 2.0f - buttonSize * 2.0f);
    if (ImGui::BeginCombo(sead::FormatFixedSafeString<64>("##%sCombo", name).cstr(), itemName.cstr()))
    {
        if (allowNone)
        {
            bool selected = *itemPtr == nullptr;
            if (ImGui::Selectable("[-] (none)", selected))
            {
                *itemPtr = nullptr;
                ret = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        for (auto it = list.robustBegin(); it != list.robustEnd(); ++it)
        {
            Item* item = (*it).val();

            itemName = item->getFormattedName();

            bool selected = *itemPtr == item;
            if (ImGui::Selectable(itemName.cstr(), selected))
            {
                *itemPtr = item;
                ret = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGuiButtonFlags buttonFlags = ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;

    ImGui::SameLine(0.0f, spacing);
    if (ImGui::ArrowButtonEx(sead::FormatFixedSafeString<64>("##%sComboLeft", name).cstr(), ImGuiDir_Left, ImVec2(buttonSize, buttonSize), buttonFlags))
    {
        Item* item = *itemPtr;

        if (item)
        {
            Item::ListNode* node = list.prev(item);

            if (node)
            {
                *itemPtr = node->val();
                ret = true;
            }
            else if (allowNone && item == list.front())
            {
                *itemPtr = nullptr;
                ret = true;
            }
        }
        else if (!allowNone)
        {
            Item::ListNode* node = list.front();

            if (node)
            {
                *itemPtr = node->val();
                ret = true;
            }
        }
    }

    ImGui::SameLine(0.0f, spacing);
    if (ImGui::ArrowButtonEx(sead::FormatFixedSafeString<64>("##%sComboRight", name).cstr(), ImGuiDir_Right, ImVec2(buttonSize, buttonSize), buttonFlags))
    {
        Item* item = *itemPtr;

        if (item)
        {
            Item::ListNode* node = list.next(item);

            if (node)
            {
                *itemPtr = node->val();
                ret = true;
            }
            else if (allowNone && item != list.back())
            {
                *itemPtr = nullptr;
                ret = true;
            }
        }
        else
        {
            Item::ListNode* node = list.front();

            if (node)
            {
                *itemPtr = node->val();
                ret = true;
            }
        }
    }

    ImGui::SameLine(0, style.ItemInnerSpacing.x);

    bool redText = *itemPtr == nullptr && !allowNone;
    if (redText)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,         ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
    }

    ImGui::Text(name);

    if (redText)
    {
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    }

    return ret;
}

bool WaveArchiveSelector(const char* name, WaveArchiveType* warcType, Item** warcPtr, const Item::List& warcList)
{
    SEAD_ASSERT(name);
    SEAD_ASSERT(warcType);
    SEAD_ASSERT(warcPtr);

    if (*warcType == WaveArchiveType::Explicit && *warcPtr == nullptr)
    {
        SEAD_ASSERT(false);
    }

    sead::FixedSafeString<256> itemName;
    {
        Item* item = *warcPtr;

        if (item)
        {
            itemName = item->getFormattedName();
        }
        else if (*warcType == WaveArchiveType::AutomaticShared)
        {
            itemName.copy("Automatic (Shared)");
        }
        else if (*warcType == WaveArchiveType::AutomaticIndividual)
        {
            itemName.copy("Automatic (Individual)");
        }
        else
        {
            itemName = "Invalid";
        }
    }

    bool ret = false;

    ImGuiStyle& style = ImGui::GetStyle();
    f32 w = ImGui::CalcItemWidth();
    f32 spacing = style.ItemInnerSpacing.x;
    f32 buttonSize = ImGui::GetFrameHeight();

    ImGui::PushItemWidth(w - spacing * 2.0f - buttonSize * 2.0f);
    if (ImGui::BeginCombo(sead::FormatFixedSafeString<64>("##%sCombo", name).cstr(), itemName.cstr()))
    {
        {
            bool selected = *warcType == WaveArchiveType::AutomaticShared;
            if (ImGui::Selectable("Automatic (Shared)", selected))
            {
                *warcType = WaveArchiveType::AutomaticShared;
                *warcPtr = nullptr;
                ret = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        {
            bool selected = *warcType == WaveArchiveType::AutomaticIndividual;
            if (ImGui::Selectable("Automatic (Individual)", selected))
            {
                *warcType = WaveArchiveType::AutomaticIndividual;
                *warcPtr = nullptr;
                ret = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        for (auto it = warcList.robustBegin(); it != warcList.robustEnd(); ++it)
        {
            Item* item = (*it).val();

            itemName = item->getFormattedName();

            bool selected = *warcPtr == item;
            if (ImGui::Selectable(itemName.cstr(), selected))
            {
                *warcType = WaveArchiveType::Explicit;
                *warcPtr = item;
                ret = true;
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGuiButtonFlags buttonFlags = ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;

    ImGui::SameLine(0.0f, spacing);
    if (ImGui::ArrowButtonEx(sead::FormatFixedSafeString<64>("##%sComboLeft", name).cstr(), ImGuiDir_Left, ImVec2(buttonSize, buttonSize), buttonFlags))
    {
        if (*warcType == WaveArchiveType::AutomaticShared)
        {
            // do nothing
        }
        else if (*warcType == WaveArchiveType::AutomaticIndividual)
        {
            *warcType = WaveArchiveType::AutomaticShared;
            *warcPtr = nullptr;
            ret = true;
        }
        else
        {
            Item* item = *warcPtr;

            if (item)
            {
                Item::ListNode* node = warcList.prev(item);

                if (node)
                {
                    *warcType = WaveArchiveType::Explicit;
                    *warcPtr = node->val();
                    ret = true;
                }
                else if (item == warcList.front())
                {
                    *warcType = WaveArchiveType::AutomaticIndividual;
                    *warcPtr = nullptr;
                    ret = true;
                }
            }
            else
            {
                Item::ListNode* node = warcList.front();

                if (node)
                {
                    *warcType = WaveArchiveType::Explicit;
                    *warcPtr = node->val();
                    ret = true;
                }
            }
        }
    }

    ImGui::SameLine(0.0f, spacing);
    if (ImGui::ArrowButtonEx(sead::FormatFixedSafeString<64>("##%sComboRight", name).cstr(), ImGuiDir_Right, ImVec2(buttonSize, buttonSize), buttonFlags))
    {
        if (*warcType == WaveArchiveType::AutomaticShared)
        {
            *warcType = WaveArchiveType::AutomaticIndividual;
            *warcPtr = nullptr;
            ret = true;
        }
        else if (*warcType == WaveArchiveType::AutomaticIndividual)
        {
            Item::ListNode* node = warcList.front();

            if (node)
            {
                *warcType = WaveArchiveType::Explicit;
                *warcPtr = node->val();
                ret = true;
            }
        }
        else
        {
            Item* item = *warcPtr;

            if (item)
            {
                Item::ListNode* node = warcList.next(item);

                if (node)
                {
                    *warcType = WaveArchiveType::Explicit;
                    *warcPtr = node->val();
                    ret = true;
                }
                else if (item != warcList.back())
                {
                    *warcType = WaveArchiveType::AutomaticShared;
                    *warcPtr = nullptr;
                    ret = true;
                }
            }
            else
            {
                Item::ListNode* node = warcList.front();

                if (node)
                {
                    *warcType = WaveArchiveType::Explicit;
                    *warcPtr = node->val();
                    ret = true;
                }
            }
        }
    }

    ImGui::SameLine(0, style.ItemInnerSpacing.x);
    ImGui::Text(name);

    return ret;
}

void ItemIdTable(const char* name, IdTable& table, const Item::List& itemList)
{
    f32 spacing = ImGui::GetStyle().ItemInnerSpacing.x;

    ImGui::Text("%ss", name);
    ImGui::SameLine(0.0f, spacing);

    ImGui::Text("(%i)", table.size());
    ImGui::SameLine(0.0f, spacing);

    f32 buttonSize = ImGui::GetFrameHeight();

    if (ImGui::Button(sead::FormatFixedSafeString<64>("-##%s", name).cstr(), ImVec2(buttonSize, buttonSize)))
    {
        sead::TListNode<IdEntry*>* entry = table.popBack();
        if (entry)
        {
            delete entry->val();
        }
    }

    ImGui::SameLine(0.0f, spacing);

    if (ImGui::Button(sead::FormatFixedSafeString<64>("+##%s", name).cstr(), ImVec2(buttonSize, buttonSize)))
    {
        table.pushBack(new IdEntry(table.getOwner(), nullptr));
    }

    if (ImGui::BeginChild(name, ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border))
    {
        u32 i = 0;
        for (IdEntry* entry : table)
        {
            SEAD_ASSERT(entry);

            Item* item = entry->getIdRef().getItem();
            if (ItemSelector(sead::FormatFixedSafeString<32>("%s %u", name, i).cstr(), itemList, &item, true))
            {
                entry->getIdRef().attach(item);
            }

            if (i + 1 != table.size())
            {
                ImGui::Separator();
            }

            i++;
        }
    }
    ImGui::EndChild();
}
