#include "AppFramework.h"

AppFramework::AppFramework(const CreateArg& arg)
    : sead::GameFrameworkWinGL(arg)
{
}

void AppFramework::initialize(const InitializeArg& arg)
{
    sead::GameFramework::initialize(arg);
}
