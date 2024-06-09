#pragma once

#include <framework/seadFramework.h>

namespace util {

sead::Framework* getFramework();
void setFramework_(sead::Framework* framework);

inline const sead::SafeString cAppName("TuneBloom");

} // namespace util
