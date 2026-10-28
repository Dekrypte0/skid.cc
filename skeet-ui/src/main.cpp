// skeet-ui — standalone Win32 + Direct3D 11 host for the menu framework.
// The UI itself lives in src/ui and is host-agnostic: call ui::Render() from any
// ImGui frame (the cheat, a tool, a demo) and it draws the same menu.

#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "ui/ui.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <tchar.h>
#include <windows.h>

#include <cmath>

// ---------------------------------------------------------------------------
// d3d11 state
// ---------------------------------------------------------------------------
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))) && pBackBuffer)
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_10_0;
    const HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
    {
        // Fall back to WARP so the demo still opens on machines without a GPU driver.
        D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
            &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    }
    if (!g_pSwapChain || !g_pd3dDevice || !g_pd3dDeviceContext)
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

// ---------------------------------------------------------------------------
// window state
// ---------------------------------------------------------------------------
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool g_menuVisible = true;
static UINT g_hotkeyId = 0xB001;

static void CenterWindow(HWND hwnd)
{
    RECT r{};
    GetWindowRect(hwnd, &r);
    const int w = r.right - r.left;
    const int h = r.bottom - r.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void ToggleMenu(HWND hwnd)
{
    g_menuVisible = !g_menuVisible;
    ui::SetVisible(g_menuVisible);
    ShowWindow(hwnd, g_menuVisible ? SW_SHOW : SW_HIDE);
    if (g_menuVisible)
    {
        SetForegroundWindow(hwnd);
        SetActiveWindow(hwnd);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_HOTKEY:
        if (wParam == g_hotkeyId)
        {
            ToggleMenu(hWnd);
            return 0;
        }
        break;
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
                                        DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) // disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// entry point
// ---------------------------------------------------------------------------
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    // Per-monitor v2 DPI so the menu stays crisp on scaled displays.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const UINT dpi = GetDpiForSystem();
    const float scale = (float)dpi / 96.0f;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC | CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(10, 10, 10));
    wc.lpszClassName = L"skeet-ui-window";
    RegisterClassExW(&wc);

    const int winW = (int)std::lround(780.0 * scale);
    const int winH = (int)std::lround(620.0 * scale);
    const int winX = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    const int winY = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW, wc.lpszClassName, L"skeet ui", WS_POPUP,
        winX, winY, winW, winH, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd)
        return 1;

    // Rounded corners + dark chrome where the OS supports it.
    {
        const DWORD dark = 1;
        DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)); // DWMWA_USE_IMMERSIVE_DARK_MODE
        const DWORD corner = 2;
        DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner)); // DWMWA_WINDOW_CORNER_PREFERENCE
    }
    CenterWindow(hwnd);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    RegisterHotKey(hwnd, g_hotkeyId, MOD_NOREPEAT, VK_INSERT);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // menu state is ours, not imgui.ini
    io.LogFilename = nullptr;

    ui::Initialize(scale);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    const float clear[4] = { 10.0f / 255.0f, 10.0f / 255.0f, 10.0f / 255.0f, 1.0f };

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
            {
                done = true;
                break;
            }
        }
        if (done)
            break;

        if (ui::ConsumeCloseRequest())
        {
            DestroyWindow(hwnd);
            break;
        }

        if (!g_menuVisible)
        {
            Sleep(20);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ui::Render();

        ImVec2 drag;
        if (ui::TakeDrag(drag))
        {
            RECT r{};
            GetWindowRect(hwnd, &r);
            SetWindowPos(hwnd, nullptr,
                         r.left + (int)drag.x, r.top + (int)drag.y,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0); // vsync
    }

    UnregisterHotKey(hwnd, g_hotkeyId);
    ui::Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
