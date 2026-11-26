#include "../src/sampling.hpp"
#include "../src/crowns.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>
#include <fstream>
#include "settings_fixture.hpp"
using Microsoft::WRL::ComPtr;
using namespace meter;
namespace meter {
std::atomic<bool> stopping{};
void log(const char* fmt,...) {
    va_list args;va_start(args,fmt);vprintf(fmt,args);va_end(args);puts("");
}
}
#include "fixture_input.hpp"
#include "perf_fixture.hpp"
int wmain(int argc,wchar_t** argv) {
    if(argc==3 && std::wstring_view(argv[1])==L"--bench")return runBenchmark(argv[2]);
    if(argc<2)return 1;
    const bool chained=argc>=4;
    if(chained && std::wstring_view(argv[2])==L"meter-first")loadFixtureMeter(argv[3]);
    Modules mods{};
    const auto file=std::filesystem::absolute(argv[1]);
    assert(!loadPanel(file.parent_path()/L"missing.dll","missing",mods));
    assert(!loadPanel(file,"wrong-id",mods));
    assert(loadPanel(file,"monster-info",mods) && loadPanel(file,"monster-info",mods) && !mods[1]);
    Input input;
    const uint64_t base=0x140000000,manager=0x20000000,c=0x30000000,kulu=0xfa4f0080,tzitzi=0xfa510fa0;
    input.set(base+0x0500cf40,manager);
    std::array<uint64_t,128> slots{c,c+0x10000};
    input.set(manager+0x38,slots);
    input.monster(c,kulu,27,752.5f,821.5f);
    input.monster(c+0x10000,tzitzi,28,2093.5f,2093.5f);
    constexpr uint64_t zone=0x40000000;
    input.set(base+0x0500eca0,zone);
    input.set(zone+0xaed0,101);
    IcebornFrame frame{sizeof(frame),421810,66102,2,1,base,&input,readMem};
    assert(monsterHealthModeAvailable() && !monsterHealthAlways());
    assert(samplePanels(mods,frame).count==0);
    input.monster(c,kulu,27,700.f,821.5f);
    assert(samplePanels(mods,frame).count==1);
    setMonsterHealthAlways(true);
    input.monster(c,kulu,27,752.5f,821.5f);
    auto scene=samplePanels(mods,frame);
    assert(scene.count==2 && scene.rows[0].current==752.5f && scene.rows[1].maximum==2093.5f);
    assert(!strcmp(scene.rows[0].label,"Kulu-Ya-Ku") && !strcmp(scene.rows[1].label,"Tzitzi-Ya-Ku"));
    // Two consumers receive the same quest sample. Removing either is safe.
    static const IcebornPanelModule other{sizeof(IcebornPanelModule),ICEBORN_PANEL_ABI,"fixture",200,
        [](const IcebornFrame* f,IcebornRow* out,uint32_t cap)->uint32_t {
            if(!cap || f->quest!=66102) return 0;
            out[0]={7,"Fixture",1,2}; return 1;
        }};
    mods[1]=&other;
    assert(samplePanels(mods,frame).count==3);
    input.set(zone+0xaed0,109);
    auto guiding=frame;
    guiding.quest=-1;guiding.state=0;guiding.generation=2;
    assert(samplePanels(mods,guiding).count==0);
    input.monster(c,kulu,27,700.f,821.5f);
    const auto engaged=samplePanels(mods,guiding);
    assert(engaged.count==1 && engaged.rows[0].instance==kulu && engaged.rows[0].current==700.f);
    input.set(zone+0xaed0,101);
    assert(samplePanels(mods,guiding).count==0);
    input.monster(c,kulu,27,650.f,821.5f);
    assert(samplePanels(mods,guiding).count==1);
    input.set(zone+0xaed0,305);
    assert(samplePanels(mods,guiding).count==0);
    const auto* timers=argc>=6?loadTimers(std::filesystem::absolute(argv[5])):nullptr;
    Scene timerScene;
    if(argc>=6) {
        assert(timers && !loadTimers(file));
        constexpr uint64_t root=0x45000000,actor=0x46000000,effects=0x47000000;
        constexpr uint64_t weapon=0x49000000+0x2e8,equipment=0x4c000000+0x460;
        input.set(base+0x050139a0,root);input.set(root+0x50,actor);
        input.set(actor+0x7d20,effects);
        struct Attack {float remaining; int32_t variant;};
        input.set(effects+0x6a0,Attack{179.7f,10});
        input.set(actor+0xc0,uint64_t{0x48000000});
        input.set(0x48000008,uint64_t{0x48100000});input.set(0x48100078,uint64_t{0x49000000});
        input.set(weapon+0x34,std::array<int32_t,2>{3,1});
        input.set(actor+0x80,uint64_t{0x4a000000});
        input.set(0x4a000080,uint64_t{0x4b000000});input.set(0x4b000018,uint64_t{0x4c000000});
        std::array<float,40> active{},cooldown{};
        active[1]=120.f;active[21]=120.f;
        cooldown[1]=300.f;cooldown[21]=300.f;
        cooldown[3]=117.2f;cooldown[23]=360.f;
        input.set(equipment+0x99c,cooldown);input.set(equipment+0xa8c,active);
        assert(samplePanels({},guiding,timers).timerCount==0); // Hub.
        input.set(zone+0xaed0,109);
        timerScene=samplePanels({},guiding,timers);
        assert(!timerScene.count && timerScene.timerCount==3);
        assert(timerScene.timers[0].effect==ICEBORN_ROCKSTEADY && timerScene.timers[0].seconds==118);
        assert(timerScene.timers[0].state==ICEBORN_TIMER_RECHARGING);
        assert(timerScene.timers[1].state==ICEBORN_TIMER_ACTIVE && timerScene.timers[1].seconds==120);
        assert(timerScene.timers[2].effect==ICEBORN_MIGHT_SEED && timerScene.timers[2].seconds==180);
        active[3]=83.2f;active[23]=90.f;cooldown[3]=360.f;
        input.set(equipment+0x99c,cooldown);input.set(equipment+0xa8c,active);
        input.set(effects+0x6a0,Attack{0,10});
        auto expired=samplePanels({},guiding,timers);
        assert(expired.timerCount==2 && expired.timers[0].seconds==84 &&
               expired.timers[0].state==ICEBORN_TIMER_ACTIVE);
        input.set(effects+0x6a0,Attack{15,25});
        assert(samplePanels({},guiding,timers).timerCount==2); // Might Pill shares the field.
    }
    input.monster(c,kulu,27,752.5f,821.5f);
    input.set(zone+0xaed0,101);
    mods[0]=nullptr;
    assert(samplePanels(mods,frame).count==1);
    setMonsterHealthAlways(false);
    publish(scene);
    HWND win=CreateWindowExW(0,L"STATIC",L"Core panel fixture",WS_OVERLAPPEDWINDOW,0,0,800,600,
                               nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(win);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=800;desc.BufferDesc.Height=600;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferCount=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect=DXGI_SWAP_EFFECT_SEQUENTIAL;desc.OutputWindow=win;desc.Windowed=TRUE;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> ctx;ComPtr<IDXGISwapChain> chain;
    require(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &desc,&chain,&device,nullptr,&ctx));
    assert(installHooks());
    if(chained && std::wstring_view(argv[2])==L"core-first")loadFixtureMeter(argv[3]);
    const bool hp=argc<=4 || std::wstring_view(argv[4])!=L"meter-only";
    initializeSettings(std::filesystem::absolute("settings-test.ini"),hp);
    BOOL originalFullscreen{};require(chain->GetFullscreenState(&originalFullscreen,nullptr));
    int previousWidth=0;
    for(int stage=0;stage!=(timers?16:13);++stage) {
        if(stage==1) {
            require(chain->ResizeBuffers(1,800,600,DXGI_FORMAT_UNKNOWN,0));
        }
        if(stage==2) {
            auto four=scene;
            four.rows[2]=scene.rows[0]; four.rows[3]=scene.rows[1]; four.count=4;
            publish(four);
        }
        if(stage==3) {auto single=scene; single.count=1; publish(single);}
        if(stage==4) {frame.state=4;publish(samplePanels(mods,frame));}
        if(stage==5) {
            publish(scene);Placement moved{};moved.x_percent=0;moved.top_percent=20;
            moved.background_percent=100;
            setPanelPlacement(moved);
        }
        if(stage==6 || stage==7) {
            auto moved=panelPlacement();moved.visible=stage==7;
            if(stage==7)moved.background_percent=0;
            setPanelPlacement(moved);
        }
        if(stage==8){publish({});findDamageSettings();testMenuOpen();}
        if(stage==9 || stage==11)testMenuKey(VK_RIGHT);
        if(stage==10 && hp && chained)testMenuKey(VK_TAB);
        if(stage==12)testMenuKey(VK_ESCAPE);
        if(stage==13) {setPanelPlacement(Placement{});publish(timerScene);}
        if(stage==14) {auto combined=scene;combined.timerCount=timerScene.timerCount;combined.timers=timerScene.timers;publish(combined);}
        if(stage==15)publish({});
        ComPtr<ID3D11Texture2D> back;require(chain->GetBuffer(0,IID_PPV_ARGS(&back)));
        ComPtr<ID3D11RenderTargetView> view;require(device->CreateRenderTargetView(back.Get(),nullptr,&view));
        const float gray=stage>=5 && stage<8?.25f:0.f;
        const float clear[4]={gray,gray,gray,1};ctx->ClearRenderTargetView(view.Get(),clear);
        for(int i=0;i<8;++i) {serviceSettings();require(chain->Present(0,0));Sleep(15);}
        D3D11_TEXTURE2D_DESC tex{};back->GetDesc(&tex);tex.Usage=D3D11_USAGE_STAGING;
        tex.CPUAccessFlags=D3D11_CPU_ACCESS_READ;tex.BindFlags=0;tex.MiscFlags=0;
        ComPtr<ID3D11Texture2D> copy;require(device->CreateTexture2D(&tex,nullptr,&copy));
        ctx->CopyResource(copy.Get(),back.Get());
        D3D11_MAPPED_SUBRESOURCE map{};require(ctx->Map(copy.Get(),0,D3D11_MAP_READ,0,&map));
        auto crimson=[&](int x,int y){auto p=static_cast<const BYTE*>(map.pData)+y*map.RowPitch+4*x;
            return p[0]>120 && p[1]<110 && p[2]<105 && p[0]>p[1]*1.6;};
        assert(!crimson(100,235));
        if(stage<=1) {
            assert(crimson(300,32) && !crimson(515,32)); // Kulu's empty tail.
            assert(crimson(300,61) && crimson(515,61)); // Full Tzitzi below Kulu.
        } else if(stage==2) {
            for(int y:{32,61,89,118})assert(crimson(300,y) && !crimson(200,y) && !crimson(550,y));
        } else if(stage<=4) {
            assert(crimson(300,32)==(stage==3) && !crimson(300,61));
        }
        if(stage>=5 && stage<8) {
            assert(crimson(25,140)==(stage!=6) && crimson(25,168)==(stage!=6));
            assert(!crimson(300,32));
            const auto* background=static_cast<const BYTE*>(map.pData)+122*map.RowPitch+145*4;
            assert(background[0]==(stage==5?0:64));
        }
        if(stage>=8 && stage<=12) {
            const auto top=MenuLayout(800,600).top;
            const auto pixel=static_cast<const BYTE*>(map.pData)+int(top+0.5f)*map.RowPitch+400*4;
            assert((pixel[0]>90 && pixel[1]>80 && pixel[0]>pixel[2])==(stage!=12));
            if(stage==8 && hp) {
                const MenuLayout menu(800,600);
                const int x=int(menu.left+496*menu.scale);
                const int y=int(menu.top+193*menu.scale);
                const auto* track=static_cast<const BYTE*>(map.pData)+y*map.RowPitch+x*4;
                assert(track[1]>80 && track[1]>track[0] && track[2]>70);
            }
            int first=800,last=-1;
            for(int y=8;y<int(top)-2;++y)for(int x=0;x<800;++x){
                auto rgb=static_cast<const BYTE*>(map.pData)+y*map.RowPitch+x*4;
                if(std::max({rgb[0],rgb[1],rgb[2]})>14){first=std::min(first,x);last=std::max(last,x);}
            }
            const int width=last<first?0:1+last-first;
            if(stage==12)assert(width==0);
            else {
                assert(width>100);
                if(stage==9 || stage==11)assert(width>previousWidth);
            }
            previousWidth=width;
            BOOL full{};require(chain->GetFullscreenState(&full,nullptr));assert(full==originalFullscreen);
        }
        if(stage>=13) {
            const auto* pixel=static_cast<const BYTE*>(map.pData)+105*map.RowPitch+597*4;
            assert((pixel[1]>pixel[0] && pixel[1]>150)==(stage!=15));
            assert(crimson(300,32)==(stage==14));
        }
        if(stage==0 || stage==8 || stage==10 || stage==14) {
            std::ofstream image(stage==14?"buff-trackers.ppm":stage==10?"settings-second.ppm":stage==8?"settings-ingame.ppm":"kulu-render.ppm",std::ios::binary);
            image<<"P6\n800 600\n255\n";
            for(unsigned y=0;y<600;++y) for(unsigned x=0;x<800;++x)
                image.write(static_cast<const char*>(map.pData)+y*map.RowPitch+4*x,3);
        }
        ctx->Unmap(copy.Get(),0);
    }
    if(argc>4 || !chained)
        verifyInGameControls(win,hp,chained);
    if(argc==3) {
        const auto* crownModule=loadCrowns(std::filesystem::absolute(argv[2]));
        assert(crownModule && !loadCrowns(file));
        input.set(kulu+0x184,.88f);input.set(kulu+0x7730,1.f);
        input.set(tzitzi+0x184,1.17f);input.set(tzitzi+0x7730,1.f);
        std::array<IcebornCrownRow,ICEBORN_MAX_CROWNS> rows{};
        const auto count=crownModule->sample(&guiding,rows.data(),static_cast<uint32_t>(rows.size()));
        assert(count==2 && rows[0].kind==ICEBORN_CROWN_MINI_GOLD && rows[1].kind==ICEBORN_CROWN_LARGE_SILVER);
        CrownFeed feed;
        Scene notice;
        feed.observe(guiding,{rows.data(),2},1000);
        feed.show(notice,CrownSettings{},1000);
        assert(notice.crownCount==1 && notice.crowns[0].species==27);
        auto preview=notice;
        preview.crowns[preview.crownCount++]={7,ICEBORN_CROWN_LARGE_SILVER};
        publish(preview);
        // With no health panel or timer panel active the crown must still composite.
        ComPtr<ID3D11Texture2D> back;require(chain->GetBuffer(0,IID_PPV_ARGS(&back)));
        ComPtr<ID3D11RenderTargetView> view;require(device->CreateRenderTargetView(back.Get(),nullptr,&view));
        const float clear[4]={.25f,.25f,.25f,1};ctx->ClearRenderTargetView(view.Get(),clear);
        require(chain->Present(0,0));
        D3D11_TEXTURE2D_DESC tex{};back->GetDesc(&tex);tex.Usage=D3D11_USAGE_STAGING;
        tex.CPUAccessFlags=D3D11_CPU_ACCESS_READ;tex.BindFlags=0;tex.MiscFlags=0;
        ComPtr<ID3D11Texture2D> copy;require(device->CreateTexture2D(&tex,nullptr,&copy));
        ctx->CopyResource(copy.Get(),back.Get());
        D3D11_MAPPED_SUBRESOURCE map{};require(ctx->Map(copy.Get(),0,D3D11_MAP_READ,0,&map));
        const auto* pixel=static_cast<const BYTE*>(map.pData)+547*map.RowPitch+12*4;
        assert(pixel[0]>180 && pixel[1]>140 && pixel[2]<140);
        const auto* silverFrame=static_cast<const BYTE*>(map.pData)+577*map.RowPitch+12*4;
        assert(silverFrame[0]>180 && silverFrame[1]>140 && silverFrame[2]<140);
        const auto defaultBackground=(static_cast<const BYTE*>(map.pData)+553*map.RowPitch+180*4)[0];
        assert(defaultBackground>=4 && defaultBackground<=6);
        {
            std::ofstream image("crown-notices.ppm",std::ios::binary);
            image<<"P6\n800 600\n255\n";
            for(unsigned y=0;y<600;++y) for(unsigned x=0;x<800;++x)
                image.write(static_cast<const char*>(map.pData)+y*map.RowPitch+4*x,3);
        }
        ctx->Unmap(copy.Get(),0);
        const auto originalCrowns=crownSettings();
        for(int percent:{0,50,100}) {
            auto testCrowns=originalCrowns;testCrowns.backgroundPercent=percent;testCrowns.opacity=50;
            setCrownSettings(testCrowns);
            ctx->ClearRenderTargetView(view.Get(),clear);
            require(chain->Present(0,0));
            ctx->CopyResource(copy.Get(),back.Get());
            require(ctx->Map(copy.Get(),0,D3D11_MAP_READ,0,&map));
            const auto* backgroundPixel=static_cast<const BYTE*>(map.pData)+553*map.RowPitch+180*4;
            const auto* keptFrame=static_cast<const BYTE*>(map.pData)+547*map.RowPitch+12*4;
            const int expected=64*(100-percent)/100;
            assert(std::abs(backgroundPixel[0]-expected)<=1 && keptFrame[0]>90);
            ctx->Unmap(copy.Get(),0);
        }
        setCrownSettings(originalCrowns);
        CrownSettings withSilver;withSilver.silver=1;withSilver.fixed=0;withSilver.seconds=6;
        feed.show(notice,withSilver,1000);assert(notice.crownCount==2);
        feed.observe(guiding,{rows.data(),2},7000);
        feed.show(notice,withSilver,7000);assert(notice.crownCount==0);
        feed.show(notice,CrownSettings{},7000);assert(notice.crownCount==1);
        feed.observe(guiding,{},7200);
        feed.show(notice,CrownSettings{},7200);assert(notice.crownCount==0);
        feed.observe(guiding,{rows.data(),2},7400);
        feed.show(notice,withSilver,7400);assert(notice.crownCount==0);
        feed.observe(guiding,{},7600);feed.observe(guiding,{},7800);
        feed.observe(guiding,{rows.data(),2},8000);
        feed.show(notice,withSilver,8000);assert(notice.crownCount==2);
        auto hub=guiding;hub.generation=0;
        feed.observe(hub,{},8200);feed.show(notice,withSilver,8200);assert(notice.crownCount==0);
        const auto migration=std::filesystem::absolute("background-migration.ini");
        assert(WritePrivateProfileStringW(L"panels",L"Background",L"0",migration.c_str()));
        assert(WritePrivateProfileStringW(L"crowns",L"Background",L"1",migration.c_str()));
        assert(loadBackgroundPercent(migration,L"panels",84,100)==0);
        assert(loadBackgroundPercent(migration,L"crowns",92,50)==46);
        assert(WritePrivateProfileStringW(L"crowns",L"BackgroundPercent",L"100",migration.c_str()));
        assert(loadBackgroundPercent(migration,L"crowns",92,50)==100);
        initializeSettings(std::filesystem::absolute("crown-settings-test.ini"),false,true);
        testMenuOpen();
        auto tick=[&]()->const Menu& {serviceSettings();return updateMenu(win,800,600,true);};
        tick();const auto& menu=tick();
        assert(menu.open && menu.ready && menu.crown && menu.tabCount==1 && !menu.damage);
        const int initialScale=menu.values[crownScaleIndex];
        testMenuKey(initialScale>40?VK_LEFT:VK_RIGHT);tick();tick();
        const auto scale=crownSettings().scale;
        assert(scale==initialScale+(initialScale>40?-5:5));
        testMenuKey(VK_DOWN);tick();assert(menu.row==crownFixedIndex);
        if(!menu.values[crownFixedIndex]){testMenuKey(VK_RETURN);tick();tick();}
        testMenuKey(VK_DOWN);tick();assert(optionLocked(menu,crownDurationIndex));
        const int previous=menu.values[crownDurationIndex];
        testMenuKey(previous<120?VK_RIGHT:VK_LEFT);tick();tick();
        assert(menu.values[crownDurationIndex]==previous && !menu.rejected);
        const MenuLayout crownMenu(800,600);
        const auto controlX=crownMenu.left+(MenuLayout::valueLeft+60)*crownMenu.scale;
        const auto durationY=crownMenu.top+(110.f+40*crownDurationIndex)*crownMenu.scale;
        testMenuPointer(controlX,durationY,true);tick();assert(!menu.draggingValue);
        testMenuPointer(controlX+20,durationY,false);tick();
        assert(menu.values[crownDurationIndex]==previous);
        testMenuKey(VK_UP);tick();
        testMenuKey(VK_RETURN);tick();tick();
        assert(!menu.values[crownFixedIndex] && !optionLocked(menu,crownDurationIndex));
        testMenuKey(VK_DOWN);tick();
        testMenuKey(previous<120?VK_RIGHT:VK_LEFT);tick();tick();
        const int duration=previous+(previous<120?1:-1);
        assert(menu.values[crownDurationIndex]==duration);
        testMenuKey(VK_DOWN);tick();assert(menu.row==crownOpacityIndex);
        const int initialOpacity=menu.values[crownOpacityIndex];
        testMenuKey(initialOpacity>10?VK_LEFT:VK_RIGHT);tick();tick();
        const int opacity=initialOpacity+(initialOpacity>10?-5:5);
        assert(menu.values[crownOpacityIndex]==opacity);
        testMenuKey(VK_DOWN);tick();assert(menu.row==crownBackgroundIndex);
        const int initialBackground=menu.values[crownBackgroundIndex];
        const int background=initialBackground+(initialBackground<100?1:-1);
        testMenuKey(initialBackground<100?VK_RIGHT:VK_LEFT);tick();tick();
        assert(menu.values[crownBackgroundIndex]==background);
        const int silverIndex=static_cast<int>(menu.provider->count)-1;
        for(int row=crownBackgroundIndex;row<silverIndex;++row){testMenuKey(VK_DOWN);tick();}
        const bool silver=menu.values[silverIndex]!=0;
        testMenuKey(VK_RETURN);tick();tick();
        assert((menu.values[silverIndex]!=0)!=silver && (crownSettings().silver!=0)!=silver);
        testMenuKey('M');tick();assert(menu.moving);
        testMenuPointer(360,240,true);tick();
        testMenuPointer(368,270,false);tick();tick();
        assert(!menu.moving && menu.values[0]==46 && menu.values[1]==45);
        initializeSettings(std::filesystem::absolute("crown-settings-test.ini"),false,true);
        const auto saved=crownSettings();
        assert(saved.x==46 && saved.y==45 && !saved.fixed &&
               saved.seconds==duration && saved.scale==scale && saved.opacity==opacity &&
               saved.backgroundPercent==background);
        testMenuKey(VK_ESCAPE);assert(!tick().open);
        puts("PASS independent 0-100% backgrounds, legacy preferences and settings persistence");
        wchar_t buf[32768]{};
        const DWORD length=GetEnvironmentVariableW(L"ICEBORN_TEST_BUFF_HUD",buf,32768);
        if(length && length<32768) {
            const auto lib=LoadLibraryExW(buf,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
            assert(lib);
            const auto query=reinterpret_cast<IcebornQueryHudTimers>(GetProcAddress(lib,"IcebornQueryHudTimerModule"));
            assert(query);
            const auto* module=query(ICEBORN_HUD_TIMER_ABI);
            assert(module && module->layout);
            const IcebornHudTimer buffs[]{
                {ICEBORN_MIGHT_SEED,ICEBORN_TIMER_ACTIVE,ICEBORN_BUFF_ICON,90,180},
                {ICEBORN_DEMON_POWDER,ICEBORN_TIMER_ACTIVE,ICEBORN_BUFF_ICON,5,180},
                {ICEBORN_TEMPORAL,ICEBORN_TIMER_ACTIVE,ICEBORN_SECOND_MANTLE_SLOT,60,120},
            };
            const IcebornHudIconBounds icons[]{{1,116,42,136,61},{1,116,42,136,61},{2,48,84,68,102}};
            Scene hud;
            hud.buffHud.viewportWidth=800;
            hud.buffHud.viewportHeight=600;
            hud.buffHud.count=module->layout(buffs,icons,3,1,hud.buffHud.rows.data(),ICEBORN_MAX_HUD_TIMERS);
            assert(hud.buffHud.count==3 && hud.buffHud.rows[1].top<hud.buffHud.rows[0].top);
            auto rendered=[&] {
                ctx->ClearRenderTargetView(view.Get(),clear);
                require(chain->Present(0,0));
                ctx->CopyResource(copy.Get(),back.Get());
                require(ctx->Map(copy.Get(),0,D3D11_MAP_READ,0,&map));
                const auto* buff=static_cast<const BYTE*>(map.pData)+39*map.RowPitch+120*4;
                const auto* mantle=static_cast<const BYTE*>(map.pData)+103*map.RowPitch+51*4;
                const bool visible=buff[1]>160 && mantle[1]>160;
                ctx->Unmap(copy.Get(),0);
                return visible;
            };
            publish(hud);assert(rendered());
            hud.buffHud.viewportWidth=1024;publish(hud);assert(!rendered());
            FreeLibrary(lib);
            puts("PASS Buff Tracker layout compositing and viewport mismatch suppression");
        }
    }
    puts("PASS module negotiation, vertical HP stack pixel readback, resize and inactive quest");
    return 0;
}
