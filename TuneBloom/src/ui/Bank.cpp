#include <ui/UI.h>

// Banks

const Item* Bank::validate(sead::BufferedSafeString& error) const
{
    if (!Item::validateName(error))
    {
        return this;
    }

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

    if (!getFileRef().isAttached())
    {
        error = "Invalid Bank File";
        return this;
    }

    return nullptr;
}

InstanciateItemCallback CreateBankFunc(bool clear)
{
    return CreateItemFunc(clear, []() -> Item* { return new Bank(); }, nullptr);
}

const char* BankNamePrefixFunc(Item* item)
{
    Bank* bank = static_cast<Bank*>(item);

    BankFile* bankFile = static_cast<BankFile*>(bank->getFileRef().getItem());
    if (!bankFile)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button(sead::FormatFixedSafeString<32>(ICON_LC_FILE_PEN "###%u", bank->getId()).cstr()))
    {
        OpenFileWindow(bankFile);
    }

    if (!bankFile)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    return nullptr;
}

void DrawBanksUI()
{
    DrawAllItemsUI("Bank", sBfsar.getBankList(), &CreateBankFunc, &BankNamePrefixFunc);
}

void DrawBankPropertiesUI()
{
    Bank* bank = static_cast<Bank*>(sSelectedItem);

    {
        Item* warc = bank->getWaveArchiveRef().getItem();
        WaveArchiveType warcType = bank->getWaveArchiveType();
        if (WaveArchiveSelector("Wave Archive", &warcType, &warc, sBfsar.getWaveArchiveList()))
        {
            bank->getWaveArchiveRef().attach(warc);
            bank->setWaveArchiveType(warcType);
        }
    }

    {
        Item* file = bank->getFileRef().getItem();
        if (ItemSelector("Bank File", sBfsar.getBankFileList(), &file))
        {
            bank->getFileRef().attach(file);
        }
    }
}
