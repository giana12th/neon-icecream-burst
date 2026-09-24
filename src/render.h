#pragma once

#include <windows.h>
#include <d2d1_3.h>
#include <wrl/client.h>

#include <random>
#include <vector>

struct Particle {
    float angle;          // 移動方向（ラジアン。スポーン時に決めて以後固定）
    float distance;       // 中心からの距離（px）
    float speed;          // 移動速度（px/秒）
    float scale;          // 現在のサイズ
    float scaleSpeed;     // サイズの増加速度（スケール/秒）
    float rotation;       // 現在の回転角（度）
    float rotationSpeed;  // 回転速度（度/秒。正負で回転方向が変わる）
    int bitmapIndex;      // 色バリエーションの番号
};

// 1つのウィンドウに対する描画処理
class Renderer {
public:
    Renderer(ID2D1Factory1* factory, HWND hwnd, bool vsync);

    void Resize(UINT width, UINT height);
    void Frame(float dt);

private:
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();
    HRESULT GenerateNeonVariants();

    void Update(float dt, D2D1_SIZE_F screen);
    void TrySpawn(float unit);
    Particle SpawnParticle(float unit);
    void DrawParticle(const Particle& p, D2D1_POINT_2F center, float unit);

    float RandomFloat(float min, float max);

    Microsoft::WRL::ComPtr<ID2D1Factory1> factory_;
    HWND hwnd_;
    bool vsync_;

    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext5> context_;
    std::vector<Microsoft::WRL::ComPtr<ID2D1Bitmap>> variants_;

    std::vector<Particle> particles_;  // スポーン順（古い順）
    float spawnTimer_ = 0.0f;
    std::mt19937 rng_;
};
