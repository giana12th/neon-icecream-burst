#include "render.h"

#include <shlwapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <span>

#include "params.h"
#include "resource.h"

using Microsoft::WRL::ComPtr;

namespace {

D2D1_COLOR_F HsvToNeon(float hueDeg) {
    float h = hueDeg / 60.0f;
    float s = 1.0f, v = 1.0f;
    int i = static_cast<int>(h) % 6;
    float f = h - static_cast<int>(h);
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (i) {
        case 0: return D2D1::ColorF(v, t, p);
        case 1: return D2D1::ColorF(q, v, p);
        case 2: return D2D1::ColorF(p, v, t);
        case 3: return D2D1::ColorF(p, q, v);
        case 4: return D2D1::ColorF(t, p, v);
        default: return D2D1::ColorF(v, p, q);
    }
}

const std::vector<D2D1_COLOR_F>& NeonPalette() {
    static const std::vector<D2D1_COLOR_F> palette = {
        HsvToNeon(0),    // ネオンレッド
        HsvToNeon(45),   // ネオンオレンジ
        HsvToNeon(90),   // ネオンイエローグリーン
        HsvToNeon(135),  // ネオングリーン
        HsvToNeon(180),  // ネオンシアン
        HsvToNeon(225),  // ネオンブルー
        HsvToNeon(270),  // ネオンパープル
        HsvToNeon(315),  // ネオンピンク
    };
    return palette;
}

// exe に埋め込んだ particle.svg を取り出す
std::span<const BYTE> LoadSvgResource() {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(module, MAKEINTRESOURCEW(IDR_PARTICLE_SVG), RT_RCDATA);
    if (!res) return {};
    HGLOBAL handle = LoadResource(module, res);
    if (!handle) return {};
    auto* data = static_cast<const BYTE*>(LockResource(handle));
    return {data, SizeofResource(module, res)};
}

}  // namespace

Renderer::Renderer(ID2D1Factory1* factory, HWND hwnd, bool vsync)
    : factory_(factory), hwnd_(hwnd), vsync_(vsync), rng_(std::random_device{}()) {
    // 上限数ぶん先に確保しておき、フレーム中はメモリ確保しない
    particles_.reserve(params::kMaxParticles);
}

HRESULT Renderer::CreateDeviceResources() {
    if (context_) return S_OK;

    RECT rc;
    GetClientRect(hwnd_, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT32>(std::max<LONG>(1, rc.right - rc.left)),
        static_cast<UINT32>(std::max<LONG>(1, rc.bottom - rc.top)));

    // DPI を 96 に固定して、DIP とピクセルを一致させる
    auto rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f, 96.0f);
    auto hwndProps = D2D1::HwndRenderTargetProperties(
        hwnd_, size, vsync_ ? D2D1_PRESENT_OPTIONS_NONE : D2D1_PRESENT_OPTIONS_IMMEDIATELY);

    HRESULT hr = factory_->CreateHwndRenderTarget(rtProps, hwndProps, &target_);
    if (FAILED(hr)) return hr;

    // SVG 描画には ID2D1DeviceContext5（Windows 10 1703 以降）が必要
    hr = target_.As(&context_);
    if (SUCCEEDED(hr)) hr = GenerateNeonVariants();
    if (FAILED(hr)) DiscardDeviceResources();
    return hr;
}

void Renderer::DiscardDeviceResources() {
    variants_.clear();
    context_.Reset();
    target_.Reset();
}

// 8色ぶんのビットマップを起動時に作っておく（設計書 5.4）
HRESULT Renderer::GenerateNeonVariants() {
    std::span<const BYTE> svg = LoadSvgResource();
    if (svg.empty()) return E_FAIL;

    const float px = static_cast<float>(params::kBitmapSize);
    variants_.clear();

    for (const auto& color : NeonPalette()) {
        // オフスクリーン（透明背景）の描画先
        ComPtr<ID2D1BitmapRenderTarget> offscreen;
        HRESULT hr = context_->CreateCompatibleRenderTarget(
            D2D1::SizeF(px, px),
            D2D1::SizeU(params::kBitmapSize, params::kBitmapSize),
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
            &offscreen);
        if (FAILED(hr)) return hr;

        ComPtr<ID2D1DeviceContext5> offscreenContext;
        hr = offscreen.As(&offscreenContext);
        if (FAILED(hr)) return hr;

        ComPtr<IStream> stream;
        stream.Attach(SHCreateMemStream(svg.data(), static_cast<UINT>(svg.size())));
        if (!stream) return E_OUTOFMEMORY;

        ComPtr<ID2D1SvgDocument> svgDoc;
        hr = offscreenContext->CreateSvgDocument(stream.Get(), D2D1::SizeF(px, px), &svgDoc);
        if (FAILED(hr)) return hr;

        // 背景要素が残っていた場合の保険（本来は SVG 側で削除済み）
        ComPtr<ID2D1SvgElement> bg;
        if (SUCCEEDED(svgDoc->FindElementById(L"background", &bg)) && bg) {
            bg->SetAttributeValue(L"display", D2D1_SVG_ATTRIBUTE_STRING_TYPE_SVG, L"none");
        }

        // クリーム部分をネオンカラーに差し替え
        ComPtr<ID2D1SvgElement> cream;
        hr = svgDoc->FindElementById(L"icecream", &cream);
        if (FAILED(hr) || !cream) return E_FAIL;

        wchar_t hex[16];
        auto to255 = [](float v) { return static_cast<int>(v * 255.0f + 0.5f); };
        swprintf_s(hex, L"#%02X%02X%02X", to255(color.r), to255(color.g), to255(color.b));
        hr = cream->SetAttributeValue(L"fill", D2D1_SVG_ATTRIBUTE_STRING_TYPE_SVG, hex);
        if (FAILED(hr)) return hr;

        offscreenContext->BeginDraw();
        offscreenContext->Clear(D2D1::ColorF(0, 0, 0, 0));
        offscreenContext->DrawSvgDocument(svgDoc.Get());
        hr = offscreenContext->EndDraw();
        if (FAILED(hr)) return hr;

        ComPtr<ID2D1Bitmap> bitmap;
        hr = offscreen->GetBitmap(&bitmap);
        if (FAILED(hr)) return hr;
        variants_.push_back(bitmap);
    }
    return S_OK;
}

void Renderer::Resize(UINT width, UINT height) {
    if (target_) target_->Resize(D2D1::SizeU(width, height));
}

void Renderer::Frame(float dt) {
    if (FAILED(CreateDeviceResources())) return;
    if (target_->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED) return;

    const D2D1_SIZE_F screen = target_->GetSize();
    const float unit = screen.height / params::kReferenceHeight;
    Update(dt, screen);

    context_->BeginDraw();
    context_->SetTransform(D2D1::Matrix3x2F::Identity());
    context_->Clear(D2D1::ColorF(D2D1::ColorF::Black));  // 背景は黒

    if constexpr (params::kAdditiveBlend) context_->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_ADD);

    // 新しい（小さい）ものから描き、古い（大きい）ものを手前に重ねる
    const D2D1_POINT_2F center = D2D1::Point2F(screen.width / 2, screen.height / 2);
    for (auto it = particles_.rbegin(); it != particles_.rend(); ++it) {
        DrawParticle(*it, center, unit);
    }

    context_->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    context_->SetTransform(D2D1::Matrix3x2F::Identity());

    if (context_->EndDraw() == D2DERR_RECREATE_TARGET) DiscardDeviceResources();
}

void Renderer::Update(float dt, D2D1_SIZE_F screen) {
    const float unit = screen.height / params::kReferenceHeight;

    spawnTimer_ += dt;
    while (spawnTimer_ >= params::kSpawnInterval) {
        spawnTimer_ -= params::kSpawnInterval;
        TrySpawn(unit);
    }

    for (auto& p : particles_) {
        p.distance += p.speed * dt;
        p.scale += p.scaleSpeed * dt;
        p.rotation += p.rotationSpeed * dt;
    }

    // 画像全体が画面外に出たものを削除する。
    // イラストは中心から半径 0.53 × 表示サイズ程度に収まるので、それを当たり判定の半径とする
    std::erase_if(particles_, [&](const Particle& p) {
        const float x = screen.width / 2 + std::cos(p.angle) * p.distance;
        const float y = screen.height / 2 + std::sin(p.angle) * p.distance;
        const float r = params::kBaseSize * unit * p.scale * 0.53f;
        return x + r < 0 || x - r > screen.width || y + r < 0 || y - r > screen.height;
    });
}

void Renderer::TrySpawn(float unit) {
    if (particles_.size() >= static_cast<size_t>(params::kMaxParticles)) {
        if constexpr (params::kOverflowMode == params::OverflowMode::PauseSpawn) return;
        particles_.erase(particles_.begin());  // 最も古いものを削除
    }
    particles_.push_back(SpawnParticle(unit));
}

Particle Renderer::SpawnParticle(float unit) {
    Particle p;
    p.angle = RandomFloat(0.0f, 2 * std::numbers::pi_v<float>);  // 以後固定（直線移動）
    p.distance = 0.0f;
    p.speed = RandomFloat(params::kSpeedMin, params::kSpeedMax) * unit;
    p.scale = params::kInitialScale;
    p.scaleSpeed = RandomFloat(params::kScaleSpeedMin, params::kScaleSpeedMax);
    p.rotation = RandomFloat(0.0f, 360.0f);
    p.rotationSpeed = RandomFloat(params::kRotationSpeedMin, params::kRotationSpeedMax);
    p.bitmapIndex = std::uniform_int_distribution<int>(
        0, static_cast<int>(variants_.size()) - 1)(rng_);
    return p;
}

void Renderer::DrawParticle(const Particle& p, D2D1_POINT_2F center, float unit) {
    const float x = center.x + std::cos(p.angle) * p.distance;
    const float y = center.y + std::sin(p.angle) * p.distance;
    const float half = params::kBaseSize * unit * p.scale * 0.5f;

    // 回転は画像の中心を軸にする
    context_->SetTransform(D2D1::Matrix3x2F::Rotation(p.rotation, D2D1::Point2F(x, y)));
    const D2D1_RECT_F dest = D2D1::RectF(x - half, y - half, x + half, y + half);
    context_->DrawBitmap(variants_[p.bitmapIndex].Get(), &dest, 1.0f,
                         D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, nullptr);
}

float Renderer::RandomFloat(float min, float max) {
    return std::uniform_real_distribution<float>(min, max)(rng_);
}
