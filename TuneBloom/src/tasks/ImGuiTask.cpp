#include "tasks/ImGuiTask.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_win32.h"

#include <filedevice/seadFileDeviceMgr.h>
#include <framework/win/seadGameFrameworkWinGL.h>
#include <framework/seadProcessMeter.h>
#include <heap/seadExpHeap.h>

#include <ui/UI.h>

#include <shellapi.h>

#include "icons/IconsLucide.h"

SEAD_TASK_SINGLETON_DISPOSER_IMPL(ImGuiTask);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct WGL_WindowData
{
    HDC hDC;
};

static bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    HDC hDc = ::GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0)
        return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
        return false;
    ::ReleaseDC(hWnd, hDc);

    data->hDC = ::GetDC(hWnd);

    return true;
}

static void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data)
{
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hWnd, data->hDC);
}

static void HookRendererCreateWindow(ImGuiViewport* viewport)
{
    SEAD_ASSERT(!viewport->RendererUserData);

    WGL_WindowData* data = IM_NEW(WGL_WindowData);
    bool b = CreateDeviceWGL((HWND)viewport->PlatformHandle, data);
    SEAD_ASSERT(b);
    viewport->RendererUserData = data;
}

static void HookRendererDestroyWindow(ImGuiViewport* viewport)
{
    if (viewport->RendererUserData != NULL)
    {
        WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData;
        CleanupDeviceWGL((HWND)viewport->PlatformHandle, data);
        IM_DELETE(data);
        viewport->RendererUserData = NULL;
    }
}

static void HookPlatformRenderWindow(ImGuiViewport* viewport, void* hglrc)
{
    // Activate the platform window DC in the OpenGL rendering context
    WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData;
    SEAD_ASSERT(data);

    {
        bool b = wglMakeCurrent(data->hDC, (HGLRC)hglrc);
        SEAD_ASSERT(b);
    }
}

static void HookRendererSwapBuffers(ImGuiViewport* viewport, void*)
{
    WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData;
    SEAD_ASSERT(data);

    {
        bool b = ::SwapBuffers(data->hDC);
        SEAD_ASSERT(b);
    }
}

static LRESULT __stdcall MsgProc(HWND hWnd, u32 msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 1;

    switch (msg)
    {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                return 1;
            }

        case WM_CLOSE:
            TryExit();
            return 1;

        case WM_DROPFILES:
        {
            HDROP drop = (HDROP)wParam;

            UINT fileCount = DragQueryFileA(drop, 0xFFFFFFFF, NULL, 0);
            if (fileCount > 0)
            {
                DragQueryFileA(drop, 0, sDroppedFilePath.getBuffer(), sDroppedFilePath.getBufferSize());
            }
            DragFinish(drop);

            return 1;
        }
    }

    return 0;
}

static void SetupImGuiStyle();

static void* ImGuiAllocFunc(size_t size, void* userPtr)
{
    sead::Heap* heap = static_cast<sead::Heap*>(userPtr);

    return heap->alloc(size);
}

static void ImGuiFreeFunc(void* ptr, void* userPtr)
{
    sead::Heap* heap = static_cast<sead::Heap*>(userPtr);

    heap->free(ptr);
}

ImGuiTask::ImGuiTask(const sead::TaskConstructArg& arg)
    : sead::Task(arg, "ImGuiTask")
    , mCalcMeter("ImGuiTask::calc", sead::Color4f::cCyan)
    , mDrawMeter("ImGuiTask::draw", sead::Color4f::cBlue)
{
}

void ImGuiTask::prepare()
{
    {
        sead::ExpHeap* heap = sead::ExpHeap::create(90 * 1024 * 1024, "ImGui::Heap", nullptr);
        ImGui::SetAllocatorFunctions(&ImGuiAllocFunc, &ImGuiFreeFunc, heap);
        //ImGui::SetAllocatorFunctions(&ImGuiAllocFunc, &ImGuiFreeFunc, sead::HeapMgr::getUnboundHeap());
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;     // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    SetupImGuiStyle();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_Right;

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        //style.WindowRounding = 0.0f;
        //style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    {
        sead::FileDevice::LoadArg arg;
        //arg.path = "main://fonts/SegoeUI.ttf";
        //arg.path = "main://fonts/CamingoCode-Regular.ttf";
        //arg.path = "main://fonts/tahoma.ttf";
        arg.path = "main://fonts/SpaceMono-Regular.ttf";

        u8* font = sead::FileDeviceMgr::instance()->load(arg);

        ImFontConfig config;
        //config.OversampleH = 2;
        //config.OversampleV = 2;
        //config.GlyphMinAdvanceX = 20.0f;

        io.Fonts->AddFontFromMemoryTTF(font, arg.read_size, 32.0f, &config);
        ImFont* smallFont = io.Fonts->AddFontFromMemoryTTF(font, arg.read_size, 20.0f, &config);
        io.FontDefault = smallFont;
    }

    {
        sead::FileDevice::LoadArg arg;
        arg.path = "main://fonts/" FONT_ICON_FILE_NAME_LC;

        u8* font = sead::FileDeviceMgr::instance()->load(arg);

        f32 baseFontSize = 18.0f; // 13.0f is the size of the default font. Change to the font size you use.
        f32 iconFontSize = baseFontSize; // * 2.0f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

        static const ImWchar iconsRanges[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };
        ImFontConfig iconsConfig;
        iconsConfig.MergeMode = true;
        iconsConfig.PixelSnapH = true;
        iconsConfig.GlyphOffset.y += 4.0f;
        //iconsConfig.OversampleH = 3;
        //iconsConfig.OversampleV = 3;
        //iconsConfig.GlyphMinAdvanceX = iconFontSize;
        iconsConfig.RasterizerDensity = 3.0f;

        io.Fonts->AddFontFromMemoryTTF(font, arg.read_size, iconFontSize, &iconsConfig, iconsRanges);
    }

    sead::GameFrameworkBaseWin* fw = sead::DynamicCast<sead::GameFrameworkBaseWin>(getFramework());
    fw->setMsgProcCallback(&MsgProc);

    sead::Graphics::instance()->lockDrawContext();
    {
        // Setup Platform/Renderer backends
        ImGui_ImplWin32_InitForOpenGL(fw->getWindowHandle());
        ImGui_ImplOpenGL3_Init();
    }
    sead::Graphics::instance()->unlockDrawContext();

    // Win32+GL needs specific hooks for viewport, as there are specific things needed to tie Win32 and GL api.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        IM_ASSERT(!platform_io.Renderer_CreateWindow);
        IM_ASSERT(!platform_io.Renderer_DestroyWindow);
        IM_ASSERT(!platform_io.Renderer_SwapBuffers);
        IM_ASSERT(!platform_io.Platform_RenderWindow);
        platform_io.Renderer_CreateWindow = HookRendererCreateWindow;
        platform_io.Renderer_DestroyWindow = HookRendererDestroyWindow;
        platform_io.Renderer_SwapBuffers = HookRendererSwapBuffers;
        platform_io.Platform_RenderWindow = HookPlatformRenderWindow;
    }

    adjustHeapAll();
}

void ImGuiTask::enter()
{
    if (sead::ProcessMeter::instance())
    {
        sead::ProcessMeter::instance()->attachProcessMeterBar(&mCalcMeter);
        sead::ProcessMeter::instance()->attachProcessMeterBar(&mDrawMeter);
    }
}

void ImGuiTask::exit()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiTask::calc()
{
    mCalcMeter.measureBegin();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    mCalcMeter.measureEnd();
}

void ImGuiTask::draw()
{
    mDrawMeter.measureBegin();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        sead::GameFrameworkWinGL* fw = sead::DynamicCast<sead::GameFrameworkWinGL>(getFramework());

        HDC hdc = wglGetCurrentDC();
        HGLRC hglrc = wglGetCurrentContext();

        GLint prevFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(fw->getHGLRC());

        // Restore the OpenGL rendering context to the main window DC, since platform windows might have changed it.
        wglMakeCurrent(hdc, hglrc);

        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    }

    mDrawMeter.measureEnd();
}

static void SetupImGuiStyle()
{
/*
	// Dark Ruda style
	ImGuiStyle& style = ImGui::GetStyle();

	style.ScaleAllSizes(1.5f);

	style.Alpha = 1.0f;
	style.DisabledAlpha = 0.6000000238418579f;
	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.WindowRounding = 0.0f;
	style.WindowBorderSize = 1.0f;
	style.WindowMinSize = ImVec2(32.0f, 32.0f);
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(4.0f, 3.0f);
	style.FrameRounding = 4.0f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
	style.CellPadding = ImVec2(4.0f, 2.0f);
	style.IndentSpacing = 21.0f;
	style.ColumnsMinSpacing = 6.0f;
	style.ScrollbarSize = 14.0f;
	style.ScrollbarRounding = 9.0f;
	style.GrabMinSize = 10.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 4.0f;
	style.TabBorderSize = 0.0f;
	style.TabMinWidthForCloseButton = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

	style.Colors[ImGuiCol_Text] = ImVec4(0.9490196108818054f, 0.95686274766922f, 0.9764705896377563f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.3568627536296844f, 0.4196078479290009f, 0.4666666686534882f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1098039224743843f, 0.1490196138620377f, 0.168627455830574f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.1490196138620377f, 0.1764705926179886f, 0.2196078449487686f, 1.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0784313753247261f, 0.0784313753247261f, 0.0784313753247261f, 0.9399999976158142f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.0784313753247261f, 0.09803921729326248f, 0.1176470592617989f, 1.0f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2000000029802322f, 0.2470588237047195f, 0.2862745225429535f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.1176470592617989f, 0.2000000029802322f, 0.2784313857555389f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.08627451211214066f, 0.1176470592617989f, 0.1372549086809158f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08627451211214066f, 0.1176470592617989f, 0.1372549086809158f, 0.6499999761581421f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0784313753247261f, 0.09803921729326248f, 0.1176470592617989f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.5099999904632568f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1490196138620377f, 0.1764705926179886f, 0.2196078449487686f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01960784383118153f, 0.01960784383118153f, 0.01960784383118153f, 0.3899999856948853f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.2000000029802322f, 0.2470588237047195f, 0.2862745225429535f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.1764705926179886f, 0.2196078449487686f, 0.2470588237047195f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.08627451211214066f, 0.2078431397676468f, 0.3098039329051971f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.2784313857555389f, 0.5568627715110779f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.2784313857555389f, 0.5568627715110779f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.3686274588108063f, 0.6078431606292725f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.2000000029802322f, 0.2470588237047195f, 0.2862745225429535f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2784313857555389f, 0.5568627715110779f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.05882352963089943f, 0.529411792755127f, 0.9764705896377563f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.2000000029802322f, 0.2470588237047195f, 0.2862745225429535f, 0.550000011920929f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.800000011920929f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.2000000029802322f, 0.2470588237047195f, 0.2862745225429535f, 1.0f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.09803921729326248f, 0.4000000059604645f, 0.7490196228027344f, 0.7799999713897705f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.09803921729326248f, 0.4000000059604645f, 0.7490196228027344f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.25f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.6700000166893005f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.949999988079071f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.1098039224743843f, 0.1490196138620377f, 0.168627455830574f, 1.0f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.800000011920929f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.2000000029802322f, 0.2470588237047195f, 0.2862745225429535f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.1098039224743843f, 0.1490196138620377f, 0.168627455830574f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1098039224743843f, 0.1490196138620377f, 0.168627455830574f, 1.0f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6078431606292725f, 0.6078431606292725f, 0.6078431606292725f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.4274509847164154f, 0.3490196168422699f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8980392217636108f, 0.6980392336845398f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6000000238418579f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05999999865889549f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.3499999940395355f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.8999999761581421f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.2000000029802322f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.3499999940395355f);
*/
/*
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
    colors[ImGuiCol_Border]                 = ImVec4(0.54f, 0.54f, 0.54f, 0.29f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_Button]                 = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
    colors[ImGuiCol_Separator]              = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding                     = ImVec2(8.00f, 8.00f);
    style.FramePadding                      = ImVec2(5.00f, 2.00f);
    style.CellPadding                       = ImVec2(6.00f, 6.00f);
    style.ItemSpacing                       = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing                  = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding                 = ImVec2(0.00f, 0.00f);
    style.IndentSpacing                     = 25;
    style.ScrollbarSize                     = 15;
    style.GrabMinSize                       = 15;
    style.WindowBorderSize                  = 1;
    style.ChildBorderSize                   = 1;
    style.PopupBorderSize                   = 1;
    style.FrameBorderSize                   = 1;
    style.TabBorderSize                     = 1;
    style.WindowRounding                    = 7;
    style.ChildRounding                     = 4;
    style.FrameRounding                     = 3;
    style.PopupRounding                     = 4;
    style.ScrollbarRounding                 = 9;
    style.GrabRounding                      = 3;
    style.LogSliderDeadzone                 = 4;
    style.TabRounding                       = 4;
*/

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.3f;
    style.FrameRounding = 2.3f;
    style.ScrollbarRounding = 12;
    style.GrabRounding = 2;

    style.Colors[ImGuiCol_Text]                  = ImVec4(0.90f, 0.90f, 0.90f, 0.90f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
    //style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.05f, 0.05f, 0.10f, 0.85f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.70f, 0.70f, 0.70f, 0.65f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.00f, 0.00f, 0.01f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.90f, 0.80f, 0.80f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.90f, 0.65f, 0.65f, 0.45f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.83f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.00f, 0.00f, 0.00f, 0.87f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.01f, 0.01f, 0.02f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.55f, 0.53f, 0.55f, 0.51f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.56f, 0.56f, 0.56f, 0.91f);
    //style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.1f, 0.1f, 0.1f, 0.99f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.90f, 0.90f, 0.90f, 0.83f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.70f, 0.70f, 0.70f, 0.62f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.30f, 0.30f, 0.30f, 0.84f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.48f, 0.72f, 0.89f, 0.49f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.50f, 0.69f, 0.99f, 0.68f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.30f, 0.69f, 1.00f, 0.53f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.44f, 0.61f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.38f, 0.62f, 0.83f, 1.00f);
    //style.Colors[ImGuiCol_Column]                = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    //style.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.70f, 0.60f, 0.60f, 1.00f);
    //style.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.90f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    //style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.50f, 0.50f, 0.90f, 0.50f);
    //style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.70f, 0.70f, 0.90f, 0.60f);
    //style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.45f, 0.39f, 0.96f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
    //style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}
