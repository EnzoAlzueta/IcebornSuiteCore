#include "panel_painter.hpp"
#include <cmath>
#include <algorithm>
#include <cwchar>
#include <cstring>

using Microsoft::WRL::ComPtr;
HRESULT PanelPainter::initialize(ID2D1RenderTarget* target) {
    ComPtr<IDWriteFactory> factory;
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                      reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
    if (FAILED(hr)) return hr;
    hr = factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16, L"en-us", &title_);
    if (FAILED(hr)) return hr;
    hr = factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13, L"en-us", &detail_);
    if (FAILED(hr)) return hr;
    hr = factory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,21,L"es-ar",&heading_);
    if(FAILED(hr))return hr;
    hr = factory->CreateTextFormat(L"Georgia",nullptr,DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,17,L"en-us",&healthTitle_);
    if(FAILED(hr))return hr;
    hr = healthTitle_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if(FAILED(hr))return hr;
    hr = factory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,13,L"en-us",&healthValues_);
    if(FAILED(hr))return hr;
    hr = healthValues_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    if(FAILED(hr))return hr;
    hr = healthValues_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if(FAILED(hr))return hr;
    return target->CreateSolidColorBrush(D2D1::ColorF(0xffffff), &brush_);
}
void PanelPainter::draw(ID2D1RenderTarget* target, std::span<const IcebornRow> rows,
                        float left, float top, float scale,float opacity,float backgroundOpacity) const {
    if (!brush_ || !std::isfinite(scale) || scale <= 0) return;
    D2D1_MATRIX_3X2_F original;
    target->GetTransform(&original);
    target->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
        D2D1::Matrix3x2F::Translation(left, top) * original);
    const auto box=[&](float x,float y,float right,float bottom,UINT32 color,float alpha=1.f) {
        brush_->SetColor(D2D1::ColorF(color,alpha));
        target->FillRectangle(D2D1::RectF(x,y,right,bottom),brush_.Get());
    };
    for (size_t i=0;i<rows.size();++i) {
        const auto& r=rows[i];
        if (!std::isfinite(r.current) || !std::isfinite(r.maximum) ||
            r.current <= 0 || r.current > r.maximum) continue;
        const auto len = strnlen_s(r.label, sizeof(r.label));
        if (!len || len == sizeof(r.label)) continue;
        wchar_t name[64]{}, values[96]{};
        const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, r.label,
            static_cast<int>(len), name, 64);
        if (!chars) continue;
        const double ratio = static_cast<double>(r.current) / r.maximum;
        const int n = swprintf_s(values, L"%g / %g (%.2f%%)", r.current, r.maximum, ratio * 100);
        const float y=healthStep*static_cast<float>(i);
        if(backgroundOpacity>0.f) {
            brush_->SetOpacity(1.f);
            box(0,y,healthWidth,y+healthHeight,0x000000,backgroundOpacity);
        }
        brush_->SetOpacity(opacity);
        box(0,y+2,2,y+21,0xa68a61);
        brush_->SetColor(D2D1::ColorF(0xf1e6cf));
        target->DrawText(name,chars,healthTitle_.Get(),D2D1::RectF(11,y+1,201,y+24),brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        if(n>0) {
            brush_->SetColor(D2D1::ColorF(0xcfbb9c));
            target->DrawText(values,n,healthValues_.Get(),D2D1::RectF(205,y+4,healthWidth-10,y+24),
                             brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        const float fillEnd=9+(healthWidth-18)*static_cast<float>(ratio);
        box(7,y+24,healthWidth-7,y+35,0x8c7253);
        box(9,y+25,healthWidth-9,y+34,0x291e1b);
        box(9,y+25,fillEnd,y+34,0x9f302b);
        box(9,y+25,fillEnd,y+27,0xcf6754);
    }
    target->SetTransform(original);
    brush_->SetOpacity(1.f);
}
void PanelPainter::drawTimers(ID2D1RenderTarget* target,std::span<const IcebornTimerRow> rows,
                              float left,float top,float scale) const {
    D2D1_MATRIX_3X2_F saved;
    target->GetTransform(&saved);
    target->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale)*
                         D2D1::Matrix3x2F::Translation(left,top)*saved);
    for(size_t i=0;i<rows.size();++i) {
        const auto& row=rows[i];
        const wchar_t* label=nullptr;
        switch(row.effect) {
        case ICEBORN_MIGHT_SEED:label=L"Semilla de poder";break;
        case ICEBORN_ROCKSTEADY:label=L"Manto de firmeza";break;
        case ICEBORN_TEMPORAL:label=L"Manto temporal";break;
        }
        if(!label || (row.state!=ICEBORN_TIMER_ACTIVE && row.state!=ICEBORN_TIMER_RECHARGING) ||
           row.seconds>3600 || (!row.seconds && row.state!=ICEBORN_TIMER_ACTIVE))continue;
        const bool recharge=row.state==ICEBORN_TIMER_RECHARGING;
        const float y=timerStep*static_cast<float>(i);
        brush_->SetColor(D2D1::ColorF(0x151917,.94f));
        target->FillRectangle(D2D1::RectF(0,y,timerWidth,y+timerHeight),brush_.Get());
        brush_->SetColor(D2D1::ColorF(recharge?0xc3a675:0x83c5a2));
        target->FillRectangle(D2D1::RectF(0,y,3,y+timerHeight),brush_.Get());
        wchar_t value[24]{};
        if(row.seconds)swprintf_s(value,recharge?L"Recarga %u:%02u":L"%u:%02u",row.seconds/60,row.seconds%60);
        else wcscpy_s(value,L"Activo");
        target->DrawText(value,static_cast<UINT32>(wcslen(value)),healthValues_.Get(),
            D2D1::RectF(140,y+5,timerWidth-9,y+26),brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        brush_->SetColor(D2D1::ColorF(0xf2ebd9));
        target->DrawText(label,static_cast<UINT32>(wcslen(label)),detail_.Get(),
            D2D1::RectF(10,y+5,142,y+26),brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    target->SetTransform(saved);
}
void PanelPainter::drawBuffBars(ID2D1RenderTarget* target,std::span<const IcebornHudBar> bars) const {
    for(const auto& bar:bars) {
        const float bottom=bar.top+bar.height;
        brush_->SetColor(D2D1::ColorF(0x1b171a,.92f));
        target->FillRectangle(D2D1::RectF(bar.left,bar.top,bar.left+bar.width,bottom),brush_.Get());
        brush_->SetColor(D2D1::ColorF(bar.expiring?0xffb05c:0xd1ddaa));
        target->FillRectangle(D2D1::RectF(bar.left,bar.top,bar.left+bar.fill,bottom),brush_.Get());
    }
}
void PanelPainter::drawCrowns(ID2D1RenderTarget* target,std::span<const meter::CrownNotice> rows,
                              float left,float top,float scale,float opacity,float backgroundOpacity) const {
    using namespace D2D1;
    D2D1_MATRIX_3X2_F saved;
    target->GetTransform(&saved);
    target->SetTransform(Matrix3x2F::Scale(scale,scale)*Matrix3x2F::Translation(left,top)*saved);
    for(size_t i=0;i<rows.size();++i) {
        const auto& row=rows[i];
        const auto* name=meter::crownMonsterName(row.species);
        if(!name)continue;
        const bool mini=row.kind==ICEBORN_CROWN_MINI_GOLD;
        const bool silver=row.kind==ICEBORN_CROWN_LARGE_SILVER;
        const float y=crownStep*static_cast<float>(i);
        if(backgroundOpacity>0.f) {
            brush_->SetOpacity(1.f);
            brush_->SetColor(ColorF(0x000000,backgroundOpacity));
            target->FillRectangle(RectF(0,y,crownWidth,y+crownHeight),brush_.Get());
        }
        brush_->SetOpacity(opacity);
        brush_->SetColor(ColorF(0xe3be6b));
        target->FillRectangle(RectF(0,y,2,y+crownHeight),brush_.Get());
        target->DrawRectangle(RectF(.5f,y+.5f,crownWidth-.5f,y+crownHeight-.5f),brush_.Get(),1.f);
        if(silver)brush_->SetColor(ColorF(0xc1cad0));
        const float inset=mini?3.f:0.f;
        const float side=y+(mini?12.f:9.f);
        const float valley=y+(mini?16.f:15.f);
        const float peak=y+(mini?9.f:6.f);
        const D2D1_POINT_2F shape[]{Point2F(10+inset,side),Point2F(16,y+21),Point2F(30,y+21),
            Point2F(36-inset,side),Point2F(28,valley),Point2F(23,peak),
            Point2F(18,valley),Point2F(10+inset,side)};
        for(size_t point=1;point<std::size(shape);++point)
            target->DrawLine(shape[point-1],shape[point],brush_.Get(),1.7f);
        target->FillRectangle(RectF(16,y+23,30,y+24.5f),brush_.Get());
        brush_->SetColor(ColorF(0xf2e7ce));
        target->DrawText(name,static_cast<UINT32>(wcslen(name)),healthTitle_.Get(),
            RectF(44,y+4,crownWidth-7,y+crownHeight),brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    target->SetTransform(saved);
    brush_->SetOpacity(1.f);
}
void PanelPainter::drawMenu(ID2D1RenderTarget* t,const meter::Menu& e,float w,float h,bool realPreview) const {
    using namespace D2D1;
    const meter::MenuLayout l(w,h);
    D2D1_MATRIX_3X2_F saved;t->GetTransform(&saved);
    auto box=[&](float x,float y,float r,float b,UINT32 color,float alpha=1.f) {
        brush_->SetColor(ColorF(color,alpha));t->FillRectangle(RectF(x,y,r,b),brush_.Get());
    };
    auto label=[&](float x,float y,float right,float bottom,const wchar_t* str,UINT32 color,IDWriteTextFormat* font){
        brush_->SetColor(ColorF(color));
        t->DrawText(str,UINT32(wcslen(str)),font,RectF(x,y,right,bottom),brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    };
    auto frame=[&](float x,float y,float r,float b,UINT32 color,float alpha=1.f) {
        brush_->SetColor(ColorF(color,alpha));t->DrawRectangle(RectF(x,y,r,b),brush_.Get(),1.f);
    };
    constexpr unsigned brass=0xb49b64, gold=0xe0c78e, cream=0xf2ebd9, muted=0xb4b7ac;
    constexpr unsigned shell=0x141918, rule=0x534c39, teal=0x94c5ba;
    if(e.moving) {
        const float x=e.x*w/100.f,y=e.y*h/100.f;
        const float card=std::min(w,214*l.scale),ht=40*l.scale;
        const float left=std::clamp(x,0.f,w-card),top=std::clamp(y,0.f,h-ht);
        t->SetTransform(Matrix3x2F::Scale(l.scale,l.scale)*Matrix3x2F::Translation(left,top));
        if(!realPreview){
        box(0,0,214,40,shell,.96f);frame(.5f,.5f,213.5f,39.5f,brass);
        label(12,5,203,28,e.provider->title,cream,title_.Get());box(12,31,202,33,e.damage?gold:teal);
        }
        t->SetTransform(Matrix3x2F::Scale(l.scale,l.scale)*Matrix3x2F::Translation(l.left,h-54*l.scale));
        box(0,0,540,38,shell,.96f);frame(.5f,.5f,539.5f,37.5f,brass);
        label(12,9,531,34,L"Colocar HUD   ·   Clic o Enter: guardar   ·   Flechas: afinar   ·   Esc: cancelar",cream,detail_.Get());
    } else {
        t->SetTransform(Matrix3x2F::Scale(l.scale,l.scale)*Matrix3x2F::Translation(l.left,l.top));
        box(3,4,543,374,0x050706,.64f);
        box(0,0,540,370,shell,.97f);
        box(2,2,538,47,0x242622);
        frame(.5f,.5f,539.5f,369.5f,brass);
        box(0,0,540,1,brass);
        // Brass corner fittings give the frame definition without a wide accent band.
        for(float x:{3.f,521.f}) {
            box(x,3,x+16,5,gold);box(x,365,x+16,367,gold);
        }
        box(18,46,522,47,rule);
        label(20,8,306,43,L"Ajustes",cream,heading_.Get());
        label(395,17,484,40,L"Iceborn Suite",gold,detail_.Get());
        box(493,11,522,36,0x1a1e1a);
        frame(493.5f,11.5f,521.5f,35.5f,rule);
        label(503,12,521,38,L"×",gold,title_.Get());
        auto tab=[&](float left,float end,const wchar_t* name,bool active){
            box(left,52,end,85,active?0x39362a:0x202522);
            frame(left+.5f,52.5f,end-.5f,84.5f,active?brass:rule);
            if(active)box(left+1,82,end-1,84,gold);
            label(left+10,58,end-3,82,name,active?cream:muted,e.tabCount>2?detail_.Get():title_.Get());
        };
        for(int i=0;i<e.tabCount;++i)
            tab(l.tabLeft(i,e.tabCount),l.tabLeft(i+1,e.tabCount)-4,e.tabs[i]->title,e.tabs[i]==e.provider);
        const int start=e.row/5*5;
        for(int i=start;i<std::min(int(e.provider->count),start+5);++i) {
            const float y=94.f+(i-start)*40;
            const auto& option=e.provider->options[i];
            const bool selected=e.row==i;
            const bool locked=meter::optionLocked(e,i);
            box(18,y,522,y+36,selected?0x2f403c:0x1e2522);
            if(selected) {
                frame(18.5f,y+.5f,521.5f,y+35.5f,teal,.62f);
                box(19,y+1,22,y+35,teal);
            }
            label(31,y+8,357,y+34,option.label,locked?muted:cream,title_.Get());
            const auto left=l.valueLeft, right=l.valueRight;
            const bool editing=e.draggingValue && e.previewRow==i;
            const auto current=e.previewRow==i?e.previewValue:e.values[i];
            box(left,y+4,right,y+32,editing?0x243430:0x141a18);
            frame(left+.5f,y+4.5f,right-.5f,y+31.5f,editing?teal:selected?brass:rule);
            wchar_t value[24]{};
            float textRight=right-41;
            if(!e.ready)wcscpy_s(value,L"…");
            else if(locked) {
                wcscpy_s(value,L"Modo fijo");
                textRight=right-8;
            }
            else if(meter::isToggle(option)) {
                wcscpy_s(value,current?L"Activado":L"Desactivado");
                box(right-39,y+11,right-9,y+26,current?0x35534b:0x272d28);
                frame(right-38.5f,y+11.5f,right-9.5f,y+25.5f,current?teal:rule);
                const float thumb=current?right-22:right-36;
                box(thumb,y+14,thumb+10,y+23,current?teal:muted);
            } else {
                swprintf_s(value,L"%d",current);
                const float start=right-67,end=right-11;
                const float range=std::max(1.f,float(option.maximum)-float(option.minimum));
                const float fraction=std::clamp((float(current)-float(option.minimum))/range,0.f,1.f);
                const float knob=start+(end-start)*fraction;
                box(start,y+17,end,y+21,0x53615a);
                box(start,y+17,knob,y+21,selected?teal:brass);
                box(knob-2.5f,y+13,knob+2.5f,y+25,selected?cream:gold);
                textRight=right-72;
            }
            label(left+10,y+8,textRight,y+31,value,locked?muted:gold,title_.Get());
        }
        box(19,300,210,332,0x333127);
        frame(19.5f,300.5f,209.5f,331.5f,brass);
        label(31,306,210,331,L"Mover HUD   [M]",cream,title_.Get());
        if(e.provider->count>5) {
            wchar_t page[20];swprintf_s(page,L"%d / %d",1+start/5,(int(e.provider->count)+4)/5);
            label(359,307,418,331,page,muted,detail_.Get());
            label(433,304,466,332,L"‹",gold,heading_.Get());label(487,304,515,332,L"›",gold,heading_.Get());
        }
        box(18,340,522,341,rule);
        const wchar_t* hint=meter::optionLocked(e,e.row)?L"Desactivá Dejar lista fija para cambiar la duración":
            meter::isToggle(e.provider->options[e.row])?
            L"Clic o Enter: alternar    ·    Tab: mod    ·    ↑↓: fila    ·    Esc: cerrar":
            L"Arrastrar el valor o ← → para ajustar    ·    Tab: mod    ·    ↑↓: fila    ·    Esc: cerrar";
        label(20,347,532,369,e.rejected?L"El ajuste no se pudo aplicar":hint,e.rejected?gold:muted,detail_.Get());
    }
    t->SetTransform(Matrix3x2F::Scale(l.scale,l.scale)*Matrix3x2F::Translation(e.cursorX,e.cursorY));
    brush_->SetColor(ColorF(0x121610));t->DrawLine(Point2F(-6,0),Point2F(6,0),brush_.Get(),4);
    t->DrawLine(Point2F(0,-6),Point2F(0,6),brush_.Get(),4);
    brush_->SetColor(ColorF(cream));t->DrawLine(Point2F(-6,0),Point2F(6,0),brush_.Get(),1.5f);
    t->DrawLine(Point2F(0,-6),Point2F(0,6),brush_.Get(),1.5f);t->SetTransform(saved);
}

