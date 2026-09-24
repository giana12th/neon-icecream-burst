#include "window.h"

#include <d2d1_1.h>
#include <wrl/client.h>

#include <cstdlib>
#include <memory>
#include <vector>

#include "render.h"

using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kClassName[] = L"NeonIcecreamBurstWindow";
constexpr int kMouseMoveThreshold = 8;  // この px 以上マウスが動いたら終了

struct WindowState {
    Mode mode;
    std::unique_ptr<Renderer> renderer;
};

POINT g_initialCursor{};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    auto* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!state) return DefWindowProcW(hwnd, msg, wParam, lParam);
    const bool exitOnInput = state->mode == Mode::Fullscreen;

    switch (msg) {
        case WM_SIZE:
            if (state->renderer) state->renderer->Resize(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_PAINT:
            ValidateRect(hwnd, nullptr);  // 描画はメインループで行う
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SETCURSOR:
            if (exitOnInput) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;

        case WM_MOUSEMOVE:
            if (exitOnInput) {
                POINT pt;
                GetCursorPos(&pt);
                if (std::abs(pt.x - g_initialCursor.x) > kMouseMoveThreshold ||
                    std::abs(pt.y - g_initialCursor.y) > kMouseMoveThreshold) {
                    PostQuitMessage(0);
                }
            }
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            if (exitOnInput) {
                PostQuitMessage(0);
                return 0;
            }
            if (state->mode == Mode::Windowed && msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_ACTIVATEAPP:
            if (exitOnInput && !wParam) PostQuitMessage(0);
            break;

        case WM_SYSCOMMAND:
            // 実行中にスクリーンセーバーが二重起動しないようにする
            if (exitOnInput && (wParam & 0xFFF0) == SC_SCREENSAVE) return 0;
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

BOOL CALLBACK CollectMonitor(HMONITOR, HDC, LPRECT rect, LPARAM lParam) {
    reinterpret_cast<std::vector<RECT>*>(lParam)->push_back(*rect);
    return TRUE;
}

}  // namespace

int RunScreensaver(HINSTANCE instance, Mode mode, HWND parent) {
    // 高 DPI 環境でも物理ピクセルで描画する
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ComPtr<ID2D1Factory1> factory;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf()))) {
        return 1;
    }

    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    GetCursorPos(&g_initialCursor);

    std::vector<std::unique_ptr<WindowState>> states;
    std::vector<HWND> windows;

    auto createWindow = [&](DWORD exStyle, DWORD style, const wchar_t* title, int x, int y,
                            int w, int h, HWND parentWindow, bool vsync) {
        auto state = std::make_unique<WindowState>();
        state->mode = mode;
        HWND hwnd = CreateWindowExW(exStyle, kClassName, title, style, x, y, w, h, parentWindow,
                                    nullptr, instance, state.get());
        if (!hwnd) return;
        state->renderer = std::make_unique<Renderer>(factory.Get(), hwnd, vsync);
        windows.push_back(hwnd);
        states.push_back(std::move(state));
    };

    switch (mode) {
        case Mode::Fullscreen: {
            // モニタごとにウィンドウを作る。垂直同期待ちは 1 枚目だけにする
            std::vector<RECT> monitors;
            EnumDisplayMonitors(nullptr, nullptr, CollectMonitor,
                                reinterpret_cast<LPARAM>(&monitors));
            for (size_t i = 0; i < monitors.size(); ++i) {
                const RECT& r = monitors[i];
                createWindow(WS_EX_TOPMOST, WS_POPUP | WS_VISIBLE, L"", r.left, r.top,
                             r.right - r.left, r.bottom - r.top, nullptr, i == 0);
            }
            break;
        }
        case Mode::Preview: {
            RECT r;
            GetClientRect(parent, &r);
            createWindow(0, WS_CHILD | WS_VISIBLE, L"", 0, 0, r.right, r.bottom, parent, true);
            break;
        }
        case Mode::Windowed: {
            RECT r{0, 0, 1280, 720};
            AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
            createWindow(0, WS_OVERLAPPEDWINDOW | WS_VISIBLE, L"Neon Icecream Burst (debug)",
                         CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                         nullptr, true);
            break;
        }
        case Mode::Configure:
            return 0;
    }
    if (windows.empty()) return 1;

    LARGE_INTEGER freq, prev;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    MSG msg{};
    bool running = true;
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float dt = static_cast<float>(now.QuadPart - prev.QuadPart) / freq.QuadPart;
        prev = now;
        if (dt > 0.1f) dt = 0.1f;  // 復帰直後などに大きく飛ばないようにする

        for (auto& state : states) state->renderer->Frame(dt);

        // 垂直同期が効かない状況（ウィンドウが隠れている等）で CPU を使い切らないようにする
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        const double frameMs = (end.QuadPart - now.QuadPart) * 1000.0 / freq.QuadPart;
        if (frameMs < 4.0) Sleep(static_cast<DWORD>(16.0 - frameMs));
    }

    for (HWND hwnd : windows) {
        if (IsWindow(hwnd)) DestroyWindow(hwnd);
    }
    return static_cast<int>(msg.wParam);
}
