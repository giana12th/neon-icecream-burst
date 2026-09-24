#include <windows.h>
#include <shellapi.h>

#include <cwctype>
#include <cstdlib>

#include "window.h"

namespace {

HWND ParseHwnd(const wchar_t* text) {
    return reinterpret_cast<HWND>(static_cast<INT_PTR>(_wcstoui64(text, nullptr, 10)));
}

}  // namespace

// 引数:
//   /s          フルスクリーンで実行
//   /p HWND     プレビュー（/p:HWND も可）
//   /c[:HWND]   設定（現時点では設定項目なし）
//   /w          開発用のウィンドウ表示
//   引数なし     フルスクリーンで実行
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    Mode mode = Mode::Fullscreen;
    HWND parent = nullptr;

    if (argv && argc >= 2 && (argv[1][0] == L'/' || argv[1][0] == L'-')) {
        const wchar_t option = static_cast<wchar_t>(std::towlower(argv[1][1]));
        const wchar_t* value = argv[1] + 2;
        if (*value == L':') ++value;
        if (*value == L'\0' && argc >= 3) value = argv[2];

        switch (option) {
            case L's': mode = Mode::Fullscreen; break;
            case L'p': mode = Mode::Preview; parent = ParseHwnd(value); break;
            case L'c': mode = Mode::Configure; parent = ParseHwnd(value); break;
            case L'w': mode = Mode::Windowed; break;
        }
    }
    if (argv) LocalFree(argv);

    if (mode == Mode::Configure) {
        MessageBoxW(parent, L"このスクリーンセーバーには設定項目がありません。",
                    L"Neon Icecream Burst", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (mode == Mode::Preview && !IsWindow(parent)) return 0;

    return RunScreensaver(instance, mode, parent);
}
