#include <Utilll.h>

#include <framework/win/seadGameFrameworkBaseWin.h>
#include <gfx/seadTexture.h>

static sead::Framework* sFramework = nullptr;
static sead::Texture* sIcon = nullptr;

namespace util {

sead::Framework* getFramework()
{
    return sFramework;
}

void setFramework_(sead::Framework* framework)
{
    sFramework = framework;
}

sead::Texture* getIcon()
{
    return sIcon;
}

void setIcon_(sead::Texture* icon)
{
    sIcon = icon;
}

bool updateTitle(const char* path)
{
    sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(getFramework());
    if (!fw)
    {
        return false;
    }

    sead::FixedSafeString<512> title;
    title.format("%s %s", cAppName.cstr(), cAppVersion.cstr());

    if (path)
    {
        title.appendWithFormat(" - %s", path);
    }

    fw->setCaption(title);
    return true;
}

} // namespace util
