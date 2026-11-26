#pragma once
#include "../1.0.0-dev.1/iceborn_panel.h"

#define ICEBORN_TIMER_ABI 0x00010001u
#define ICEBORN_MAX_TIMERS 3u

#ifdef __cplusplus
extern "C" {
#endif
enum IcebornEffectId { ICEBORN_MIGHT_SEED=1, ICEBORN_ROCKSTEADY=2, ICEBORN_TEMPORAL=3 };
enum IcebornTimerState { ICEBORN_TIMER_ACTIVE=1, ICEBORN_TIMER_RECHARGING=2 };
typedef struct IcebornTimerRow {
    uint32_t effect, state, seconds; /* Zero seconds means active without a verified countdown. */
} IcebornTimerRow;
typedef struct IcebornTimerModule {
    uint32_t size, abi, period_ms;
    uint32_t (*sample)(const IcebornFrame*, IcebornTimerRow*, uint32_t);
} IcebornTimerModule;
typedef const IcebornTimerModule* (*IcebornQueryTimers)(uint32_t);
#ifdef __cplusplus
}
#endif
