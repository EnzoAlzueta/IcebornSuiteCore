#pragma once
#include <filesystem>
#include <windows.h>
#include <array>
#include "../sdk/preview-dev.1/iceborn_preview.hpp"
#include "../sdk/settings-dev.1/iceborn_settings.h"
namespace meter {
void initializeSettings(const std::filesystem::path&,bool,bool crowns=false);
int loadBackgroundPercent(const std::filesystem::path&,const wchar_t*,int defaultPercent,int panelOpacity);
void markMenuSurfaceReady();
void findDamageSettings();
void serviceSettings();
struct Menu {
    bool open=false, moving=false, ready=false, rejected=false;
    bool damage=false, crown=false;
    const IcebornSettings* provider=nullptr;
    std::array<const IcebornSettings*,3> tabs{};
    int tabCount=0;
    std::array<int32_t,64> values{};
    int row=2,x=50,y=2;
    int previewRow=-1,previewValue=0;
    bool draggingValue=false;
    float cursorX=0,cursorY=0;
    unsigned revision=0;
};
struct MenuLayout {
    static constexpr int width=540,height=370,rows=5;
    static constexpr float valueLeft=369,valueRight=514;
    static float tabLeft(int index,int count) { return 18.f + 508.f*index/count; }
    float scale,left,top;
    explicit MenuLayout(float w,float h);
};
inline bool isToggle(const IcebornSetting& option) {
    return option.minimum==0 && option.maximum==1;
}
inline constexpr int healthOpacityIndex=4, healthBackgroundIndex=5, healthMissionModeIndex=6;
inline constexpr int crownScaleIndex=2, crownFixedIndex=3, crownDurationIndex=4, crownOpacityIndex=5,
    crownBackgroundIndex=6;
inline bool optionLocked(const Menu& menu,int index) {
    return menu.ready && menu.crown && index==crownDurationIndex && menu.values[crownFixedIndex]!=0;
}
const Menu& updateMenu(HWND,float,float,bool focused);
bool drawMeterPreview(void*,const IcebornPreviewArea&,int row,int value);
void suspendMenu(HWND);
#ifdef CORE_GRAPHICS_TEST
void testMenuOpen();
void testMenuPointer(float,float,bool);
void testMenuKey(int vk);
#endif
}
