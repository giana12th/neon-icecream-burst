#pragma once

#include <windows.h>

enum class Mode {
    Fullscreen,  // /s : 全モニタにフルスクリーン表示。入力で終了
    Preview,     // /p HWND : 画面のプロパティの小窓に描画
    Configure,   // /c : 設定（現時点では設定項目なし）
    Windowed,    // /w : 開発用のウィンドウ表示。Esc か閉じるボタンで終了
};

// ウィンドウを作成してメッセージループを回す。終了コードを返す
int RunScreensaver(HINSTANCE instance, Mode mode, HWND parent);
