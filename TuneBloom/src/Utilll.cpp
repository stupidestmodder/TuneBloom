#include <Utilll.h>

static sead::Framework* sFramework = nullptr;

namespace util {

sead::Framework* getFramework()
{
    return sFramework;
}

void setFramework_(sead::Framework* framework)
{
    sFramework = framework;
}

} // namespace util
