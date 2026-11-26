#include "sampling.hpp"
#include <algorithm>
#include <cstring>
namespace meter {
namespace {
std::mutex placementMutex;
Placement savedPlacement;
Placement savedTimers{-1,12,100,98,true};
}
Placement panelPlacement() {
    std::lock_guard lock(placementMutex);
    return savedPlacement;
}
void setPanelPlacement(const Placement& value) {
    std::lock_guard lock(placementMutex);
    savedPlacement=value;
}
Placement timerPlacement() {
    std::lock_guard lock(placementMutex);
    return savedTimers;
}
void setTimerPlacement(const Placement& value) {
    std::lock_guard lock(placementMutex);
    savedTimers=value;
}
namespace {
std::mutex scene_mutex;
Scene published;
using SetMonsterHealthMode = void (*)(int32_t);
SetMonsterHealthMode setHealthMode{};
bool alwaysShowMonsterHealth{};
}
bool monsterHealthModeAvailable() { return setHealthMode!=nullptr; }
bool monsterHealthAlways() { return alwaysShowMonsterHealth; }
void setMonsterHealthAlways(bool always) {
    alwaysShowMonsterHealth=always;
    if(setHealthMode)setHealthMode(always);
}
bool loadPanel(const std::filesystem::path& path, const char* expected, Modules& modules) {
    for (auto mod : modules) if (mod && strcmp(mod->id,expected)==0) return true;
    auto slot=std::find(modules.begin(),modules.end(),nullptr);
    if(slot==modules.end() || !path.is_absolute()) return false;
    auto library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!library) return false;
    const auto query=reinterpret_cast<IcebornQuery>(GetProcAddress(library,"IcebornQueryPanel"));
    const auto* api=query ? query(ICEBORN_PANEL_ABI) : nullptr;
    if (!api || api->size!=sizeof(*api) || api->abi!=ICEBORN_PANEL_ABI ||
        !memchr(api->id,0,sizeof(api->id)) || strcmp(api->id,expected) ||
        api->period_ms!=200 || !api->sample) {
        FreeLibrary(library);
        return false;
    }
    if(strcmp(expected,"monster-info")==0) {
        setHealthMode=reinterpret_cast<SetMonsterHealthMode>(GetProcAddress(library,"IcebornSetMonsterHealthAlways"));
        if(setHealthMode)setHealthMode(alwaysShowMonsterHealth);
    }
    *slot=api;
    return true; // Process-owned while its callback is registered.
}
const IcebornTimerModule* loadTimers(const std::filesystem::path& path) {
    if(!path.is_absolute())return nullptr;
    auto library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!library)return nullptr;
    const auto query=reinterpret_cast<IcebornQueryTimers>(GetProcAddress(library,"IcebornQueryTimerModule"));
    const auto* api=query?query(ICEBORN_TIMER_ABI):nullptr;
    if(api && api->size==sizeof(*api) && api->abi==ICEBORN_TIMER_ABI && api->period_ms==200 && api->sample)
        return api;
    FreeLibrary(library);
    return nullptr;
}
Scene samplePanels(const Modules& modules,const IcebornFrame& frame,const IcebornTimerModule* timers) {
    Scene scene{};
    if(frame.profile!=421810) return scene;
    const bool activeQuest=frame.quest>0 && frame.state==2;
    for(const auto* mod:modules) {
        if (!mod) continue;
        // Monster Info also samples expeditions and clears its visit on exit.
        if (!activeQuest && strcmp(mod->id,"monster-info")!=0) continue;
        auto available=ICEBORN_MAX_ROWS-scene.count;
        if(!available) break;
        const auto n=mod->sample(&frame,scene.rows.data()+scene.count,available);
        if(n<=available) scene.count+=n;
    }
    if(timers) {
        const auto count=timers->sample(&frame,scene.timers.data(),ICEBORN_MAX_TIMERS);
        if(count<=ICEBORN_MAX_TIMERS)scene.timerCount=count;
    }
    return scene;
}
void publish(const Scene& scene) {
    std::lock_guard lock(scene_mutex);
    if(published.buffHud.count==scene.buffHud.count &&
       published.buffHud.viewportWidth==scene.buffHud.viewportWidth &&
       published.buffHud.viewportHeight==scene.buffHud.viewportHeight &&
       !memcmp(published.buffHud.rows.data(),scene.buffHud.rows.data(),
               scene.buffHud.count*sizeof(IcebornHudBar)) &&
       published.crownCount==scene.crownCount &&
       std::equal(scene.crowns.begin(),scene.crowns.begin()+scene.crownCount,published.crowns.begin()) &&
       published.timerCount==scene.timerCount &&
       !memcmp(published.timers.data(),scene.timers.data(),scene.timerCount*sizeof(IcebornTimerRow)) &&
       published.count==scene.count &&
       !memcmp(published.rows.data(),scene.rows.data(),scene.count*sizeof(IcebornRow))) return;
    const auto revision=published.revision+1;
    published=scene; published.revision=revision;
}
void latest(Scene& current) {
    std::unique_lock lock(scene_mutex,std::try_to_lock);
    if(lock && current.revision!=published.revision) current=published;
}
}
