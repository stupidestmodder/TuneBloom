#include "tasks/RootTask.h"

#include <filedevice/seadFileDeviceMgr.h>
#include <framework/win/seadGameFrameworkBaseWin.h>
#include <framework/seadProcessMeter.h>
#include <gfx/seadPrimitiveRenderer.h>

#include "tasks/BfsarTask.h"
#include "tasks/ImGuiTask.h"

RootTask::RootTask(const sead::TaskConstructArg& arg)
    : sead::Task(arg, "RootTask")
    , mNativeFileDevice(nullptr)
{
}

void RootTask::prepare()
{
    {
        // sead::Framework::CreateSystemTaskArg arg;
        // arg.infloop_detection_span = sead::TickSpan::makeFromSeconds(5);

        // getFramework()->createSystemTasks(this, arg);
        sead::GameFramework* fw = sead::DynamicCast<sead::GameFramework>(getFramework());
        fw->createProcessMeter(this);
    }

    mNativeFileDevice = new sead::WinNativeFileDevice();
    sead::FileDeviceMgr::instance()->mount(mNativeFileDevice);

    sead::PrimitiveRenderer::createInstance(nullptr);
    sead::PrimitiveRenderer::instance()->prepare(nullptr, sead::SafeString::cEmptyString);

    adjustHeapAll();

    {
        sead::TaskBase::CreateArg arg(&sead::TTaskFactory<ImGuiTask>);
        arg.tag = sead::TaskBase::Tag::eSystem;

        getTaskMgr()->createSingletonTaskSync<ImGuiTask>(arg);
    }

    {
        sead::TaskBase::CreateArg arg(&sead::TTaskFactory<BfsarTask>);

        createTaskSync(arg);
    }

    //adjustHeapAll();
}

void RootTask::calc()
{
    // sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(getFramework());

    // sead::FixedSafeString<32> title;
    // title.format("FPS: %f", fw->calcFps());

    //fw->setCaption(title);
}

void RootTask::draw()
{
}
