#include "dx11_surface.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>

namespace meter {
namespace {
constexpr char shader[] = R"(
struct V { float4 position : SV_Position; float2 uv : TEXCOORD0; };
V vs(uint id : SV_VertexID) {
    V o; o.uv = float2((id << 1) & 2, id & 2);
    o.position = float4(o.uv * float2(2,-2) + float2(-1,1), 0, 1); return o;
}
Texture2D image : register(t0); SamplerState sampling : register(s0);
float4 ps(V input) : SV_Target { return image.Sample(sampling, input.uv); }
)";
}
HRESULT Dx11Surface::initialize(ID3D11Device* device, ID3D11Device** drawingDevice, ID3D11DeviceContext** drawingContext) {
    game=device;
    Ptr<ID3D11Device1> device1;
    HRESULT hr=game.As(&device1); if(FAILED(hr))return hr;
    Ptr<ID3D11DeviceContext> immediate;game->GetImmediateContext(&immediate);
    hr=immediate.As(&context);if(FAILED(hr))return hr;
    const auto level=game->GetFeatureLevel();
    const UINT flags=(game->GetCreationFlags()&D3D11_CREATE_DEVICE_SINGLETHREADED)?D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED:0;
    hr=device1->CreateDeviceContextState(flags,&level,1,D3D11_SDK_VERSION,__uuidof(ID3D11Device),nullptr,&state);
    if(FAILED(hr))return hr;
    Ptr<IDXGIDevice> dxgi;hr=game.As(&dxgi);if(FAILED(hr))return hr;
    Ptr<IDXGIAdapter> adapter;hr=dxgi->GetAdapter(&adapter);if(FAILED(hr))return hr;
    hr=D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&ui,nullptr,&uiContext);if(FAILED(hr))return hr;
    // Compile once at initialization, never from the frame path.
    Ptr<ID3DBlob> vs,ps,error;
    hr=D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"vs","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error);
    if(FAILED(hr))return hr;
    hr=D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"ps","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&error);
    if(FAILED(hr))return hr;
    hr=game->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex);if(FAILED(hr))return hr;
    hr=game->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel);if(FAILED(hr))return hr;
    D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    s.MaxLOD=D3D11_FLOAT32_MAX;hr=game->CreateSamplerState(&s,&sampler);if(FAILED(hr))return hr;
    D3D11_BLEND_DESC b{};auto& rt=b.RenderTarget[0];rt.BlendEnable=TRUE;
    rt.SrcBlend=D3D11_BLEND_ONE;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;hr=game->CreateBlendState(&b,&blend);if(FAILED(hr))return hr;
    D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;r.ScissorEnable=TRUE;
    hr=game->CreateRasterizerState(&r,&raster);if(FAILED(hr))return hr;
    hr=ui.CopyTo(drawingDevice);if(FAILED(hr))return hr;
    return uiContext.CopyTo(drawingContext);
}
HRESULT Dx11Surface::createTarget(IDXGISwapChain* chain,ID2D1DeviceContext* drawingContext,ID2D1Bitmap1** bitmap) {
    Ptr<ID3D11Texture2D> back;HRESULT hr=chain->GetBuffer(0,IID_PPV_ARGS(&back));if(FAILED(hr))return hr;
    D3D11_TEXTURE2D_DESC d{};back->GetDesc(&d);width=d.Width;height=d.Height;
    hr=game->CreateRenderTargetView(back.Get(),nullptr,&target);if(FAILED(hr))return hr;
    d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc={1,0};
    d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    d.CPUAccessFlags=0;d.MiscFlags=D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    hr=ui->CreateTexture2D(&d,nullptr,&texture);if(FAILED(hr))return hr;
    hr=texture.As(&producer);if(FAILED(hr))return hr;
    Ptr<IDXGIResource> resource;hr=texture.As(&resource);if(FAILED(hr))return hr;
    HANDLE handle=nullptr;hr=resource->GetSharedHandle(&handle);if(FAILED(hr))return hr;
    // A legacy DXGI shared handle is owned by the resource; do not CloseHandle.
    hr=game->OpenSharedResource(handle,IID_PPV_ARGS(&opened));if(FAILED(hr))return hr;
    hr=opened.As(&consumer);if(FAILED(hr))return hr;
    hr=game->CreateShaderResourceView(opened.Get(),nullptr,&view);if(FAILED(hr))return hr;
    Ptr<IDXGISurface> surface;hr=texture.As(&surface);if(FAILED(hr))return hr;
    const auto properties=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(d.Format,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
    return drawingContext->CreateBitmapFromDxgiSurface(surface.Get(),&properties,bitmap);
}
void Dx11Surface::releaseTarget() {
    target.Reset();view.Reset();consumer.Reset();producer.Reset();opened.Reset();texture.Reset();published=false;rasterReady=false;
    if(uiContext)uiContext->Flush();
}
HRESULT Dx11Surface::begin() {
    if(published)return S_FALSE;
    const HRESULT hr=producer->AcquireSync(0,0);
    if(hr==WAIT_TIMEOUT)return S_FALSE;
    return hr==S_OK?S_OK:(FAILED(hr)?hr:E_FAIL);
}
HRESULT Dx11Surface::end(const D2D1_RECT_F& region) {
    bounds=region;uiContext->Flush();
    rasterReady=true;++rasterizations;
    const HRESULT hr=producer->ReleaseSync(1);if(FAILED(hr))return hr;
    published=true;return composite();
}
HRESULT Dx11Surface::reuse() {
    if(!rasterReady)return S_FALSE;
    const auto acquired=begin();
    if(acquired==S_FALSE)return composite();
    if(FAILED(acquired))return acquired;
    // Reuse immutable pixels; no D2D commands or producer Flush are needed.
    const auto released=producer->ReleaseSync(1);if(FAILED(released))return released;
    published=true;return composite();
}
HRESULT Dx11Surface::composite() {
    if(!published)return S_OK;
    const HRESULT hr=consumer->AcquireSync(1,0);
    if(hr==WAIT_TIMEOUT)return S_OK; // Retry next frame rather than waiting on GPU.
    if(hr!=S_OK)return FAILED(hr)?hr:E_FAIL;
    Ptr<ID3DDeviceContextState> previous;
    context->SwapDeviceContextState(state.Get(),&previous);
    ID3D11RenderTargetView* output=target.Get();context->OMSetRenderTargets(1,&output,nullptr);
    context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);
    context->RSSetState(raster.Get());
    const D3D11_VIEWPORT viewport{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
    context->RSSetViewports(1,&viewport);
    const D3D11_RECT clip{
        (std::max)(0L,static_cast<LONG>(std::floor(bounds.left))),
        (std::max)(0L,static_cast<LONG>(std::floor(bounds.top))),
        (std::min)(static_cast<LONG>(width),static_cast<LONG>(std::ceil(bounds.right))),
        (std::min)(static_cast<LONG>(height),static_cast<LONG>(std::ceil(bounds.bottom)))};
    context->RSSetScissorRects(1,&clip);
    context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex.Get(),nullptr,0);context->PSSetShader(pixel.Get(),nullptr,0);
    ID3D11ShaderResourceView* input=view.Get();context->PSSetShaderResources(0,1,&input);
    ID3D11SamplerState* sampling=sampler.Get();context->PSSetSamplers(0,1,&sampling);
    context->Draw(3,0);
    // Our saved state must not retain resize-sensitive backbuffer references.
    input=nullptr;context->PSSetShaderResources(0,1,&input);context->OMSetRenderTargets(0,nullptr,nullptr);
    context->SwapDeviceContextState(previous.Get(),nullptr);
    ++composites;published=false;
    return consumer->ReleaseSync(0);
}
}
