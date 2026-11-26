#pragma once
#include <windows.h>
#include <unknwn.h>
#include <atomic>
#include <mutex>
#include <array>
#include <cstdint>
#include "../sdk/1.0.0-dev.1/iceborn_panel.h"
#include "../sdk/timer-dev.1/iceborn_timer.h"
#include "../sdk/crown-dev.1/iceborn_crown.h"
#include "../sdk/hud-timer-dev.1/iceborn_hud_timer.h"

namespace meter {
extern std::atomic<bool> stopping;
void log(const char*, ...);
bool installHooks();
void removeHooks();
bool attachSwapchain(void*, IUnknown*);
void drawOverlay(void*, UINT);
void observePresentResult(void*, HRESULT);
void beforeResize(void*);
void afterResize(void*);
inline constexpr uint32_t maxCrownNotices=5;
struct CrownNotice {
    int32_t species{};
    uint32_t kind{};
    bool operator==(const CrownNotice&) const = default;
};
struct BuffHudBars {
    float viewportWidth{}, viewportHeight{};
    uint32_t count{};
    std::array<IcebornHudBar,ICEBORN_MAX_HUD_TIMERS> rows{};
};
struct Scene {
    uint64_t revision{};
    uint32_t count{};
    std::array<IcebornRow, ICEBORN_MAX_ROWS> rows{};
    uint32_t timerCount{};
    std::array<IcebornTimerRow, ICEBORN_MAX_TIMERS> timers{};
    uint32_t crownCount{};
    std::array<CrownNotice,maxCrownNotices> crowns{};
    BuffHudBars buffHud;
};
struct Placement {
    int left=-1, top_percent=2, scale_percent=100, x_percent=-1;
    bool visible=true;
    int opacity_percent=100;
    int background_percent=84;
    bool operator==(const Placement&) const = default;
};
Placement panelPlacement();
void setPanelPlacement(const Placement&);
Placement timerPlacement();
void setTimerPlacement(const Placement&);
void rememberLegacyX(int percent);
bool hookSlot(void**,void*);
void* originalHook(void*,size_t);
bool installInput(HWND);
void setEditing(bool);
bool getEditPointer(POINT&,bool&);
void latest(Scene& current);
}
