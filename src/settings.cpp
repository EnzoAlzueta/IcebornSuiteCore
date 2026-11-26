#include "runtime.hpp"
#include "../sdk/settings-dev.1/iceborn_settings.h"
#include "settings.hpp"
#include "sampling.hpp"
#include "crowns.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

namespace meter {
namespace {

std::filesystem::path settingsFile;
bool hasHealth=false, hasCrowns=false;
std::atomic<const IcebornSettings*> damage{};
std::atomic<bool> legacyMeter{}, dx11Ready{};
using BorrowInput=int (*)(HWND,int,POINT*,int*);
std::atomic<BorrowInput> meterInput{};
std::atomic<IcebornDrawPreview> previewMeter{};
using DrawSettingPreview=int (*)(void*,const IcebornPreviewArea*,int,int);
std::atomic<DrawSettingPreview> previewMeterSetting{};
std::atomic<int> legacyHorizontal{50};

constexpr IcebornSetting healthOptions[]={{L"Horizontal (%)",0,100,1},{L"Vertical (%)",0,100,1},
    {L"Escala (%)",40,175,5},{L"Mostrar barras",0,1,1},{L"Opacidad (%)",10,100,5},
    {L"Fondo oscuro (%)",0,100,1},{L"Mostrar siempre en misiones",0,1,1}};
int readHealth(int32_t* values,uint32_t count) {
    if(!values || count<6) return 0;
    const auto p=panelPlacement();
    values[0]=p.x_percent<0 ? legacyHorizontal.load() : p.x_percent;
    values[1]=p.top_percent; values[2]=p.scale_percent; values[3]=p.visible;
    values[healthOpacityIndex]=p.opacity_percent;
    values[healthBackgroundIndex]=p.background_percent;
    if(count>healthMissionModeIndex)values[healthMissionModeIndex]=monsterHealthAlways();
    return 1;
}
bool persist(const wchar_t* key,int value) {
    return WritePrivateProfileStringW(L"panels",key,std::to_wstring(value).c_str(),settingsFile.c_str())!=0;
}
int setHealth(uint32_t index,int value) {
    if(index>=std::size(healthOptions) || value<healthOptions[index].minimum || value>healthOptions[index].maximum) return 0;
    if(index==healthMissionModeIndex) {
        if(!persist(L"MonsterHealthAlways",value))return 0;
        setMonsterHealthAlways(value!=0);
        return 1;
    }

    auto p=panelPlacement();
    switch(index) {
        case 0: p.x_percent=value; break;
        case 1: p.top_percent=value; break;
        case 2: p.scale_percent=value; break;
        case 3: p.visible=value!=0; break;
        case healthOpacityIndex: p.opacity_percent=value; break;
        case healthBackgroundIndex: p.background_percent=value; break;
    }
    if(index<2) {
        const auto pair=std::to_wstring(p.x_percent)+L","+std::to_wstring(p.top_percent);
        if(!WritePrivateProfileStringW(L"panels",L"Position",pair.c_str(),settingsFile.c_str())) return 0;
    } else {
        constexpr const wchar_t* keys[]{L"ScalePercent",L"Visible",L"OpacityPercent",L"BackgroundPercent"};
        if(!persist(keys[index-2],value))return 0;
    }
    setPanelPlacement(p);
    return 1;
}
int moveHealth(int x,int y) {
    if(x<0 || x>100 || y<0 || y>100) return 0;
    const auto pair=std::to_wstring(x)+L","+std::to_wstring(y);
    if(!WritePrivateProfileStringW(L"panels",L"Position",pair.c_str(),settingsFile.c_str())) return 0;
    auto p=panelPlacement();p.x_percent=x;p.top_percent=y;setPanelPlacement(p);
    return 1;
}
IcebornSettings health{sizeof(IcebornSettings),ICEBORN_SETTINGS_ABI,L"Vida de monstruos",
    uint32_t(std::size(healthOptions)),healthOptions,readHealth,setHealth,moveHealth};

constexpr IcebornSetting crownOptions[]={{L"Horizontal (%)",0,100,1},{L"Vertical (%)",0,100,1},
    {L"Escala (%)",40,175,5},{L"Dejar lista fija",0,1,1},{L"Duración en modo temporal (s)",1,120,1},
    {L"Opacidad (%)",10,100,5},{L"Fondo oscuro (%)",0,100,1},{L"Mostrar coronas",0,1,1},
    {L"Corona chica de oro",0,1,1},
    {L"Corona grande de oro",0,1,1},{L"Corona grande de plata",0,1,1}};
constexpr int CrownSettings::* crownFields[]{&CrownSettings::x,&CrownSettings::y,&CrownSettings::scale,
    &CrownSettings::fixed,&CrownSettings::seconds,&CrownSettings::opacity,&CrownSettings::backgroundPercent,&CrownSettings::visible,
    &CrownSettings::mini,&CrownSettings::gold,&CrownSettings::silver};
constexpr const wchar_t* crownKeys[]{L"",L"",L"ScalePercent",L"Fixed",L"Seconds",L"OpacityPercent",
    L"BackgroundPercent",L"Visible",L"MiniGold",L"LargeGold",L"LargeSilver"};
int readCrowns(int32_t* values,uint32_t count) {
    if(!values || count<std::size(crownFields))return 0;
    const auto current=crownSettings();
    for(size_t i=0;i<std::size(crownFields);++i) values[i]=current.*crownFields[i];
    return 1;
}
bool saveCrownPosition(const CrownSettings& current) {
    const auto pair=std::to_wstring(current.x)+L","+std::to_wstring(current.y);
    return WritePrivateProfileStringW(L"crowns",L"Position",pair.c_str(),settingsFile.c_str())!=0;
}
int setCrown(uint32_t index,int value) {
    if(index>=std::size(crownOptions) || value<crownOptions[index].minimum || value>crownOptions[index].maximum)
        return 0;
    auto current=crownSettings();
    if(index==crownDurationIndex && current.fixed)return 0;
    current.*crownFields[index]=value;
    const bool saved=index<2?saveCrownPosition(current):
        WritePrivateProfileStringW(L"crowns",crownKeys[index],std::to_wstring(value).c_str(),settingsFile.c_str())!=0;
    if(saved)setCrownSettings(current);
    return saved;
}
int moveCrown(int x,int y) {
    if(x<0 || x>100 || y<0 || y>100)return 0;
    auto current=crownSettings();
    current.x=x;current.y=y;
    if(!saveCrownPosition(current))return 0;
    setCrownSettings(current);
    return 1;
}
const IcebornSettings crowns{sizeof(IcebornSettings),ICEBORN_SETTINGS_ABI,L"Coronas",
    uint32_t(std::size(crownOptions)),crownOptions,readCrowns,setCrown,moveCrown};

enum class Edit { Adjust, Set, Toggle, Move };
struct Command { const IcebornSettings* provider; Edit kind; int row,a,b; unsigned ticket; };
struct Snapshot {
    const IcebornSettings* provider{};
    std::array<int32_t,64> values{};
    bool ready{},rejected{};
    unsigned revision{},completed{};
};
std::mutex queueMutex;
std::array<Command,32> commands{};
size_t queued{};
Snapshot snapshot{};
std::atomic<const IcebornSettings*> selected{};
std::atomic<unsigned> requestKey{};
Menu menu;
std::array<bool,256> held{};
bool mouseHeld=false, armed=false;
float dragOriginX=0;
int dragOriginValue=0;
unsigned lastTicket=0,pendingValue=0;
uint32_t shortcut=VK_F6;
BorrowInput borrowed{};
#ifdef CORE_GRAPHICS_TEST
int simulatedKey=0;float testX=0,testY=0;bool testDown=false;
#endif
void requestMenu(void*,uint32_t key) {requestKey.store(key);}
bool pressed(int key) {
#ifdef CORE_GRAPHICS_TEST
    if(simulatedKey==key) {simulatedKey=0;return true;}
    return false;
#else
    const bool down=(GetAsyncKeyState(key)&0x8000)!=0;
    const bool edge=down && !held[key];
    held[key]=down;
    return edge;
#endif
}
void clearValuePreview() {
    menu.previewRow=-1;menu.draggingValue=false;pendingValue=0;
}
void select(const IcebornSettings* provider) {
    clearValuePreview();
    menu.provider=provider;menu.crown=provider==&crowns;menu.damage=provider!=&health&&!menu.crown;menu.row=2;
    menu.moving=false;menu.ready=false;menu.rejected=false;selected=provider;++menu.revision;
}
void updateTabs(const IcebornSettings* meter) {
    std::array<const IcebornSettings*,3> tabs{};
    int count=0;
    if(hasHealth)tabs[count++]=&health;
    if(meter)tabs[count++]=meter;
    if(hasCrowns)tabs[count++]=&crowns;
    if(tabs!=menu.tabs) {menu.tabs=tabs;menu.tabCount=count;++menu.revision;}
}
void closeMenu(HWND window) {
#ifndef CORE_GRAPHICS_TEST
    if(borrowed) borrowed(window,0,nullptr,nullptr);
    else setEditing(false);
#endif
    clearValuePreview();
    borrowed=nullptr;menu.open=false;menu.moving=false;selected=nullptr;++menu.revision;
}
unsigned enqueue(Edit kind,int a,int b=0) {
    if(!menu.ready || (kind!=Edit::Move && optionLocked(menu,menu.row)))return 0;
    std::lock_guard guard(queueMutex);
    ++menu.revision;
    if(queued==commands.size()) {menu.rejected=true;return 0;}
    const auto ticket=++lastTicket;
    commands[queued++]={menu.provider,kind,menu.row,a,b,ticket};
    return ticket;
}
void chooseRow(int delta) {
    menu.row=std::clamp(menu.row+delta,0,int(menu.provider->count)-1);
    ++menu.revision;
}
void startMove() {
    if(!menu.ready) return;
    menu.x=menu.values[0];menu.y=menu.values[1];menu.moving=true;armed=false;++menu.revision;
}
void finishMove() {
    enqueue(Edit::Move,menu.x,menu.y);menu.moving=false;++menu.revision;
}
}
MenuLayout::MenuLayout(float w,float h) {
    scale=std::min({w/(width+24.f),h/(height+24.f),std::clamp(h/740.f,.74f,1.65f)});
    left=(w-width*scale)/2;top=h-(height+12)*scale;
}
int loadBackgroundPercent(const std::filesystem::path& ini,const wchar_t* section,
                          int defaultPercent,int panelOpacity) {
    const bool oldBackground=GetPrivateProfileIntW(section,L"Background",1,ini.c_str())!=0;
    const int fallback=oldBackground?(defaultPercent*panelOpacity+50)/100:0;
    return std::clamp(static_cast<int>(GetPrivateProfileIntW(section,L"BackgroundPercent",fallback,ini.c_str())),
                      0,100);
}
void initializeSettings(const std::filesystem::path& ini,bool hp,bool crown) {
    settingsFile=ini;hasHealth=hp;hasCrowns=crown;
    health.count=uint32_t(std::size(healthOptions))-(monsterHealthModeAvailable()?0:1);
    if(!crown)return;
    CrownSettings values;
    for(size_t i=2;i<std::size(crownFields);++i) {
        auto& value=values.*crownFields[i];
        if(i==crownBackgroundIndex)value=loadBackgroundPercent(ini,L"crowns",value,values.opacity);
        else value=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"crowns",crownKeys[i],value,ini.c_str())),
                              crownOptions[i].minimum,crownOptions[i].maximum);
    }
    wchar_t pair[48]{},more{};int x{},y{};
    GetPrivateProfileStringW(L"crowns",L"Position",L"",pair,48,ini.c_str());
    if(swscanf_s(pair,L"%d,%d%c",&x,&y,&more,1u)==2 && x>=0 && x<=100 && y>=0 && y<=100) {
        values.x=x;values.y=y;
    }
    setCrownSettings(values);
}
void markMenuSurfaceReady(){dx11Ready=true;}
void findDamageSettings() {
    static bool checked=false;
    if(checked || !dx11Ready.load()) return;
    HMODULE plugin{};
    if(!GetModuleHandleExW(0,L"iceborne_damage_meter.dll",&plugin))return;
    auto query=reinterpret_cast<IcebornConnectSettings>(GetProcAddress(plugin,"IcebornConnectMeterSettings"));
    auto input=reinterpret_cast<BorrowInput>(GetProcAddress(plugin,"IcebornMenuInputV1"));
    const auto* p=query && input?query(ICEBORN_SETTINGS_ABI,requestMenu):nullptr;
    checked=true;
    if(p && p->size==sizeof(*p) && p->abi==ICEBORN_SETTINGS_ABI && p->count>=3 && p->count<=64 &&
       p->title && p->options && p->read && p->set && p->move) {
        previewMeter.store(reinterpret_cast<IcebornDrawPreview>(GetProcAddress(plugin,"IcebornDrawMeterPreviewV1")));
        previewMeterSetting.store(reinterpret_cast<DrawSettingPreview>(GetProcAddress(plugin,"IcebornDrawMeterSettingPreview")));
        meterInput.store(input);damage.store(p);
    } else {legacyMeter=true;FreeLibrary(plugin);}
}
void serviceSettings() {
    // Only this worker calls providers or writes INIs. Present never waits for meter rendering.
    std::array<Command,32> batch{};size_t count{};
    {
        std::lock_guard guard(queueMutex);
        count=queued;std::copy_n(commands.begin(),count,batch.begin());queued=0;
    }
    auto* provider=selected.load();
    bool rejected=false;
    unsigned completed=0;
    for(size_t i=0;i<count;++i) {
        const auto& cmd=batch[i]; const auto* p=cmd.provider;
        bool ok=false;
        if(cmd.kind==Edit::Move) ok=p->move(cmd.a,cmd.b)!=0;
        else if(cmd.kind==Edit::Set) ok=p->set(cmd.row,cmd.a)!=0;
        else {
            std::array<int32_t,64> values{};
            const auto& opt=p->options[cmd.row];
            if(p->read(values.data(),64)) {
                const int current=values[cmd.row];
                const int next=cmd.kind==Edit::Toggle?!current:std::clamp(current+cmd.a*opt.step,opt.minimum,opt.maximum);
                ok=p->set(cmd.row,next)!=0;
            }
        }
        if(p==provider) {rejected|=!ok;completed=cmd.ticket;}
    }
    if(!provider) return;
    Snapshot next{};next.provider=provider;next.ready=provider->read(next.values.data(),64)!=0;
    std::lock_guard guard(queueMutex);
    next.completed=completed?completed:snapshot.completed;
    next.rejected=count?rejected:(snapshot.provider==provider && snapshot.rejected);
    if(next.provider!=snapshot.provider || next.values!=snapshot.values || next.ready!=snapshot.ready ||
       next.rejected!=snapshot.rejected || next.completed!=snapshot.completed) {next.revision=snapshot.revision+1;snapshot=next;}
}
bool drawMeterPreview(void* chain,const IcebornPreviewArea& area,int row,int value) {
    if(row>=2)if(const auto draw=previewMeterSetting.load())return draw(chain,&area,row,value)!=0;
    const auto draw=previewMeter.load();
    return draw && draw(chain,&area);
}
void rememberLegacyX(int percent){legacyHorizontal=percent;}
void suspendMenu(HWND window) {
    requestKey=0;
    if(menu.open)closeMenu(window);
}
const Menu& updateMenu(HWND window,float w,float h,bool focused) {
    if(!focused || w<=0 || h<=0){suspendMenu(window);return menu;}
    const auto* meter=damage.load();
    updateTabs(meter);
    unsigned requested=requestKey.exchange(0);
    if(!menu.open && !meter && menu.tabCount) {
        const int key=legacyMeter.load()?VK_F10:VK_F6;
        if(pressed(key) && !(GetAsyncKeyState(VK_CONTROL)&0x8000))requested=key;
    }
    if(!menu.open) {
        if(!requested || !menu.tabCount)return menu;
        borrowed=meterInput.load();
#ifndef CORE_GRAPHICS_TEST
        if(borrowed) {if(!borrowed(window,1,nullptr,nullptr))return menu;}
        else {if(!installInput(window))return menu;setEditing(true);}
#endif
        shortcut=requested;
        held.fill(false);held[shortcut]=true;mouseHeld=true;
        menu.open=true;select(menu.tabs[0]);
        menu.cursorX=w*.5f;menu.cursorY=h*.5f;
        return menu;
    }
    if(requested || pressed(shortcut)) {closeMenu(window);return menu;}
    if(pressed(VK_ESCAPE)) {
        if(menu.draggingValue){clearValuePreview();++menu.revision;}
        else if(menu.moving){menu.moving=false;++menu.revision;}
        else closeMenu(window);
        return menu;
    }
    {
        std::unique_lock guard(queueMutex,std::try_to_lock);
        if(guard && snapshot.provider==menu.provider && (menu.values!=snapshot.values ||
           menu.ready!=snapshot.ready || menu.rejected!=snapshot.rejected)) {
            menu.values=snapshot.values;menu.ready=snapshot.ready;menu.rejected=snapshot.rejected;++menu.revision;
        }
        if(guard && pendingValue && snapshot.provider==menu.provider && snapshot.completed>=pendingValue) {
            clearValuePreview();++menu.revision;
        }
    }
    bool down=false;POINT pt{};RECT client{};
    const float lastX=menu.cursorX,lastY=menu.cursorY;
#ifndef CORE_GRAPHICS_TEST
    int button{};
    bool have=borrowed ? borrowed(window,1,&pt,&button)!=0 : getEditPointer(pt,down);
    if(!have){closeMenu(window);return menu;}
    if(borrowed) down=button!=0;
    if(have && GetClientRect(window,&client) && client.right && client.bottom) {
        menu.cursorX=std::clamp(pt.x*w/client.right,0.f,w-1);
        menu.cursorY=std::clamp(pt.y*h/client.bottom,0.f,h-1);
    }
#endif
#ifdef CORE_GRAPHICS_TEST
    menu.cursorX=testX;menu.cursorY=testY;down=testDown;
#endif
    const bool click=down && !mouseHeld, release=!down && mouseHeld;
    mouseHeld=down;
    bool motion=lastX!=menu.cursorX || lastY!=menu.cursorY;
    if(motion)++menu.revision;
    if(menu.moving) {
        if(motion || click) {
            menu.x=std::clamp(int(menu.cursorX*100/w+.5f),0,100);
            menu.y=std::clamp(int(menu.cursorY*100/h+.5f),0,100);
        }
        int dx=int(pressed(VK_RIGHT))-int(pressed(VK_LEFT));
        int dy=int(pressed(VK_DOWN))-int(pressed(VK_UP));
        if(dx||dy){menu.x=std::clamp(menu.x+dx,0,100);menu.y=std::clamp(menu.y+dy,0,100);++menu.revision;}
        if(click)armed=true;
        if((armed && release)||pressed(VK_RETURN))finishMove();
        return menu;
    }
    const MenuLayout l(w,h);
    const float x=(menu.cursorX-l.left)/l.scale,y=(menu.cursorY-l.top)/l.scale;
    if(menu.draggingValue) {
        const auto& option=menu.provider->options[menu.previewRow];
        const int steps=static_cast<int>((x-dragOriginX)/8.f);
        const int value=std::clamp(dragOriginValue+steps*option.step,option.minimum,option.maximum);
        if(value!=menu.previewValue){menu.previewValue=value;++menu.revision;}
        if(release) {
            menu.draggingValue=false;
            if(value!=dragOriginValue)pendingValue=enqueue(Edit::Set,value);
            if(!pendingValue)clearValuePreview();
            ++menu.revision;
        }
        return menu;
    }
    if(menu.tabCount>1 && pressed(VK_TAB)) {
        const int index=static_cast<int>(std::find(menu.tabs.begin(),menu.tabs.end(),menu.provider)-menu.tabs.begin());
        select(menu.tabs[(index+1)%menu.tabCount]);
    }
    if(pendingValue)return menu;
    if(pressed(VK_UP))chooseRow(-1);
    if(pressed(VK_DOWN))chooseRow(1);
    if(pressed(VK_LEFT))enqueue(Edit::Adjust,-1);
    if(pressed(VK_RIGHT))enqueue(Edit::Adjust,1);
    if(pressed(VK_RETURN) && isToggle(menu.provider->options[menu.row]))
        enqueue(Edit::Toggle,0);
    if(pressed('M'))startMove();
    if(!click) return menu;
    if(x<0||x>=l.width || y<0 || y>=l.height)return menu;
    if(y<48 && x>=486)closeMenu(window);
    else if(y>=51 && y<=84 && x>=18 && x<522 && menu.tabCount>1) {
        for(int i=0;i<menu.tabCount;++i)
            if(x>=l.tabLeft(i,menu.tabCount) && x<l.tabLeft(i+1,menu.tabCount)-4) {
                select(menu.tabs[i]);break;
            }
    }
    else if(y>=94 && y<94+5*40) {
        const int row=menu.row/5*5+int((y-94)/40);
        if(row<int(menu.provider->count)) {
            menu.row=row;++menu.revision;
            if(menu.ready && !optionLocked(menu,row) && x>=l.valueLeft && x<l.valueRight && y<94+(row%5)*40+36) {
                const auto& option=menu.provider->options[row];
                if(isToggle(option))enqueue(Edit::Toggle,0);
                else {
                    menu.previewRow=row;menu.draggingValue=true;
                    dragOriginX=x;dragOriginValue=menu.previewValue=menu.values[row];
                }
            }
        }
    } else if(y>=300 && y<=333) {
        if(x>=20 && x<=215)startMove();
        if(x>=418 && x<463)chooseRow(-5);
        if(x>=471 && x<522)chooseRow(5);
    }
    return menu;
}
#ifdef CORE_GRAPHICS_TEST
void testMenuOpen(){requestMenu(nullptr,VK_F6);}
void testMenuKey(int vk){simulatedKey=vk;}
void testMenuPointer(float x,float y,bool down){testX=x;testY=y;testDown=down;}
#endif
}

