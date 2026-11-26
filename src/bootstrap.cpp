// Original MIT-licensed bootstrap. Starts the shared suite adapter.
#include <windows.h>
#include <Xinput.h>
#include <string>
#include <cstdio>

namespace {
HMODULE self = nullptr;
INIT_ONCE systemOnce = INIT_ONCE_STATIC_INIT;
using StateFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
using SetFn = DWORD (WINAPI*)(DWORD, XINPUT_VIBRATION*);
using CapsFn = DWORD (WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);
using AudioFn = DWORD (WINAPI*)(DWORD, GUID*, GUID*);
StateFn stateFn = nullptr;
SetFn setFn = nullptr;
CapsFn capsFn = nullptr;
AudioFn audioFn = nullptr;

BOOL CALLBACK loadSystem(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t system[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(system, MAX_PATH);
    if (!length || length >= MAX_PATH) return TRUE;
    // Absolute system path: never resolve our own proxy or a DLL in the CWD.
    if (wcscat_s(system, L"\\xinput9_1_0.dll") != 0) return TRUE;
    const HMODULE real = LoadLibraryExW(system, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (real) {
        stateFn = reinterpret_cast<StateFn>(GetProcAddress(real, "XInputGetState"));
        setFn = reinterpret_cast<SetFn>(GetProcAddress(real, "XInputSetState"));
        capsFn = reinterpret_cast<CapsFn>(GetProcAddress(real, "XInputGetCapabilities"));
        audioFn = reinterpret_cast<AudioFn>(GetProcAddress(real, "XInputGetDSoundAudioDeviceGuids"));
    }
    // The OS owns both XInput modules until process exit; do not unload them.
    return TRUE;
}
void systemReady() { InitOnceExecuteOnce(&systemOnce, loadSystem, nullptr, nullptr); }

DWORD WINAPI start(void*) {
    try {
        wchar_t host[32768]{}, location[32768]{};
        const DWORD hostLength = GetModuleFileNameW(nullptr, host, 32768);
        const DWORD ownLength = GetModuleFileNameW(self, location, 32768);
        if (!hostLength || hostLength >= 32768 || !ownLength || ownLength >= 32768) return 0;
        const std::wstring executable(host, hostLength);
        const size_t separator = executable.find_last_of(L"\\/");
        if (separator == std::wstring::npos ||
            _wcsicmp(executable.c_str() + separator + 1, L"MonsterHunterWorld.exe") != 0) return 0;
        const std::wstring ownPath(location, ownLength);
        const auto end = ownPath.find_last_of(L"\\/");
        if (end == std::wstring::npos) return 0;
        const std::wstring root = ownPath.substr(0, end + 1);
        // Prevent an accidentally misplaced proxy from loading plugins elsewhere.
        if (_wcsicmp(root.c_str(), executable.substr(0, separator + 1).c_str()) != 0) return 0;
        const std::wstring plugin = root + L"nativePC\\plugins\\iceborn_suite_adapter.dll";
        const HMODULE loaded = LoadLibraryExW(plugin.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        const DWORD error = loaded ? ERROR_SUCCESS : GetLastError();
        FILE* log = nullptr;
        if (_wfopen_s(&log, (root + L"iceborn_suite_bootstrap.log").c_str(), L"wb") == 0 && log) {
            fprintf(log, "Iceborn Suite bootstrap: %s (Win32 error %lu)\n",
                loaded ? "plugin loaded" : "plugin could not be loaded", error);
            fclose(log);
        }
    } catch (...) { /* A failed optional mod must not stop XInput forwarding. */ }
    // No resident polling thread, timers, hooks or per-frame bootstrap work.
    return 0;
}
}

extern "C" DWORD WINAPI IdmGetState(DWORD index, XINPUT_STATE* state) {
    systemReady(); return stateFn ? stateFn(index, state) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI IdmSetState(DWORD index, XINPUT_VIBRATION* vibration) {
    systemReady(); return setFn ? setFn(index, vibration) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI IdmGetCapabilities(DWORD index, DWORD flags, XINPUT_CAPABILITIES* caps) {
    systemReady(); return capsFn ? capsFn(index, flags, caps) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI IdmGetAudioGuids(DWORD index, GUID* render, GUID* capture) {
    systemReady(); return audioFn ? audioFn(index, render, capture) : ERROR_DEVICE_NOT_CONNECTED;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        self = instance;
        // LoadLibrary and I/O run after the loader lock is released. Never wait.
        const HANDLE thread = CreateThread(nullptr, 0, start, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
