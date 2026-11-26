#include "dxgi_bridge.hpp"
#include "runtime.hpp"
#include <dxgi1_4.h>
#include <d3d12.h>
#include <MinHook.h>
#include <utility>

namespace meter {
namespace {
constexpr size_t Capacity=48;
struct Entry {void* target=nullptr;void* callback=nullptr;void* trampoline=nullptr;size_t index=0;std::atomic<bool> ready{false};};
std::array<Entry,Capacity> entries;
std::mutex bindMutex;
thread_local const Entry* incoming=nullptr;
struct Scope {
    const Entry* previous;
    explicit Scope(size_t slot):previous(incoming){incoming=entries[slot].ready.load(std::memory_order_acquire)?&entries[slot]:nullptr;}
    ~Scope(){incoming=previous;}
};
template<size_t I> HRESULT STDMETHODCALLTYPE p(IDXGISwapChain* c,UINT a,UINT b){Scope s(I);return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT)>(entries[I].callback)(c,a,b);}
template<size_t I> HRESULT STDMETHODCALLTYPE p1(IDXGISwapChain1* c,UINT a,UINT b,const DXGI_PRESENT_PARAMETERS* d){Scope s(I);return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*)>(entries[I].callback)(c,a,b,d);}
template<size_t I> HRESULT STDMETHODCALLTYPE r(IDXGISwapChain* c,UINT a,UINT b,UINT d,DXGI_FORMAT f,UINT g){Scope s(I);return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT)>(entries[I].callback)(c,a,b,d,f,g);}
template<size_t I> HRESULT STDMETHODCALLTYPE r1(IDXGISwapChain3* c,UINT a,UINT b,UINT d,DXGI_FORMAT f,UINT g,const UINT* masks,IUnknown* const* queues){Scope s(I);return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,UINT,UINT,UINT,DXGI_FORMAT,UINT,const UINT*,IUnknown* const*)>(entries[I].callback)(c,a,b,d,f,g,masks,queues);}
template<size_t I> void STDMETHODCALLTYPE e(ID3D12CommandQueue* q,UINT n,ID3D12CommandList* const* lists){Scope s(I);reinterpret_cast<void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*,UINT,ID3D12CommandList* const*)>(entries[I].callback)(q,n,lists);}
template<size_t I> HRESULT STDMETHODCALLTYPE c(IDXGIFactory* f,IUnknown* d,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** out){Scope s(I);return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**)>(entries[I].callback)(f,d,desc,out);}
template<size_t I> HRESULT STDMETHODCALLTYPE ch(IDXGIFactory2* f,IUnknown* d,HWND w,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fs,IDXGIOutput* output,IDXGISwapChain1** out){Scope s(I);return reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**)>(entries[I].callback)(f,d,w,desc,fs,output,out);}
template<size_t... I> auto thunks(DxgiMethod method,std::index_sequence<I...>) {
    switch(method) {
    case DxgiMethod::Present:return std::array<void*,sizeof...(I)>{reinterpret_cast<void*>(&p<I>)...};
    case DxgiMethod::Present1:return std::array<void*,sizeof...(I)>{reinterpret_cast<void*>(&p1<I>)...};
    case DxgiMethod::Resize:return std::array<void*,sizeof...(I)>{reinterpret_cast<void*>(&r<I>)...};
    case DxgiMethod::Resize1:return std::array<void*,sizeof...(I)>{reinterpret_cast<void*>(&r1<I>)...};
    case DxgiMethod::Execute:return std::array<void*,sizeof...(I)>{reinterpret_cast<void*>(&e<I>)...};
    case DxgiMethod::Create:return std::array<void*,sizeof...(I)>{reinterpret_cast<void*>(&c<I>)...};
    case DxgiMethod::CreateHwnd:return std::array<void*,sizeof...(I)>{reinterpret_cast<void*>(&ch<I>)...};
    }
    return std::array<void*,sizeof...(I)>{};
}
}
bool bindDxgiMethod(void* object,size_t index,void* callback,DxgiMethod method) {
    void* target=(*static_cast<void***>(object))[index];
    std::lock_guard<std::mutex> lock(bindMutex);
    for(const auto& entry:entries)if(entry.target==target)return entry.callback==callback&&entry.ready.load();
    size_t slot=0;while(slot<Capacity&&entries[slot].target)++slot;
    if(slot==Capacity)return false;
    const auto init=MH_Initialize();if(init!=MH_OK&&init!=MH_ERROR_ALREADY_INITIALIZED)return false;
    auto& entry=entries[slot];void* trampoline=nullptr;
    const auto callbacks=thunks(method,std::make_index_sequence<Capacity>{});
    const auto created=MH_CreateHook(target,callbacks[slot],&trampoline);
    if(created!=MH_OK){log("DXGI trampoline creation failed (%zu): %s",index,MH_StatusToString(created));return false;}
    entry.target=target;entry.callback=callback;entry.trampoline=trampoline;entry.index=index;
    entry.ready.store(true,std::memory_order_release);
    const auto enabled=MH_EnableHook(target);
    if(enabled!=MH_OK){log("DXGI trampoline activation failed (%zu): %s",index,MH_StatusToString(enabled));return false;}
    return true;
}
void* incomingDxgiOriginal(size_t index){return incoming&&incoming->index==index?incoming->trampoline:nullptr;}
void disableDxgiMethods(){
    std::lock_guard<std::mutex> lock(bindMutex);
    for(const auto& entry:entries)if(entry.target&&entry.ready.load())MH_DisableHook(entry.target);
    // Keep trampolines alive: an already-entered callback may still need one.
}
}
