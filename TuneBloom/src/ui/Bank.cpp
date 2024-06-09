#include <ui/UI.h>

// Banks

InstanciateItemCallback CreateBankFunc(bool clear)
{
    return CreateItemFunc(clear, []() -> Item* { return new Bank(); }, nullptr);
}

void DrawBanksUI()
{
    DrawAllItemsUI("Bank", sBfsar.getBankList(), &CreateBankFunc);
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
        Item* file = bank->getFile().getItem();
        if (ItemSelector("File", sBfsar.getBankFileList(), &file))
        {
            bank->getFile().attach(file);
        }
    }
}
