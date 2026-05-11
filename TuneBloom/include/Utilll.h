#pragma once

#include <framework/seadFramework.h>
#include <gfx/seadTexture.h>

namespace util {

sead::Framework* getFramework();
void setFramework_(sead::Framework* framework);

sead::Texture* getIcon();
void setIcon_(sead::Texture* icon);

inline const sead::SafeString cAppName("TuneBloom");

#if defined(COMMIT_SHA)
inline const sead::SafeString cAppVersion(COMMIT_SHA);
#else
inline const sead::SafeString cAppVersion("1.01");
#endif

bool updateTitle(const char* path);

} // namespace util
