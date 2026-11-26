#pragma once
#include "runtime.hpp"
#include <filesystem>
namespace meter {
using Modules = std::array<const IcebornPanelModule*,8>;
bool loadPanel(const std::filesystem::path& file, const char* expected, Modules& modules);
const IcebornTimerModule* loadTimers(const std::filesystem::path&);
bool monsterHealthModeAvailable();
bool monsterHealthAlways();
void setMonsterHealthAlways(bool);
Scene samplePanels(const Modules&, const IcebornFrame&, const IcebornTimerModule* = nullptr);
void publish(const Scene&);
}
