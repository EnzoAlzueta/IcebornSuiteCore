#include "runtime.hpp"
#include "panel_painter.hpp"
#include "dx11_surface.hpp"
#include <algorithm>
#include <cmath>
#include <d2d1_1.h>

namespace meter {
using Microsoft::WRL::ComPtr;
namespace {
struct Graphics {
    std::mutex mutex;
    Dx11Surface surface;
    PanelPainter painter;
    ComPtr<IDXGISwapChain> chain;
    ComPtr<ID3D11Device> device;
    ComPtr<ID2D1DeviceContext> context;
    ComPtr<ID2D1Bitmap1> target;
    HWND window{};
    bool failed{}, resizing{}, painted{};
    unsigned menuRevision{};
    Scene scene;
    Placement cachedPlacement;
    Placement cachedTimers;
    CrownSettings cachedCrowns;
};
// The pinned runtime's driver objects must outlive DLL_PROCESS_DETACH.
Graphics& graphics() { static auto* g = new Graphics; return *g; }
HRESULT initialize(Graphics& g) {
    ComPtr<ID3D11Device> ui;
    ComPtr<ID3D11DeviceContext> immediate;
    HRESULT hr = g.surface.initialize(g.device.Get(), &ui, &immediate);
    if (FAILED(hr)) return hr;
    ComPtr<IDXGIDevice> dxgi;
    hr = ui.As(&dxgi);
    if (FAILED(hr)) return hr;
    ComPtr<ID2D1Factory1> factory;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf());
    if (FAILED(hr)) return hr;
    ComPtr<ID2D1Device> d2d;
    hr = factory->CreateDevice(dxgi.Get(), &d2d);
    if (FAILED(hr)) return hr;
    hr = d2d->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g.context);
    if (FAILED(hr)) return hr;
    g.context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    return g.painter.initialize(g.context.Get());
}
bool matches(Graphics& g, void* raw) {
    ComPtr<IDXGISwapChain> chain;
    return raw && SUCCEEDED(static_cast<IDXGISwapChain*>(raw)->QueryInterface(IID_PPV_ARGS(&chain)))
        && chain.Get() == g.chain.Get();
}
bool check(Graphics& g, HRESULT hr) {
    if (SUCCEEDED(hr)) return true;
    suspendMenu(g.window);
    g.failed = true;
    log("DX11 panel disabled, HRESULT %08lx", hr);
    return false;
}
}
bool attachSwapchain(void* raw, IUnknown* device) {
    auto& g = graphics();
    std::lock_guard guard(g.mutex);
    if (g.chain) return matches(g, raw);
    ComPtr<IDXGISwapChain> chain;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(static_cast<IDXGISwapChain*>(raw)->QueryInterface(IID_PPV_ARGS(&chain))) ||
        FAILED(chain->GetDesc(&desc)) || !desc.OutputWindow ||
        FAILED(device->QueryInterface(IID_PPV_ARGS(&g.device)))) return false;
    g.chain = chain;
    g.window = desc.OutputWindow;
    markMenuSurfaceReady();
    return true;
}
void drawOverlay(void* raw, UINT flags) {
    auto& g = graphics();
    std::lock_guard guard(g.mutex);
    if (g.failed || g.resizing || (flags & DXGI_PRESENT_TEST) || !matches(g,raw)) return;
    bool focused=true;
#ifndef CORE_GRAPHICS_TEST
    focused=!IsIconic(g.window) && GetForegroundWindow()==g.window;
#endif
    if(!focused){suspendMenu(g.window);return;}
    RECT client{};GetClientRect(g.window,&client);
    const auto viewport=g.target ? g.target->GetSize() : D2D1::SizeF(float(client.right),float(client.bottom));
    const auto& menu=updateMenu(g.window,viewport.width,viewport.height,true);
    auto placement=panelPlacement();
    const bool editingHealth=menu.open && !menu.damage && !menu.crown;
    if(editingHealth && menu.moving) {
        placement.x_percent=menu.x;placement.top_percent=menu.y;
    }
    if(editingHealth) {
        switch(menu.previewRow) {
        case 0: placement.x_percent=menu.previewValue;break;
        case 1: placement.top_percent=menu.previewValue;break;
        case 2: placement.scale_percent=menu.previewValue;break;
        case healthOpacityIndex: placement.opacity_percent=menu.previewValue;break;
        case healthBackgroundIndex: placement.background_percent=menu.previewValue;break;
        }
    }
    const auto previous = g.scene.revision;
    latest(g.scene);
    static constexpr IcebornRow example{0,"Vista previa",750,1000};
    const bool useExample=editingHealth && !g.scene.count;
    const auto count=useExample?1u:g.scene.count;
    const auto* rows=useExample?&example:g.scene.rows.data();
    const bool showHealth=editingHealth || (!menu.open && placement.visible && count);
    const auto timers=g.scene.timerCount && !menu.open?timerPlacement():Placement{};
    const bool showTimers=!menu.open && g.scene.timerCount && timers.visible;
    const bool editingCrowns=menu.open && menu.crown;
    const bool showCrowns=editingCrowns || (!menu.open && g.scene.crownCount);
    auto crowns=showCrowns?crownSettings():CrownSettings{};
    if(editingCrowns) {
        if(menu.moving) {crowns.x=menu.x;crowns.y=menu.y;}
        if(menu.previewRow==0)crowns.x=menu.previewValue;
        if(menu.previewRow==1)crowns.y=menu.previewValue;
        if(menu.previewRow==crownScaleIndex)crowns.scale=menu.previewValue;
        if(menu.previewRow==crownOpacityIndex)crowns.opacity=menu.previewValue;
        if(menu.previewRow==crownBackgroundIndex)crowns.backgroundPercent=menu.previewValue;
    }
    const auto& hud=g.scene.buffHud;
    const bool showBuffBars=!menu.open && hud.count && hud.viewportWidth==viewport.width &&
        hud.viewportHeight==viewport.height;
    if (!showHealth && !showTimers && !showCrowns && !showBuffBars && !menu.open) { g.painted = false; return; }
    if (!g.context && !check(g, initialize(g))) return;
    if (!g.target && !check(g, g.surface.createTarget(g.chain.Get(), g.context.Get(), &g.target))) return;
    const auto size=g.target->GetSize();
    IcebornPreviewArea area{};
    if(menu.open && !menu.moving) {
        const MenuLayout layout(size.width,size.height);
        area={layout.left,layout.top,layout.left+layout.width*layout.scale,layout.top+layout.height*layout.scale,-1,-1};
    } else {area.x=menu.x;area.y=menu.y;}
    if(menu.open && menu.damage && menu.previewRow>=0 && menu.previewRow<2) {
        area.x=menu.previewRow==0?menu.previewValue:menu.values[0];
        area.y=menu.previewRow==1?menu.previewValue:menu.values[1];
    }
    const bool paintedMeter=menu.open && menu.damage && drawMeterPreview(raw,area,menu.previewRow,menu.previewValue);
    if (g.painted && previous == g.scene.revision && placement==g.cachedPlacement &&
        timers==g.cachedTimers && crowns==g.cachedCrowns &&
        g.menuRevision==menu.revision) { check(g,g.surface.reuse()); return; }
    g.painted = false;
    auto hr = g.surface.begin();
    if (hr == S_FALSE) { check(g, g.surface.composite()); return; }
    if (!check(g,hr)) return;

    const float stackHeight=PanelPainter::healthHeight+PanelPainter::healthStep*(std::max(count,1u)-1);
    const float preferredScale=std::clamp(size.height/1080.f,.65f,1.8f)*placement.scale_percent/100.f;
    const float scale=std::min({preferredScale,size.width*.58f/PanelPainter::healthWidth,
                                std::max(1.f,size.height-12.f)/stackHeight});
    const float width=PanelPainter::healthWidth*scale;
    g.cachedPlacement=placement;
    g.cachedTimers=timers;
    g.cachedCrowns=crowns;
    const float left = placement.x_percent >= 0 ? (size.width-width)*placement.x_percent/100.f :
        placement.left < 0 ? (size.width - width) * .5f :
        std::clamp(placement.left * scale, 0.f, size.width - width);
    if(placement.left>=0 && placement.x_percent<0 && size.width>width)
        rememberLegacyX(int(left*100.f/(size.width-width)+.5f));
    const float height=stackHeight*scale;
    float top = std::clamp(size.height * placement.top_percent / 100.f,
                                 0.f, (std::max)(0.f, size.height-height));
    if(editingHealth && !menu.moving)top=area.displayTop(left,top,width,height);
    // D2D's clear and the DX11 scissor must cover the same pixels.
    auto bounds=menu.open?D2D1::RectF(0,0,size.width,size.height):
        D2D1::RectF(std::floor(left),std::floor(top),std::ceil(left+width),std::ceil(top+height));
    float timerLeft=0,timerTop=0,timerScale=0;
    if(showTimers) {
        const float logicalHeight=PanelPainter::timerHeight+PanelPainter::timerStep*(g.scene.timerCount-1);
        timerScale=std::min({std::clamp(size.height/1080.f,.75f,1.8f)*timers.scale_percent/100.f,
                             size.width/PanelPainter::timerWidth,size.height/logicalHeight});
        const float timerWidth=PanelPainter::timerWidth*timerScale,timerHeight=logicalHeight*timerScale;
        timerLeft=(size.width-timerWidth)*timers.x_percent/100.f;
        timerTop=std::clamp(size.height*timers.top_percent/100.f,0.f,size.height-timerHeight);
        const auto rect=D2D1::RectF(std::floor(timerLeft),std::floor(timerTop),
                                   std::ceil(timerLeft+timerWidth),std::ceil(timerTop+timerHeight));
        bounds=showHealth?D2D1::RectF(std::min(bounds.left,rect.left),std::min(bounds.top,rect.top),
                                     std::max(bounds.right,rect.right),std::max(bounds.bottom,rect.bottom)):rect;
    }
    static constexpr CrownNotice crownExample{1,ICEBORN_CROWN_LARGE_GOLD};
    const std::span<const CrownNotice> crownRows=editingCrowns?std::span(&crownExample,1):
        std::span<const CrownNotice>(g.scene.crowns.data(),g.scene.crownCount);
    float crownLeft=0,crownTop=0,crownScale=0;
    if(showCrowns) {
        const float logicalHeight=PanelPainter::crownHeight+PanelPainter::crownStep*(crownRows.size()-1);
        crownScale=std::min({std::clamp(size.height/900.f,.8f,1.6f)*crowns.scale/100.f,
                             size.width/PanelPainter::crownWidth,size.height/logicalHeight});
        const float crownWidth=PanelPainter::crownWidth*crownScale,crownHeight=logicalHeight*crownScale;
        crownLeft=(size.width-crownWidth)*crowns.x/100.f;
        const float bottomInset=10.f*crownScale;
        crownTop=std::clamp(size.height*crowns.y/100.f,0.f,std::max(0.f,size.height-crownHeight-bottomInset));
        if(editingCrowns && !menu.moving)crownTop=area.displayTop(crownLeft,crownTop,crownWidth,crownHeight);
        if(!menu.open) {
            const auto rect=D2D1::RectF(std::floor(crownLeft),std::floor(crownTop),
                                       std::ceil(crownLeft+crownWidth),std::ceil(crownTop+crownHeight));
            bounds=showHealth || showTimers?D2D1::RectF(std::min(bounds.left,rect.left),std::min(bounds.top,rect.top),
                std::max(bounds.right,rect.right),std::max(bounds.bottom,rect.bottom)):rect;
        }
    }
    if(showBuffBars) {
        bool haveBounds=showHealth || showTimers || showCrowns;
        for(uint32_t i=0;i<hud.count;++i) {
            const auto& bar=hud.rows[i];
            const auto rect=D2D1::RectF(std::floor(bar.left),std::floor(bar.top),
                std::ceil(bar.left+bar.width),std::ceil(bar.top+bar.height));
            bounds=haveBounds?D2D1::RectF(std::min(bounds.left,rect.left),std::min(bounds.top,rect.top),
                std::max(bounds.right,rect.right),std::max(bounds.bottom,rect.bottom)):rect;
            haveBounds=true;
        }
    }
    g.context->SetTarget(g.target.Get());
    g.context->BeginDraw();
    g.context->PushAxisAlignedClip(bounds,D2D1_ANTIALIAS_MODE_ALIASED);
    g.context->Clear(D2D1::ColorF(0,0.f));
    if(showHealth)g.painter.draw(g.context.Get(),{rows,count},left,top,scale,
        placement.opacity_percent/100.f,placement.background_percent/100.f);
    if(showTimers)g.painter.drawTimers(g.context.Get(),{g.scene.timers.data(),g.scene.timerCount},timerLeft,timerTop,timerScale);
    if(showCrowns)g.painter.drawCrowns(g.context.Get(),crownRows,crownLeft,crownTop,crownScale,
        crowns.opacity/100.f,crowns.backgroundPercent/100.f);
    if(showBuffBars)g.painter.drawBuffBars(g.context.Get(),{hud.rows.data(),hud.count});
    if(menu.open) g.painter.drawMenu(g.context.Get(),menu,size.width,size.height,showHealth || paintedMeter || editingCrowns);
    g.menuRevision=menu.revision;
    g.context->PopAxisAlignedClip();
    hr = g.context->EndDraw();
    g.context->SetTarget(nullptr);
    const auto composite = g.surface.end(bounds);
    g.painted = check(g,hr) && check(g,composite);
}
void beforeResize(void* raw) {
    auto& g=graphics(); std::lock_guard guard(g.mutex);
    if (!matches(g,raw)) return;
    g.resizing=true; g.target.Reset(); g.surface.releaseTarget(); g.painted=false;
}
void afterResize(void* raw) {
    auto& g=graphics(); std::lock_guard guard(g.mutex);
    if (matches(g,raw)) g.resizing=false;
}
void observePresentResult(void* raw,HRESULT hr) {
    if (hr!=DXGI_ERROR_DEVICE_REMOVED && hr!=DXGI_ERROR_DEVICE_RESET && hr!=DXGI_ERROR_DEVICE_HUNG) return;
    auto& g=graphics(); std::lock_guard guard(g.mutex);
    if(matches(g,raw)) check(g,hr);
}
}
