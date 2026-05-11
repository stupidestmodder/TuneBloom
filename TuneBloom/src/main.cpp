#include <devenv/seadAssertConfig.h>
#include <heap/seadExpHeap.h>
#include <heap/seadHeapMgr.h>

#include "AppFramework.h"
#include "tasks/RootTask.h"

#include "Utilll.h"

void AssertException(const char* msg)
{
#if defined(SEAD_PLATFORM_WINDOWS)
    sead::FormatFixedSafeString<2048> info("%s\nAn unexpected error has ocurred, please report this issue on 'https://github.com/stupidestmodder/TuneBloom/issues' or ask here 'https://go.nsmbu.net/discord'", msg);
    MessageBoxA(nullptr, info.cstr(), "ASSERTION ERROR", 0);
#endif // SEAD_PLATFORM_WINDOWS
}

int main()
{
    sead::Delegate1 deleg = sead::FunctionDelegateCreator<const char*>(&AssertException);
    sead::AssertConfig::registerFinalCallback(&deleg);

    sead::Framework::InitializeArg initArg;
    initArg.heap_size = 100 * 1024 * 1024; // 100 MB
    AppFramework::initialize(initArg);

    sead::HeapMgr::createUnboundHeap();

    sead::GameFrameworkBaseWin::CreateArg createArg;
    createArg.wait_vblank = 0;
    createArg.window_name = util::cAppName;
    createArg.window_ex_style = WS_EX_ACCEPTFILES; // win32 for drag-and-drop
    createArg.clear_color = sead::Color4f(0.0f, 0.0f, 0.3f, 1.0f);

    AppFramework* framework = nullptr;
    {
        sead::ExpHeap* heap = sead::ExpHeap::create(0, "AppFramework", sead::HeapMgr::getRootHeap(0));
        framework = new(heap) AppFramework(createArg);
        heap->adjust();
    }

    util::setFramework_(framework);
    util::updateTitle(nullptr);

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

#if defined(SEAD_PLATFORM_WINDOWS)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    main();
    return 0;
}
#endif // SEAD_PLATFORM_WINDOWS

extern "C"
{
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}
