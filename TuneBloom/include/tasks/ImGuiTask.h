#pragma once

#include <framework/seadProcessMeterBar.h>
#include <framework/seadTask.h>
#include <framework/seadTaskMgr.h>

class ImGuiTask : public sead::Task
{
    SEAD_RTTI_OVERRIDE(ImGuiTask, sead::Task);
    SEAD_TASK_SINGLETON_DISPOSER(ImGuiTask);

public:
    explicit ImGuiTask(const sead::TaskConstructArg& arg);

    void prepare() override;
    void enter() override;
    void exit() override;
    void calc() override;
    void draw() override;

protected:
    sead::MultiProcessMeterBar<1> mCalcMeter;
    sead::MultiProcessMeterBar<1> mDrawMeter;
};

