#pragma once

#include "../timer-dev.1/iceborn_timer.h"

#define ICEBORN_HUD_TIMER_ABI 0x00020000u
#define ICEBORN_MAX_HUD_TIMERS 4u

enum IcebornHudTimerEffect { ICEBORN_DEMON_POWDER = 4 };
enum IcebornHudTimerAnchor {
    ICEBORN_BUFF_ICON = 0,
    ICEBORN_FIRST_MANTLE_SLOT = 1,
    ICEBORN_SECOND_MANTLE_SLOT = 2,
};

struct IcebornHudTimer {
    uint32_t effect, state, anchor;
    float remaining, duration;
};

struct IcebornHudIconBounds {
    uint64_t instance; // Zero means hidden or unresolved.
    float left, top, right, bottom;
};

struct IcebornHudBar {
    float left, top, width, height, fill;
    uint32_t expiring;
};

struct IcebornHudTimerModule {
    uint32_t size, abi, period_ms;
    uint32_t (*sample)(const IcebornFrame*, IcebornHudTimer*, uint32_t);
    // Bounds are in viewport pixels, paired with timer rows in the same order.
    uint32_t (*layout)(const IcebornHudTimer*, const IcebornHudIconBounds*, uint32_t,
                       float, IcebornHudBar*, uint32_t);
};

using IcebornQueryHudTimers = const IcebornHudTimerModule* (*)(uint32_t);
