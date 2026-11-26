#pragma once
#include <stdint.h>
// Windows x64, private development ABI. Providers are process-owned and pinned.
// UI calls are serialized outside render callbacks. Values are absolute integers.
#define ICEBORN_SETTINGS_ABI 0x10001u
struct IcebornSetting {
    const wchar_t* label;
    int32_t minimum, maximum, step;
};
struct IcebornSettings {
    uint32_t size, abi;
    const wchar_t* title;
    uint32_t count;
    const IcebornSetting* options;
    // Read returns zero until ready. The first two fields are HUD position (0..100).
    int (*read)(int32_t* values, uint32_t count);
    int (*set)(uint32_t index, int32_t value);
    int (*move)(int32_t x, int32_t y);
};
using IcebornOpenSettings = void (*)(void* gameWindow, uint32_t shortcut);
using IcebornConnectSettings = const IcebornSettings* (*)(uint32_t abi, IcebornOpenSettings open);


