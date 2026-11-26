#pragma once
#include <d3d11_1.h>
#include <dxgi.h>
#include <d2d1_1.h>
#include <wrl/client.h>
#include <array>
#include <span>

namespace meter {
// Private BGRA UI device: also works when the game's device lacks BGRA support.
// A shared GPU texture avoids CPU readback. Synchronization never waits a frame.
class Dx11Surface {
    template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> game, ui;
    Ptr<ID3D11DeviceContext1> context;
    Ptr<ID3D11DeviceContext> uiContext;
    Ptr<ID3DDeviceContextState> state;
    Ptr<ID3D11VertexShader> vertex;
    Ptr<ID3D11PixelShader> pixel;
    Ptr<ID3D11SamplerState> sampler;
    Ptr<ID3D11BlendState> blend;
    Ptr<ID3D11RasterizerState> raster;
    Ptr<ID3D11Texture2D> texture, opened;
    Ptr<IDXGIKeyedMutex> producer, consumer;
    Ptr<ID3D11ShaderResourceView> view;
    Ptr<ID3D11RenderTargetView> target;
    UINT width=0, height=0;
    enum class Owner { producer, pendingConsumer, consumer };
    Owner owner=Owner::producer;
    std::array<D3D11_RECT,3> clips{};
    unsigned clipCount{};
    unsigned composites=0;
    bool rasterReady=false;
    unsigned rasterizations=0;
public:
    HRESULT initialize(ID3D11Device* device, ID3D11Device** drawingDevice, ID3D11DeviceContext** drawingContext);
    HRESULT createTarget(IDXGISwapChain* chain, ID2D1DeviceContext* drawingContext, ID2D1Bitmap1** bitmap);
    void releaseTarget();
    HRESULT begin(); // S_FALSE means busy: skip UI production, never stall the game.
    HRESULT end(std::span<const D2D1_RECT_F> regions);
    HRESULT composite();
    HRESULT reuse();
    unsigned compositeCount() const { return composites; }
    unsigned rasterCount() const { return rasterizations; }
};
}
