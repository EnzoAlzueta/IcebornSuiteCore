#pragma once
#include "runtime.hpp"
#include <filesystem>
#include <span>

namespace meter {
struct CrownSettings {
    int x=2, y=100, seconds=12, visible=1, fixed=1, mini=1, gold=1, silver=0;
    int scale=100, opacity=100, backgroundPercent=92;
    bool operator==(const CrownSettings&) const = default;
};
CrownSettings crownSettings();
void setCrownSettings(const CrownSettings&);
const IcebornCrownModule* loadCrowns(const std::filesystem::path&);
const wchar_t* crownMonsterName(int32_t);

class CrownFeed {
public:
    void observe(const IcebornFrame&, std::span<const IcebornCrownRow>, uint64_t now);
    void show(Scene&, const CrownSettings&, uint64_t now) const;
private:
    struct Monster {
        IcebornCrownRow row{};
        uint64_t appeared{};
        bool missing{};
    };
    std::array<Monster, ICEBORN_MAX_CROWNS> monsters_{};
    uint64_t generation_{};
    int32_t quest_{};
};
}
