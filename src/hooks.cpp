#include "runtime.hpp"
#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include "dxgi_bridge.hpp"
#include "process_com_ptr.hpp"
namespace meter {
namespace {
using Microsoft::WRL::ComPtr;
struct Hook {
    void** slot = nullptr; void* original = nullptr; void* detour = nullptr;
};
std::array<Hook, 128> hooks;
std::mutex hookMutex;
std::atomic<bool> captured{false};
struct QueueCandidate {
    ProcessComPtr<ID3D12CommandQueue> queue;
    ProcessComPtr<IUnknown> device;
    unsigned submissions = 0;
};
std::array<QueueCandidate, 8> candidates;
std::mutex queueMutex;
// Keep the bootstrap device alive until process exit: its runtime owns the
// shared vtables patched below. The bootstrap has no worker or per-frame work.
ProcessComPtr<ID3D12Device> bootstrapDevice;
ProcessComPtr<ID3D12CommandQueue> bootstrapQueue;
ProcessComPtr<ID3D11Device> bootstrapDevice11;
ProcessComPtr<ID3D11DeviceContext> bootstrapContext11;
void capture(IDXGISwapChain* chain, IUnknown* device);
void discover(IDXGISwapChain* chain) {
    if (captured.load()) return;
    ComPtr<ID3D11Device> device11;
    if(SUCCEEDED(chain->GetDevice(IID_PPV_ARGS(&device11)))) {capture(chain,device11.Get());return;}
    ComPtr<IUnknown> identity;
    if (FAILED(chain->GetDevice(IID_PPV_ARGS(&identity)))) return;
    ComPtr<ID3D12CommandQueue> selected;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        unsigned best = 0;
        for (const auto& candidate : candidates)
            if (candidate.device.Get() == identity.Get() && candidate.submissions > best) {
                best = candidate.submissions;
                selected = candidate.queue;
            }
    }
    if (selected) capture(chain, selected.Get());
}
template<class F> F original(void* object, size_t index) {
    if(void* incoming=incomingDxgiOriginal(index))return reinterpret_cast<F>(incoming);
    auto table = *static_cast<void***>(object);
    std::lock_guard<std::mutex> lock(hookMutex);
    for (const auto& h : hooks) if (h.slot == table + index) return reinterpret_cast<F>(h.original);
    return nullptr;
}
// DirectInput/import slots only. DXGI uses per-target code trampolines below.
bool patchSlot(void** slot, void* replacement) {
    std::lock_guard<std::mutex> lock(hookMutex);
    for (const auto& h : hooks) if (h.slot == slot) return h.detour == replacement;
    Hook* empty = nullptr;
    for (auto& h : hooks) if (!h.slot) { empty = &h; break; }
    if (!empty) return false;
    void* previous=*slot;
    if(previous==replacement)return false;
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) return false;
    *empty = {};
    empty->slot=slot;empty->original=previous;empty->detour=replacement;
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot), replacement);
    DWORD ignored;
    VirtualProtect(slot, sizeof(void*), protection, &ignored);
    return true;
}
bool patch(void* object, size_t index, void* replacement);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using ResizeFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using Resize1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
thread_local bool insidePresent = false;
HRESULT STDMETHODCALLTYPE present(IDXGISwapChain* chain, UINT interval, UINT flags) {
    auto next = original<PresentFn>(chain, 8);
    if (!next) return DXGI_ERROR_INVALID_CALL;
    const bool outer = !insidePresent;
    if (outer) {
        insidePresent = true;
        try { if (!stopping.load() && !(flags & DXGI_PRESENT_TEST)) { discover(chain); drawOverlay(chain, flags); } } catch (...) { log("Overlay exception; skipping frame."); }
    }
    const HRESULT result = next(chain, interval, flags);
    if(outer&&FAILED(result)){try{observePresentResult(chain,result);}catch(...){}}
    if (outer) insidePresent = false;
    return result;
}
HRESULT STDMETHODCALLTYPE present1(IDXGISwapChain1* chain, UINT interval, UINT flags, const DXGI_PRESENT_PARAMETERS* params) {
    auto next = original<Present1Fn>(chain, 22);
    if (!next) return DXGI_ERROR_INVALID_CALL;
    const bool outer = !insidePresent;
    if (outer) {
        insidePresent = true;
        try { if (!stopping.load() && !(flags & DXGI_PRESENT_TEST)) { discover(chain); drawOverlay(chain, flags); } } catch (...) { log("Overlay exception; skipping frame."); }
    }
    const HRESULT result = next(chain, interval, flags, params);
    if(outer&&FAILED(result)){try{observePresentResult(chain,result);}catch(...){}}
    if (outer) insidePresent = false;
    return result;
}
HRESULT STDMETHODCALLTYPE resize(IDXGISwapChain* chain, UINT count, UINT w, UINT h, DXGI_FORMAT format, UINT flags) {
    auto next = original<ResizeFn>(chain, 13);
    if (!next) return DXGI_ERROR_INVALID_CALL;
    try { beforeResize(chain); } catch (...) { log("Resize cleanup failed."); }
    auto result = next(chain, count, w, h, format, flags);
    try { afterResize(chain); } catch (...) { log("Resize recovery failed."); }
    return result;
}
HRESULT STDMETHODCALLTYPE resize1(IDXGISwapChain3* chain, UINT count, UINT w, UINT h, DXGI_FORMAT format, UINT flags,
    const UINT* masks, IUnknown* const* queues) {
    auto next = original<Resize1Fn>(chain, 39);
    if (!next) return DXGI_ERROR_INVALID_CALL;
    try { beforeResize(chain); } catch (...) { log("Resize cleanup failed."); }
    auto result = next(chain, count, w, h, format, flags, masks, queues);
    try { afterResize(chain); } catch (...) { log("Resize recovery failed."); }
    return result;
}
void capture(IDXGISwapChain* chain, IUnknown* device) {
    if (!chain || !device || stopping.load()) return;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D11Device> device11;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&queue)))&&FAILED(device->QueryInterface(IID_PPV_ARGS(&device11)))) return;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(chain->GetDesc(&desc)) || !desc.OutputWindow || desc.BufferDesc.Width < 64 || desc.BufferDesc.Height < 64) return;
    DWORD pid = 0;
    GetWindowThreadProcessId(desc.OutputWindow, &pid);
    if (pid != GetCurrentProcessId()) return;
    if (!attachSwapchain(chain, device)) return;
    const auto basic = [&](void* object) {
        return patch(object, 8, reinterpret_cast<void*>(&present)) && patch(object, 13, reinterpret_cast<void*>(&resize));
    };
    bool ok = basic(chain);
    ComPtr<IDXGISwapChain1> one;
    if (SUCCEEDED(chain->QueryInterface(IID_PPV_ARGS(&one)))) {
        ok = basic(one.Get()) && ok;
        ok = patch(one.Get(), 22, reinterpret_cast<void*>(&present1)) && ok;
    }
    ComPtr<IDXGISwapChain3> three;
    if (SUCCEEDED(chain->QueryInterface(IID_PPV_ARGS(&three)))) {
        ok = basic(three.Get()) && ok;
        ok = patch(three.Get(), 22, reinterpret_cast<void*>(&present1)) && ok;
        if(!device11)ok = patch(three.Get(), 39, reinterpret_cast<void*>(&resize1)) && ok;
    }
    log("Captured %s swapchain %ux%u (%s)",device11?"DX11":"DX12", desc.BufferDesc.Width, desc.BufferDesc.Height, ok ? "hooks ready" : "hook failure");
    captured = true;
}
using ExecuteFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
void STDMETHODCALLTYPE execute(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    auto next = original<ExecuteFn>(queue, 10);
    if (!next) return;
    if (!captured.load() && !insidePresent && !stopping.load() && count &&
        queue != bootstrapQueue.Get() && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        try {
            std::lock_guard<std::mutex> lock(queueMutex);
            QueueCandidate* target = nullptr;
            for (auto& candidate : candidates) if (candidate.queue.Get() == queue) { target = &candidate; break; }
            if (!target) for (auto& candidate : candidates) if (!candidate.queue) { target = &candidate; break; }
            if (target) {
                if (!target->queue) {
                    ComPtr<ID3D12Device> device;
                    if (SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&device))) && SUCCEEDED(device.As(&target->device))) {
                        target->queue = queue;
                        log("Observed live DX12 direct queue (late attachment).");
                    }
                }
                if (target->queue) ++target->submissions;
            }
        } catch (...) { log("Queue discovery failed."); }
    }
    next(queue, count, lists);
}
bool bootstrapLateHooks11(IDXGIFactory2* factory) {
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &bootstrapDevice11,nullptr,&bootstrapContext11);
    if(FAILED(hr))hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &bootstrapDevice11,nullptr,&bootstrapContext11);
    if(FAILED(hr)){log("DX11 discovery device failed: 0x%08lx",static_cast<unsigned long>(hr));return false;}
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"IcebornSuiteDX11Bootstrap";
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    HWND window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,8,8,nullptr,nullptr,wc.hInstance,nullptr);
    if(!window)return false;
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=desc.BufferDesc.Height=8;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window;
    desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    ComPtr<IDXGISwapChain> dummy;hr=factory->CreateSwapChain(bootstrapDevice11.Get(),&desc,&dummy);
    const auto basic=[](void* object){return patch(object,8,reinterpret_cast<void*>(&present))&&patch(object,13,reinterpret_cast<void*>(&resize));};
    bool ok=SUCCEEDED(hr);
    if(ok) {
        ok=basic(dummy.Get());
        ComPtr<IDXGISwapChain1> one;if(SUCCEEDED(dummy.As(&one)))ok=basic(one.Get())&&patch(one.Get(),22,reinterpret_cast<void*>(&present1))&&ok;
        ComPtr<IDXGISwapChain3> three;if(SUCCEEDED(dummy.As(&three)))ok=basic(three.Get())&&
            patch(three.Get(),22,reinterpret_cast<void*>(&present1))&&ok;
    }
    // Flip-model D3D11 may expose another shared implementation table.
    dummy.Reset();
    DXGI_SWAP_CHAIN_DESC1 flip{};flip.Width=flip.Height=8;flip.Format=desc.BufferDesc.Format;
    flip.SampleDesc.Count=1;flip.BufferUsage=desc.BufferUsage;flip.BufferCount=2;flip.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> modern;
    if(SUCCEEDED(factory->CreateSwapChainForHwnd(bootstrapDevice11.Get(),window,&flip,nullptr,nullptr,&modern))) {
        ok=basic(modern.Get())&&patch(modern.Get(),22,reinterpret_cast<void*>(&present1))&&ok;
        ComPtr<IDXGISwapChain> base;if(SUCCEEDED(modern.As(&base)))ok=basic(base.Get())&&ok;
        ComPtr<IDXGISwapChain3> three;if(SUCCEEDED(modern.As(&three)))ok=basic(three.Get())&&
            patch(three.Get(),22,reinterpret_cast<void*>(&present1))&&ok;
    }
    modern.Reset();DestroyWindow(window);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    log("DX11 late-attachment Present hooks %s",ok?"installed":"failed");return ok;
}
bool bootstrapLateHooks(IDXGIFactory2* factory) {
    // Stracker may load plugins after the game's swapchain exists. A small
    // temporary swapchain exposes the same DXGI implementation's shared table;
    // hooking its Present reaches already-existing swapchains as well.
    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&bootstrapDevice));
    if (FAILED(hr)) { log("Late-attachment device failed: 0x%08lx", static_cast<unsigned long>(hr)); return false; }
    D3D12_COMMAND_QUEUE_DESC desc{};
    hr = bootstrapDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&bootstrapQueue));
    if (FAILED(hr)) return false;
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"IceborneDamageMeterBootstrap";
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    HWND window = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 8, 8,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!window) return false;
    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    swapDesc.Width = swapDesc.Height = 8;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> dummy;
    hr = factory->CreateSwapChainForHwnd(bootstrapQueue.Get(), window, &swapDesc, nullptr, nullptr, &dummy);
    bool ok = SUCCEEDED(hr);
    const auto basic = [](void* object) {
        return patch(object, 8, reinterpret_cast<void*>(&present)) && patch(object, 13, reinterpret_cast<void*>(&resize));
    };
    if (ok) {
        ok = basic(dummy.Get()) && patch(dummy.Get(), 22, reinterpret_cast<void*>(&present1));
        ComPtr<IDXGISwapChain3> three;
        if (SUCCEEDED(dummy.As(&three))) {
            ok = basic(three.Get()) && patch(three.Get(), 22, reinterpret_cast<void*>(&present1)) &&
                patch(three.Get(), 39, reinterpret_cast<void*>(&resize1)) && ok;
        }
        ComPtr<IDXGISwapChain> base;
        if (SUCCEEDED(dummy.As(&base))) ok = basic(base.Get()) && ok;
    }
    ok = patch(bootstrapQueue.Get(), 10, reinterpret_cast<void*>(&execute)) && ok;
    dummy.Reset();
    DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    log("Late-attachment Present/queue hooks %s", ok ? "installed" : "failed");
    return ok;
}
using CreateFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
using CreateHwndFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
HRESULT STDMETHODCALLTYPE create(IDXGIFactory* factory, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** result) {
    auto next = original<CreateFn>(factory, 10);
    if (!next) return DXGI_ERROR_INVALID_CALL;
    const auto hr = next(factory, device, desc, result);
    if (SUCCEEDED(hr) && result) try { capture(*result, device); } catch (...) { log("Swapchain capture failed."); }
    return hr;
}
HRESULT STDMETHODCALLTYPE createHwnd(IDXGIFactory2* factory, IUnknown* device, HWND window, const DXGI_SWAP_CHAIN_DESC1* desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen, IDXGIOutput* output, IDXGISwapChain1** result) {
    auto next = original<CreateHwndFn>(factory, 15);
    if (!next) return DXGI_ERROR_INVALID_CALL;
    const auto hr = next(factory, device, window, desc, fullscreen, output, result);
    if (SUCCEEDED(hr) && result) try { capture(*result, device); } catch (...) { log("Swapchain capture failed."); }
    return hr;
}
bool patch(void* object,size_t index,void* replacement) {
    DxgiMethod method;
    if(replacement==reinterpret_cast<void*>(&present))method=DxgiMethod::Present;
    else if(replacement==reinterpret_cast<void*>(&present1))method=DxgiMethod::Present1;
    else if(replacement==reinterpret_cast<void*>(&resize))method=DxgiMethod::Resize;
    else if(replacement==reinterpret_cast<void*>(&resize1))method=DxgiMethod::Resize1;
    else if(replacement==reinterpret_cast<void*>(&execute))method=DxgiMethod::Execute;
    else if(replacement==reinterpret_cast<void*>(&create))method=DxgiMethod::Create;
    else if(replacement==reinterpret_cast<void*>(&createHwnd))method=DxgiMethod::CreateHwnd;
    else return false;
    return bindDxgiMethod(object,index,replacement,method);
}
}
bool hookSlot(void** slot, void* replacement) { return patchSlot(slot, replacement); }
void* originalHook(void* object, size_t index) { return original<void*>(object, index); }
#ifdef METER_DIAGNOSTIC
void diagnosticRecapture(void* chain,IUnknown* device){capture(static_cast<IDXGISwapChain*>(chain),device);}
#endif
bool installHooks() {
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
    const auto install = [](void* f) {
        return patch(f, 10, reinterpret_cast<void*>(&create)) && patch(f, 15, reinterpret_cast<void*>(&createHwnd));
    };
    bool ok = install(factory.Get());
    ComPtr<IDXGIFactory4> four;
    if (SUCCEEDED(factory.As(&four))) ok = install(four.Get()) && ok;
    ComPtr<IDXGIFactory6> six;
    if (SUCCEEDED(factory.As(&six))) ok = install(six.Get()) && ok;
    // The base interface may have a distinct table; it only exposes CreateSwapChain.
    ComPtr<IDXGIFactory> basic;
    if (SUCCEEDED(factory.As(&basic))) ok = patch(basic.Get(), 10, reinterpret_cast<void*>(&create)) && ok;
    ComPtr<IDXGIFactory1> one;
    if (SUCCEEDED(factory.As(&one))) ok = patch(one.Get(), 10, reinterpret_cast<void*>(&create)) && ok;
    log("DXGI factory hooks %s", ok ? "installed" : "failed");
    const bool late11 = bootstrapLateHooks11(factory.Get());
    const bool late12 = false; // DX11 field cohort
    // A machine without DX12 must still be able to render the DX11 meter.
    return ok && (late11||late12);
}
void removeHooks() {
    disableDxgiMethods();
    std::lock_guard<std::mutex> lock(hookMutex);
    for (auto& h : hooks) if (h.slot) {
        DWORD old;
        if (VirtualProtect(h.slot, sizeof(void*), PAGE_READWRITE, &old)) {
            InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(h.slot), h.original, h.detour);
            DWORD ignored;
            VirtualProtect(h.slot, sizeof(void*), old, &ignored);
        }
    }
    // Retain records: an already-entered callback still needs its original.
}
}
