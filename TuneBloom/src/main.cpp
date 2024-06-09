#include <heap/seadExpHeap.h>
#include <heap/seadHeapMgr.h>

#include "AppFramework.h"
#include "tasks/RootTask.h"

#include "Utilll.h"

int main()
{
    sead::Framework::InitializeArg initArg;
    initArg.heap_size = 100 * 1024 * 1024; // 100 MB
    AppFramework::initialize(initArg);

    sead::HeapMgr::createUnboundHeap();

    sead::GameFrameworkBaseWin::CreateArg createArg;
    createArg.wait_vblank = 1;
    createArg.window_name = util::cAppName;
    createArg.clear_color = sead::Color4f(0.0f, 0.0f, 0.3f, 1.0f);

    AppFramework* framework = nullptr;
    {
        sead::ExpHeap* heap = sead::ExpHeap::create(0, "AppFramework", sead::HeapMgr::getRootHeap(0));
        framework = new(heap) AppFramework(createArg);
        heap->adjust();
    }

    util::setFramework_(framework);

    {
        sead::ExpHeap* heap = sead::ExpHeap::create(0, "GraphicSystem", sead::HeapMgr::getRootHeap(0));
        framework->initializeGraphicsSystem(heap, sead::Vector2f(1280.0f, 720.0f));
        heap->adjust();
    }

    sead::TaskBase::CreateArg rootArg(&sead::TTaskFactory<RootTask>);
    sead::Framework::RunArg runArg;
    framework->run(sead::HeapMgr::getRootHeap(0), rootArg, runArg);

    delete framework;
}

#ifdef SEAD_PLATFORM_WINDOWS
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    main();
    return 0;
}
#endif // SEAD_PLATFORM_WINDOWS
