#include "sampling.hpp"
#include "settings.hpp"
#include "crowns.hpp"
#include "exe_hash.hpp"
#include <cstdio>
#include <cstdarg>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <thread>
namespace meter {
std::atomic<bool> stopping{};
void log(const char* format,...) {
    char text[512]{};
    va_list args; va_start(args,format); vsnprintf_s(text,sizeof(text),_TRUNCATE,format,args); va_end(args);
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
}
namespace {
int32_t readExact(void*,uint64_t address,void* dest,uint32_t size) {
    SIZE_T bytes{};
    return size && address>=0x10000 && address<=0x800000000000ull-size &&
        ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),dest,size,&bytes) && bytes==size;
}
IcebornFrame questSample(uint64_t base) {
    IcebornFrame frame{sizeof(IcebornFrame),421810,0,0,0,base,nullptr,readExact};
    uint64_t ptr{},check{};
    std::array<int32_t,3> quest{};
    if(readExact(nullptr,base+0x0500ed30,&ptr,sizeof(ptr)) && ptr>=0x10000 &&
       ptr<0x800000000000ull-0x60 && readExact(nullptr,ptr+0x4c,quest.data(),sizeof(quest)) &&
       readExact(nullptr,base+0x0500ed30,&check,sizeof(check)) && ptr==check) {
        frame.quest=quest[0]; frame.state=quest[2]; frame.generation=ptr;
    }
    return frame;
}
}
DWORD WINAPI start(void*) {
    try {
        wchar_t path[32768]{};
        auto n=GetModuleFileNameW(nullptr,path,32768);
        if (!n || n>=32768) return 1;
        const std::filesystem::path game(path);
        if (_wcsicmp(game.filename().c_str(),L"MonsterHunterWorld.exe") ||
            executable_hash(game)!="c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea") return 1;
        Modules modules{};
        const auto suite=game.parent_path()/L"nativePC"/L"IcebornSuite";
        const auto root=suite/L"modules";
        const auto user=suite/L"user";
        std::filesystem::create_directories(user);
        std::ofstream diagnostic(user/L"core-dev5.log",std::ios::app);
        diagnostic<<"Core 0.1.0-dev.17: supported 421810, PID "<<GetCurrentProcessId()<<std::endl;
        const auto ini=user/L"core.ini";
        auto setting=[&](const wchar_t* key,int fallback,int low,int high) {
            return std::clamp(static_cast<int>(GetPrivateProfileIntW(L"panels",key,fallback,ini.c_str())),low,high);
        };
        Placement placement;
        placement.left=setting(L"Left",-1,-1,8192);
        placement.top_percent=setting(L"TopPercent",2,0,100);
        placement.scale_percent=setting(L"ScalePercent",100,40,175);
        placement.opacity_percent=setting(L"OpacityPercent",100,10,100);
        placement.background_percent=loadBackgroundPercent(ini,L"panels",placement.background_percent,placement.opacity_percent);
        placement.visible=setting(L"Visible",1,0,1)!=0;
        setMonsterHealthAlways(setting(L"MonsterHealthAlways",0,0,1)!=0);
        placement.x_percent=setting(L"HorizontalPercent",-1,-1,100);
        wchar_t pair[48]{},more{};int x{},y{};
        GetPrivateProfileStringW(L"panels",L"Position",L"",pair,48,ini.c_str());
        if(swscanf_s(pair,L"%d,%d%c",&x,&y,&more,1u)==2 && x>=-1 && x<=100 && y>=0 && y<=100) {
            placement.x_percent=x;placement.top_percent=y;
        }
        setPanelPlacement(placement);
        const auto timerSetting=[&](const wchar_t* key,int fallback,int low,int high) {
            return std::clamp(static_cast<int>(GetPrivateProfileIntW(L"timers",key,fallback,ini.c_str())),low,high);
        };
        const Placement timerLayout{-1,timerSetting(L"TopPercent",12,0,100),
            timerSetting(L"ScalePercent",100,40,175),timerSetting(L"HorizontalPercent",98,0,100),
            timerSetting(L"Visible",1,0,1)!=0};
        setTimerPlacement(timerLayout);
        const auto* timers=timerLayout.visible && GetPrivateProfileIntW(L"modules",L"buff-tracker",1,ini.c_str())!=0?
            loadTimers(root/L"buff-tracker"/L"0.1.0-dev.1"/L"module.dll"):nullptr;
        if(timers)diagnostic<<"Module ready: buff-tracker"<<std::endl;
        const IcebornCrownModule* crowns=nullptr;
        if(GetPrivateProfileIntW(L"modules",L"crown-tracker",1,ini.c_str())) {
            const auto directory=root/L"crown-tracker";
            crowns=loadCrowns(directory/L"0.1.0-dev.2"/L"module.dll");
            if(!crowns)crowns=loadCrowns(directory/L"0.1.0-dev.1"/L"module.dll");
        }
        if(crowns)diagnostic<<"Module ready: crown-tracker"<<std::endl;
        int loaded=0;
        bool hp=false;
        for(const char* id:{"monster-info","tool-tracker"}) {
            std::filesystem::path name(id);
            if(GetPrivateProfileIntW(L"modules",name.c_str(),1,ini.c_str())==0)continue;
            const bool monsterInfo=strcmp(id,"monster-info")==0;
            if((monsterInfo && (loadPanel(root/name/L"0.1.0-dev.4"/L"module.dll",id,modules) ||
                                loadPanel(root/name/L"0.1.0-dev.3"/L"module.dll",id,modules))) ||
               loadPanel(root/name/L"0.1.0-dev.2"/L"module.dll",id,modules) ||
               loadPanel(root/name/L"0.1.0-dev.1"/L"module.dll",id,modules)) {
                ++loaded; hp=hp || monsterInfo; diagnostic<<"Module ready: "<<id<<std::endl;
            }
        }
        initializeSettings(ini,hp,crowns!=nullptr);
        HMODULE pinned{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
               reinterpret_cast<LPCWSTR>(&start),&pinned)) return 3;
        if(!installHooks()) {removeHooks();return 4;}
        const auto base=reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
        CrownFeed crownFeed;
        bool reported=false;
        while(!stopping.load()) {
            findDamageSettings();
            serviceSettings();
            if(!loaded && !timers && !crowns) {std::this_thread::sleep_for(std::chrono::milliseconds(200));continue;}
            auto frame=questSample(base);
            auto scene=samplePanels(modules,frame,timers);
            std::array<IcebornCrownRow,ICEBORN_MAX_CROWNS> crownRows{};
            const auto count=crowns?crowns->sample(&frame,crownRows.data(),static_cast<uint32_t>(crownRows.size())):-1;
            const auto now=GetTickCount64();
            if(scene.count || scene.timerCount || crowns) {
                auto check=questSample(base);
                if(check.quest!=frame.quest || check.state!=frame.state || check.generation!=frame.generation)
                    scene={};
                else if(crowns) {
                    if(count>=0 && static_cast<size_t>(count)<=crownRows.size())
                        crownFeed.observe(frame,{crownRows.data(),static_cast<size_t>(count)},now);
                    crownFeed.show(scene,crownSettings(),now);
                }
            }
            if(scene.count && !reported) {
                diagnostic<<"First active panel: quest "<<frame.quest<<", rows "<<scene.count<<std::endl;
                reported=true;
            }
            publish(scene);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    } catch(...) { log("Core startup or sample failed."); publish({}); return 5; }
    return 0;
}
}
extern "C" __declspec(dllexport) void IcebornStartCore() {
    static std::once_flag once;
    std::call_once(once,[]{if(auto thread=CreateThread(nullptr,0,meter::start,nullptr,0,nullptr)) CloseHandle(thread);});
}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_DETACH) meter::stopping=true;
    return TRUE;
}
