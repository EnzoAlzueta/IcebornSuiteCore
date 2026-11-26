#pragma once
#include "../sdk/1.0.0-dev.1/iceborn_panel.h"
#include "../sdk/timer-dev.1/iceborn_timer.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <span>
#include "settings.hpp"
#include "crowns.hpp"

class PanelPainter {
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_, detail_, heading_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> healthTitle_, healthValues_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
public:
    static constexpr float healthWidth=384.f, healthHeight=36.f, healthStep=44.f;
    static constexpr float timerWidth=255.f,timerHeight=29.f,timerStep=34.f;
    static constexpr float crownWidth=250.f,crownHeight=30.f,crownStep=34.f;
    HRESULT initialize(ID2D1RenderTarget* target);
    void drawMenu(ID2D1RenderTarget*,const meter::Menu&,float w,float h,bool realHealth) const;
    // Caller owns BeginDraw/EndDraw so all modules share a pass.
    void draw(ID2D1RenderTarget* target, std::span<const IcebornRow> rows,
              float left, float top, float scale,float opacity,float backgroundOpacity) const;
    void drawTimers(ID2D1RenderTarget*,std::span<const IcebornTimerRow>,float left,float top,float scale) const;
    void drawCrowns(ID2D1RenderTarget*,std::span<const meter::CrownNotice>,float left,float top,
                    float scale,float opacity,float backgroundOpacity) const;
    void drawBuffBars(ID2D1RenderTarget*,std::span<const IcebornHudBar>) const;
};
