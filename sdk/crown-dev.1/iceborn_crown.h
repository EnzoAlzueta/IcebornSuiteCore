#pragma once

#include "../1.0.0-dev.1/iceborn_panel.h"

#define ICEBORN_CROWN_ABI 0x00010000u
#define ICEBORN_MAX_CROWNS 128u

enum IcebornCrownKind {
    ICEBORN_CROWN_NONE,
    ICEBORN_CROWN_MINI_GOLD,
    ICEBORN_CROWN_LARGE_GOLD,
    ICEBORN_CROWN_LARGE_SILVER,
};

struct IcebornCrownRow {
    uint64_t instance;
    uint64_t component;
    int32_t species;
    uint32_t kind;
};

struct IcebornCrownModule {
    uint32_t size, abi, period_ms;
    // A negative result indicates an incomplete read. No field changes/hides
    // should be inferred from it. A valid empty sample clears the current list.
    int32_t (*sample)(const IcebornFrame*, IcebornCrownRow*, uint32_t);
};

using IcebornQueryCrowns = const IcebornCrownModule* (*)(uint32_t);
