#pragma once

#include <framework/win/seadGameFrameworkWinGL.h>

class AppFramework : public sead::GameFrameworkWinGL
{
    SEAD_RTTI_OVERRIDE(AppFramework, sead::GameFrameworkWinGL);

public:
    explicit AppFramework(const CreateArg& arg);

    static void initialize(const InitializeArg& arg);
};
