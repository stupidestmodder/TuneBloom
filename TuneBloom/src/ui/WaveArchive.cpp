#include <ui/UI.h>

// Wave Archives

const Item* WaveArchive::validate(sead::BufferedSafeString& error) const
{
    if (!Item::validateName(error))
    {
        return this;
    }

    return nullptr;
}

InstanciateItemCallback CreateWaveArchiveFunc(bool clear)
{
    return CreateItemFunc(clear, []() -> Item* { return new WaveArchive(); }, nullptr);
}

void DrawWaveArchivesUI()
{
    DrawAllItemsUI("Wave Archive", sBfsar.getWaveArchiveList(),
        &CreateWaveArchiveFunc, nullptr, nullptr, GetItemFilterCallback()
    );
}

void DrawWaveArchivePropertiesUI()
{
    WaveArchive* warc = static_cast<WaveArchive*>(sSelectedItem);

    {
        bool isLoadIndividual = warc->getIsLoadIndividual();
        if (ImGui::Checkbox("Load Individually", &isLoadIndividual))
        {
            warc->setIsLoadIndividual(isLoadIndividual);
        }
    }
}
