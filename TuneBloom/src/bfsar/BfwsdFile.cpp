#include <bfsar/File.h>

#include <snd/snd_WaveSoundFileReader.h>

void BfwsdFile::drawUI()
{
    InnerFile::drawUI();
}

void BfwsdFile::doRead(const void* fileAddr)
{
    nw::snd::internal::WaveSoundFileReader reader(fileAddr);
}
