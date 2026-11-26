#pragma once
#include "../src/settings.hpp"
inline void verifyInGameControls(HWND window,bool hp,bool withMeter) {
    using namespace ::meter;
    testMenuOpen();
    auto tick=[&]()->const Menu& {
        serviceSettings();
        return updateMenu(window,800,600,true);
    };
    tick();
    auto& state=tick();
    assert(state.open && state.ready && state.tabCount==int(hp)+int(withMeter));
    for(int i=0;i<int(hp)+int(withMeter);++i) {
        if(i){testMenuKey(VK_TAB);tick();tick();}
        const bool damage=!hp || i!=0;
        assert(state.damage==damage);
        const int previous=state.values[2];
        testMenuKey(previous<175?VK_RIGHT:VK_LEFT);tick();tick();
        assert(state.values[2]==previous+(previous<175?5:-5));
        const MenuLayout layout(800,600);
        const float valueX=layout.left+(MenuLayout::valueLeft+42)*layout.scale;
        auto rowY=[&](int row){return layout.top+(110.f+(row%5)*40)*layout.scale;};
        const int beforeDrag=state.values[2];
        const int direction=beforeDrag<=165?1:-1;
        const float movedX=valueX+direction*20*layout.scale;
        testMenuPointer(valueX,rowY(2),true);tick();
        assert(state.draggingValue && state.previewRow==2);
        testMenuPointer(movedX,rowY(2),true);tick();tick();
        assert(state.previewValue==beforeDrag+direction*10 && state.values[2]==beforeDrag);
        testMenuPointer(movedX,rowY(2),false);tick();
        assert(!state.draggingValue && state.previewRow==2);
        tick();
        assert(state.previewRow==-1 && state.values[2]==beforeDrag+direction*10);
        testMenuPointer(valueX,rowY(2),true);tick();
        testMenuPointer(valueX-direction*20*layout.scale,rowY(2),true);tick();
        testMenuKey(VK_ESCAPE);tick();
        testMenuPointer(valueX,rowY(2),false);tick();tick();
        assert(state.open && state.previewRow==-1 && state.values[2]==beforeDrag+direction*10);

        const int toggleRow=damage?6:3;
        while(state.row<toggleRow){testMenuKey(VK_DOWN);tick();}
        const bool enabled=state.values[toggleRow]!=0;
        testMenuKey(VK_RETURN);tick();tick();
        assert((state.values[toggleRow]!=0)!=enabled);
        testMenuPointer(valueX,rowY(toggleRow),true);tick();
        testMenuPointer(valueX,rowY(toggleRow),false);tick();tick();
        assert((state.values[toggleRow]!=0)==enabled);
        if(!damage) {
            testMenuKey(VK_DOWN);tick();
            assert(state.row==healthOpacityIndex);
            const int savedOpacity=state.values[healthOpacityIndex];
            const int opacityKey=savedOpacity>10?VK_LEFT:VK_RIGHT;
            testMenuKey(opacityKey);tick();tick();
            assert(state.values[healthOpacityIndex]==savedOpacity+(opacityKey==VK_LEFT?-5:5));
            testMenuKey(opacityKey==VK_LEFT?VK_RIGHT:VK_LEFT);tick();tick();
            testMenuKey(VK_DOWN);tick();
            assert(state.row==healthBackgroundIndex);
            const int oldBackground=state.values[healthBackgroundIndex];
            const int backgroundKey=oldBackground<100?VK_RIGHT:VK_LEFT;
            testMenuKey(backgroundKey);tick();tick();
            const auto config=std::filesystem::absolute("settings-test.ini");
            const int updated=oldBackground+(backgroundKey==VK_RIGHT?1:-1);
            assert(state.values[healthBackgroundIndex]==updated);
            assert(GetPrivateProfileIntW(L"panels",L"BackgroundPercent",101,config.c_str())==updated);
            testMenuKey(backgroundKey==VK_RIGHT?VK_LEFT:VK_RIGHT);tick();tick();
        }
        if(!damage && state.provider->count>healthMissionModeIndex) {
            testMenuKey(VK_DOWN);tick();
            assert(state.row==healthMissionModeIndex && state.values[healthMissionModeIndex]==0);
            testMenuKey(VK_RETURN);tick();tick();
            assert(state.values[healthMissionModeIndex]==1);
            assert(GetPrivateProfileIntW(L"panels",L"MonsterHealthAlways",0,
                                         std::filesystem::absolute("settings-test.ini").c_str())==1);
            testMenuKey(VK_RETURN);tick();tick();
            assert(state.values[healthMissionModeIndex]==0);
        }
        testMenuKey('M');tick();assert(state.moving);
        testMenuPointer(360,240,true);tick();
        testMenuPointer(368,270,false);tick();tick();
        assert(!state.moving && state.values[0]==46 && state.values[1]==45);
        testMenuKey('M');tick();testMenuKey(VK_ESCAPE);tick();
        assert(state.open && !state.moving);
    }
    testMenuKey(VK_ESCAPE);assert(!tick().open);
    puts("PASS in-game module selection, numeric drag/commit/cancel, toggles and HUD placement");
}

