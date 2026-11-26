#pragma once
#include <stdint.h>
#define ICEBORN_PANEL_ABI 0x00010001u
#define ICEBORN_MAX_ROWS 128
#ifdef __cplusplus
extern "C" {
#endif
/* The verified host supplies one quest sample per tick. Module calls are
   serialized. All pointers and output storage are borrowed for one callback. */
typedef struct IcebornFrame {
    uint32_t size;
    int32_t profile, quest, state;
    uint64_t generation, image_base;
    void* context;
    int32_t (*read_exact)(void*, uint64_t, void*, uint32_t);
} IcebornFrame;
typedef struct IcebornRow {
    uint64_t instance;
    char label[64];
    float current, maximum;
} IcebornRow;
typedef struct IcebornPanelModule {
    uint32_t size, abi;
    char id[48];
    uint32_t period_ms;
    uint32_t (*sample)(const IcebornFrame*, IcebornRow*, uint32_t);
} IcebornPanelModule;
typedef const IcebornPanelModule* (*IcebornQuery)(uint32_t);
#ifdef __cplusplus
}
#endif
