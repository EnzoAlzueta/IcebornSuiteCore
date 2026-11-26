// Adapted from Iceborne Damage Meter 0.6.17 (MIT), src/input.cpp.
#include "runtime.hpp"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl/client.h>
#include <cstring>
#include <vector>
#include "relative_cursor.hpp"
#include "process_com_ptr.hpp"
namespace meter {
namespace {
using Microsoft::WRL::ComPtr;
std::atomic<bool> editing{false};
HWND inputWindow = nullptr;
std::atomic<WNDPROC> previousProc{nullptr};
bool initialized = false;
RECT hotspot{};
std::mutex hotspotMutex;
int cursorDecrements = 0;
constexpr UINT EditMessage = WM_APP + 0x5ad;
constexpr UINT_PTR EditTimer = 0x49444d43;
RelativeCursor editPointer;
std::mutex pointerMutex;
std::atomic<bool> rawActive{false};
bool savedMouse = false;
RAWINPUTDEVICE originalMouse{};
RECT previousClip{};
bool clipSaved = false;
ProcessComPtr<IDirectInput8W> directInput;
ProcessComPtr<IDirectInputDevice8W> mouseDevice, keyboardDevice;
ProcessComPtr<IDirectInput8A> directInputAnsi;
ProcessComPtr<IDirectInputDevice8A> mouseAnsi, keyboardAnsi;
decltype(&SetCursorPos) nextPosition = &SetCursorPos;
decltype(&ClipCursor) nextClip = &ClipCursor;
decltype(&ShowCursor) nextShow = &ShowCursor;
decltype(&SetCursor) nextCursor = &SetCursor;
using StateFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*, DWORD, LPVOID);
using DataFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
bool overButton() {
    if (GetForegroundWindow() != inputWindow) return false;
    CURSORINFO info{sizeof(CURSORINFO)};
    POINT point{};
    if (!GetCursorInfo(&info) || !(info.flags & CURSOR_SHOWING) || !GetCursorPos(&point)) return false;
    ScreenToClient(inputWindow, &point);
    std::lock_guard<std::mutex> lock(hotspotMutex);
    return PtInRect(&hotspot, point) != 0;
}
void relativeMotion(LONG dx, LONG dy) {
    std::lock_guard<std::mutex> lock(pointerMutex);
    editPointer.move(static_cast<float>(dx), static_cast<float>(dy));
}
void pointerButton(bool down) {
    std::lock_guard<std::mutex> lock(pointerMutex);
    editPointer.button(down);
}
HRESULT STDMETHODCALLTYPE state(IDirectInputDevice8W* object, DWORD bytes, LPVOID data) {
    const auto next = reinterpret_cast<StateFn>(originalHook(object, 9));
    if (!next) return DIERR_NOTINITIALIZED;
    const HRESULT hr = next(object, bytes, data);
    if (SUCCEEDED(hr) && data && editing.load() && !rawActive.load() &&
        (bytes == sizeof(DIMOUSESTATE) || bytes == sizeof(DIMOUSESTATE2))) {
        const auto mouse = static_cast<const DIMOUSESTATE*>(data);
        relativeMotion(mouse->lX, mouse->lY);
        pointerButton((mouse->rgbButtons[0] & 0x80) != 0);
    }
    if (SUCCEEDED(hr) && data && (editing.load() || (bytes <= sizeof(DIMOUSESTATE2) && overButton()))) std::memset(data, 0, bytes);
    return hr;
}
HRESULT STDMETHODCALLTYPE buffered(IDirectInputDevice8W* object, DWORD size, LPDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags) {
    const auto next = reinterpret_cast<DataFn>(originalHook(object, 10));
    if (!next) return DIERR_NOTINITIALIZED;
    const HRESULT hr = next(object, size, data, count, flags);
    if (SUCCEEDED(hr) && data && count && editing.load() && !rawActive.load() && size >= 16 && !(flags & DIGDD_PEEK)) {
        DIDEVCAPS caps{sizeof(DIDEVCAPS)};
        if (SUCCEEDED(object->GetCapabilities(&caps)) && GET_DIDEVICE_TYPE(caps.dwDevType) == DI8DEVTYPE_MOUSE) {
            for (DWORD i = 0; i < *count; ++i) {
                const auto event = reinterpret_cast<const DIDEVICEOBJECTDATA*>(reinterpret_cast<const BYTE*>(data) + i*size);
                if (event->dwOfs == DIMOFS_X) relativeMotion(static_cast<LONG>(event->dwData), 0);
                if (event->dwOfs == DIMOFS_Y) relativeMotion(0, static_cast<LONG>(event->dwData));
                if (event->dwOfs == DIMOFS_BUTTON0) pointerButton((event->dwData & 0x80) != 0);
            }
        }
    }
    if ((editing.load() || overButton()) && count) *count = 0;
    return hr;
}
BOOL WINAPI cursorPosition(int x, int y) {
    if (editing.load()) return TRUE;
    return nextPosition(x, y);
}
BOOL WINAPI cursorClip(const RECT* rect) {
    if (editing.load()) return TRUE;
    return nextClip(rect);
}
int WINAPI cursorShow(BOOL show) {
    if (editing.load()) return show ? 0 : -1;
    return nextShow(show);
}
HCURSOR WINAPI cursorSet(HCURSOR cursor) {
    if (editing.load()) return GetCursor();
    return nextCursor(cursor);
}
void beginRawMouse() {
    UINT count = 0;
    if (GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) == static_cast<UINT>(-1)) {
        log("Cannot snapshot Raw Input registrations: %lu", GetLastError()); return;
    }
    std::vector<RAWINPUTDEVICE> devices(count);
    if (count && GetRegisteredRawInputDevices(devices.data(), &count, sizeof(RAWINPUTDEVICE)) == static_cast<UINT>(-1)) {
        log("Cannot read Raw Input registrations: %lu", GetLastError()); return;
    }
    savedMouse = false;
    for (const auto& device : devices) if (device.usUsagePage == 1 && device.usUsage == 2) {
        originalMouse = device; savedMouse = true; break;
    }
    // This registration exists only while editing. Restore the game's exact
    // mouse target/flags afterward; do not touch keyboard or controller inputs.
    const RAWINPUTDEVICE mouse{1, 2, RIDEV_INPUTSINK, inputWindow};
    if (RegisterRawInputDevices(&mouse, 1, sizeof(mouse))) {
        rawActive = true;
        log("Editing: relative Raw Input pointer active; previous mouse target %s.", savedMouse ? "saved" : "absent");
    } else log("Editing: Raw Input failed (%lu); DirectInput fallback.", GetLastError());
}
void endRawMouse() {
    if (!rawActive.exchange(false)) return;
    const RAWINPUTDEVICE remove{1, 2, RIDEV_REMOVE, nullptr};
    const auto device = savedMouse ? originalMouse : remove;
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) log("Cannot restore mouse Raw Input registration: %lu", GetLastError());
    else log("Editing: mouse Raw Input registration restored.");
}
void consumeRawMouse(HRAWINPUT handle) {
    RAWINPUT input{};
    UINT bytes = sizeof(input);
    const auto read = GetRawInputData(handle, RID_INPUT, &input, &bytes, sizeof(RAWINPUTHEADER));
    if (read == static_cast<UINT>(-1) || input.header.dwType != RIM_TYPEMOUSE) return;
    const auto& mouse = input.data.mouse;
    if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
        const bool desktop = (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
        const int w = GetSystemMetrics(desktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
        const int h = GetSystemMetrics(desktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
        POINT point{static_cast<LONG>(mouse.lLastX * static_cast<double>(w) / 65535),
            static_cast<LONG>(mouse.lLastY * static_cast<double>(h) / 65535)};
        if (desktop) { point.x += GetSystemMetrics(SM_XVIRTUALSCREEN); point.y += GetSystemMetrics(SM_YVIRTUALSCREEN); }
        ScreenToClient(inputWindow, &point);
        std::lock_guard<std::mutex> lock(pointerMutex);
        editPointer.absolute(static_cast<float>(point.x), static_cast<float>(point.y));
    } else relativeMotion(mouse.lLastX, mouse.lLastY);
    if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) pointerButton(true);
    if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) pointerButton(false);
}
void containHiddenCursor() {
    if (GetForegroundWindow() != inputWindow) return;
    RECT bounds{};
    if (!GetClientRect(inputWindow, &bounds)) return;
    MapWindowPoints(inputWindow, nullptr, reinterpret_cast<POINT*>(&bounds), 2);
    ClipCursor(&bounds);
}
void showEditCursor() {
    if (editing.load()) {
        if (!rawActive.load()) beginRawMouse();
        if (!clipSaved && GetForegroundWindow() == inputWindow) clipSaved = GetClipCursor(&previousClip) != 0;
        containHiddenCursor();
        if (!cursorDecrements) {
            int counter;
            do { counter = ShowCursor(FALSE); ++cursorDecrements; } while (counter >= 0 && cursorDecrements < 32);
        }
        SetCursor(nullptr);
        SetTimer(inputWindow, EditTimer, 16, nullptr);
    } else {
        KillTimer(inputWindow, EditTimer);
        endRawMouse();
        if (clipSaved && GetForegroundWindow() == inputWindow) ClipCursor(&previousClip);
        else ClipCursor(nullptr);
        clipSaved = false;
        while (cursorDecrements > 0) { ShowCursor(TRUE); --cursorDecrements; }
    }
}
LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == EditMessage) { showEditCursor(); return 0; }
    if (message == WM_KILLFOCUS || (message == WM_ACTIVATEAPP && !wp) || message == WM_DESTROY) {
        setEditing(false); showEditCursor();
    }
    if (message == WM_TIMER && wp == EditTimer) {
        if (editing.load()) { containHiddenCursor(); SetCursor(nullptr); }
        else showEditCursor();
        return 0;
    }
    if (editing.load()) {
        if (message == WM_SETCURSOR) { SetCursor(nullptr); return TRUE; }
        if (message == WM_INPUT) {
            if (rawActive.load()) consumeRawMouse(reinterpret_cast<HRAWINPUT>(lp));
            return DefWindowProcW(window, message, wp, lp);
        }
        if ((message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) ||
            (message >= WM_KEYFIRST && message <= WM_KEYLAST)) return 0;
    }
    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) {
        CURSORINFO info{sizeof(CURSORINFO)};
        POINT point{};
        if (GetCursorInfo(&info) && (info.flags & CURSOR_SHOWING) && GetCursorPos(&point)) {
            ScreenToClient(window, &point);
            std::lock_guard<std::mutex> lock(hotspotMutex);
            if (PtInRect(&hotspot, point)) return 0;
        }
    }
    const auto next = previousProc.load();
    return next ? CallWindowProcW(next, window, message, wp, lp) : DefWindowProcW(window, message, wp, lp);
}
unsigned cursorImports() {
    const auto base = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress) return 0;
    unsigned patched = 0;
    for (auto descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); descriptor->Name; ++descriptor) {
        if (!descriptor->OriginalFirstThunk) continue;
        auto name = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->OriginalFirstThunk);
        auto slot = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->FirstThunk);
        for (; name->u1.AddressOfData; ++name, ++slot) {
            if (IMAGE_SNAP_BY_ORDINAL64(name->u1.Ordinal)) continue;
            const auto import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + name->u1.AddressOfData);
            void* detour = nullptr;
            if (!std::strcmp(import->Name, "SetCursorPos")) detour = reinterpret_cast<void*>(&cursorPosition);
            if (!std::strcmp(import->Name, "ClipCursor")) detour = reinterpret_cast<void*>(&cursorClip);
            if (!std::strcmp(import->Name, "ShowCursor")) detour = reinterpret_cast<void*>(&cursorShow);
            if (!std::strcmp(import->Name, "SetCursor")) detour = reinterpret_cast<void*>(&cursorSet);
            if (detour) {
                auto target = reinterpret_cast<void**>(&slot->u1.Function);
                if (*target != detour) {
                    if (detour == reinterpret_cast<void*>(&cursorPosition)) nextPosition = reinterpret_cast<decltype(nextPosition)>(*target);
                    if (detour == reinterpret_cast<void*>(&cursorClip)) nextClip = reinterpret_cast<decltype(nextClip)>(*target);
                    if (detour == reinterpret_cast<void*>(&cursorShow)) nextShow = reinterpret_cast<decltype(nextShow)>(*target);
                    if (detour == reinterpret_cast<void*>(&cursorSet)) nextCursor = reinterpret_cast<decltype(nextCursor)>(*target);
                }
                if (hookSlot(target, detour)) ++patched;
            }
        }
    }
    return patched;
}
}
bool installInput(HWND window) {
    if (initialized) return true;
    if (!window) return false;
    if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
        IID_IDirectInput8W, reinterpret_cast<void**>(directInput.GetAddressOf()), nullptr))) return false;
    if (FAILED(directInput->CreateDevice(GUID_SysMouse, &mouseDevice, nullptr)) ||
        FAILED(directInput->CreateDevice(GUID_SysKeyboard, &keyboardDevice, nullptr))) return false;
    for (auto device : {mouseDevice.Get(), keyboardDevice.Get()}) {
        const auto table = *reinterpret_cast<void***>(device);
        if (!hookSlot(table + 9, reinterpret_cast<void*>(&state)) || !hookSlot(table + 10, reinterpret_cast<void*>(&buffered))) return false;
    }
    // MT Framework and other mods may request ANSI or Unicode DirectInput.
    // State/data slots have the same ABI; cover both implementations.
    if (SUCCEEDED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
        IID_IDirectInput8A, reinterpret_cast<void**>(directInputAnsi.GetAddressOf()), nullptr))) {
        directInputAnsi->CreateDevice(GUID_SysMouse, &mouseAnsi, nullptr);
        directInputAnsi->CreateDevice(GUID_SysKeyboard, &keyboardAnsi, nullptr);
        for (auto device : {mouseAnsi.Get(), keyboardAnsi.Get()}) if (device) {
            const auto table = *reinterpret_cast<void***>(device);
            if (!hookSlot(table + 9, reinterpret_cast<void*>(&state)) || !hookSlot(table + 10, reinterpret_cast<void*>(&buffered))) return false;
        }
    }
    inputWindow = window;
    SetLastError(0);
    previousProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC));
    const auto actualPrevious = SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windowProc));
    if (!actualPrevious) { log("Input subclass failed: %lu", GetLastError()); return false; }
    previousProc = reinterpret_cast<WNDPROC>(actualPrevious);
    initialized = true;
    const auto imports = cursorImports();
    log("Edit input ready; %u cursor imports hooked.", imports);
    return true;
}
void setEditing(bool enabled) {
    if (enabled && !editing.load() && inputWindow) {
        POINT cursor{}; RECT client{};
        GetCursorPos(&cursor); ScreenToClient(inputWindow, &cursor); GetClientRect(inputWindow, &client);
        std::lock_guard<std::mutex> lock(pointerMutex);
        editPointer.reset(static_cast<float>(cursor.x), static_cast<float>(cursor.y),
            static_cast<float>(client.right), static_cast<float>(client.bottom));
        editPointer.button((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    }
    const bool previous = editing.exchange(enabled);
    if (previous != enabled && inputWindow) {
        log("Editing %s", enabled ? "enabled" : "disabled");
        PostMessageW(inputWindow, EditMessage, 0, 0);
    }
}
bool isEditing() { return editing.load(); }
void setInputHotspot(const RECT& rect) {
    std::lock_guard<std::mutex> lock(hotspotMutex);
    hotspot = rect;
}
bool getEditPointer(POINT& point, bool& down) {
    if (!editing.load()) return false;
    std::lock_guard<std::mutex> lock(pointerMutex);
    point = {static_cast<LONG>(editPointer.x()), static_cast<LONG>(editPointer.y())};
    down = editPointer.left();
    return true;
}
#ifdef METER_DIAGNOSTIC
void diagnosticRelativeMotion(LONG dx, LONG dy) { relativeMotion(dx, dy); }
#endif
}
