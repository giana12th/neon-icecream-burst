#pragma once

// 実機で調整するための仮パラメータ（設計書 6章）
// 距離・速度・サイズは「画面の高さ 1080px」を基準にした値。
// 実際の画面の高さに合わせてスケールするので、プレビューの小窓でも見た目がほぼ同じになる。
namespace params {

// 基準の画面高さ（px）
inline constexpr float kReferenceHeight = 1080.0f;

// スポーン間隔（秒）
inline constexpr float kSpawnInterval = 0.5f;

// パーティクル数の上限
inline constexpr int kMaxParticles = 40;

// 上限に達したときの挙動
enum class OverflowMode {
    PauseSpawn,     // (a) 新規スポーンを止め、既存のパーティクルが画面外へ消えるのを待つ
    ReplaceOldest,  // (b) 最も古いパーティクルを消して新しいものを出す
};
inline constexpr OverflowMode kOverflowMode = OverflowMode::PauseSpawn;

// 移動速度（px/秒）
inline constexpr float kSpeedMin = 80.0f;
inline constexpr float kSpeedMax = 200.0f;

// スケール（1.0 のとき kBaseSize px で表示）
inline constexpr float kInitialScale = 0.1f;
inline constexpr float kScaleSpeedMin = 0.3f;  // スケール/秒
inline constexpr float kScaleSpeedMax = 0.8f;

// 回転速度（度/秒）。正負で回転方向が変わる
inline constexpr float kRotationSpeedMin = -90.0f;
inline constexpr float kRotationSpeedMax = 90.0f;

// スケール 1.0 のときの表示サイズ（px）
inline constexpr float kBaseSize = 100.0f;

// SVG をラスタライズするビットマップの解像度（px）。大きいほど拡大時にきれい
inline constexpr unsigned kBitmapSize = 512;

// 加算合成（重なった部分が明るく光る）。設計書 5.9 の検討オプション
inline constexpr bool kAdditiveBlend = false;

}  // namespace params
