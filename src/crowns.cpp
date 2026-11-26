#include "crowns.hpp"
#include <algorithm>

namespace meter {
namespace {
std::mutex crownMutex;
CrownSettings settings;

bool sameMonster(const IcebornCrownRow& first, const IcebornCrownRow& second) {
    return first.instance==second.instance && first.component==second.component &&
        first.species==second.species;
}
bool crownEnabled(uint32_t kind, const CrownSettings& value) {
    switch(kind) {
    case ICEBORN_CROWN_MINI_GOLD:return value.mini!=0;
    case ICEBORN_CROWN_LARGE_GOLD:return value.gold!=0;
    case ICEBORN_CROWN_LARGE_SILVER:return value.silver!=0;
    default:return false;
    }
}
}

CrownSettings crownSettings() {
    std::lock_guard guard(crownMutex);
    return settings;
}
void setCrownSettings(const CrownSettings& value) {
    std::lock_guard guard(crownMutex);
    settings=value;
}
const wchar_t* crownMonsterName(int32_t species) {
    struct Name {int32_t species; const wchar_t* text;};
    static constexpr Name names[]{
#include "crown_names.inc"
    };
    const auto* name=std::lower_bound(std::begin(names),std::end(names),species,
        [](const Name& entry,int32_t id){return entry.species<id;});
    return name!=std::end(names) && name->species==species?name->text:nullptr;
}

const IcebornCrownModule* loadCrowns(const std::filesystem::path& path) {
    if(!path.is_absolute())return nullptr;
    const auto library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!library)return nullptr;
    const auto query=reinterpret_cast<IcebornQueryCrowns>(GetProcAddress(library,"IcebornQueryCrownModule"));
    const auto* api=query?query(ICEBORN_CROWN_ABI):nullptr;
    if(api && api->size==sizeof(*api) && api->abi==ICEBORN_CROWN_ABI && api->period_ms==200 && api->sample)
        return api;
    FreeLibrary(library);
    return nullptr;
}

void CrownFeed::observe(const IcebornFrame& frame, std::span<const IcebornCrownRow> rows, uint64_t now) {
    const auto quest=frame.state==2 && frame.quest>0?frame.quest:0;
    if(generation_!=frame.generation || quest_!=quest || !frame.generation ||
       (frame.state!=0 && frame.state!=2)) {
        monsters_={};generation_=frame.generation;quest_=quest;
    }
    for(auto& entry:monsters_) {
        if(!entry.row.instance)continue;
        const bool missing=std::none_of(rows.begin(),rows.end(),
            [&](const IcebornCrownRow& row){return sameMonster(entry.row,row);});
        if(missing && entry.missing)entry={};
        else entry.missing=missing;
    }
    for(const auto& row:rows) {
        if(!row.instance || !row.component || row.kind==ICEBORN_CROWN_NONE ||
           row.kind>ICEBORN_CROWN_LARGE_SILVER || !crownMonsterName(row.species))continue;
        if(std::any_of(monsters_.begin(),monsters_.end(),
            [&](const Monster& entry){return sameMonster(entry.row,row);}))continue;
        const auto free=std::find_if(monsters_.begin(),monsters_.end(),
            [](const Monster& entry){return !entry.row.instance;});
        if(free!=monsters_.end())*free={row,now,false};
    }
}
void CrownFeed::show(Scene& scene, const CrownSettings& value, uint64_t now) const {
    scene.crownCount=0;
    if(!value.visible)return;
    for(const auto& entry:monsters_) {
        if(entry.row.instance && !entry.missing && crownEnabled(entry.row.kind,value) &&
           (value.fixed || now-entry.appeared<static_cast<uint64_t>(value.seconds)*1000)) {
            scene.crowns[scene.crownCount++]={entry.row.species,entry.row.kind};
            if(scene.crownCount==scene.crowns.size())break;
        }
    }
}
}
