# neon-icecream-burst

画面の中心から、ネオンカラーのアイスクリームが回転しながら飛び出してくる Windows スクリーンセーバー（C++20 / Direct2D）。

## ビルド

Visual Studio Build Tools（C++ によるデスクトップ開発）と Ninja が必要です。
「x64 Native Tools Command Prompt」などの MSVC 環境で次を実行します。

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

`build/NeonIcecreamBurst.scr` ができます（中身は `.exe` と同じ）。
GitHub Actions でもビルドしており、各実行の Artifacts から `.scr` をダウンロードできます。

## 動作確認

| コマンド | 動作 |
|---|---|
| `NeonIcecreamBurst.exe /w` | 開発用のウィンドウ表示（1280×720）。Esc で終了 |
| `NeonIcecreamBurst.exe /s` | フルスクリーン表示（全モニタ）。マウス移動・キー・クリックで終了 |
| `.scr` を右クリック →「テスト」 | `/s` と同じ |
| `.scr` を右クリック →「インストール」 | スクリーンセーバーとして登録 |

`/c`（設定）は、現時点では「設定項目なし」のメッセージを表示するだけです。

## パラメータ調整

スポーン間隔・上限数・速度・拡大速度・回転速度などは `src/params.h` にまとめてあります（仮の値）。

## 素材

`resources/particle.svg` はアイスクリームのイラストです。Direct2D の SVG は `<style>`・`class` に対応していないため、元の SVG から次のように変換しています。

- 背景の四角（`<g id="bg">`）を削除
- CSS クラスの色を、各要素の `fill` / `fill-opacity` 属性に展開
- コーンのハイライト（`mix-blend-mode: soft-light`）は非対応のため、半透明（`fill-opacity="0.35"`）で近似

クリーム部分（`id="icecream"`）の色は、起動時に 8 色のネオンカラーに差し替えています。

## 注意

未署名のため、初回実行時に SmartScreen の警告が出ることがあります。
