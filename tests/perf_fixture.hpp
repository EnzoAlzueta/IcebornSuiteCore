#pragma once
#include <algorithm>
#include <chrono>
#include <psapi.h>
#pragma comment(lib,"psapi.lib")

namespace meter { std::array<unsigned,2> fixtureDrawCounts(); }

// Each invocation runs in a fresh directory containing copies of the selected DLLs.
int runBenchmark(std::wstring_view mode) {
    using Clock=std::chrono::steady_clock;
    auto enabled=[&](std::wstring_view id){return mode==id || mode==L"all";};
    const bool hp=enabled(L"hp"), buffs=enabled(L"buff"), crowns=enabled(L"crown");
    if(enabled(L"meter"))loadFixtureMeter(L"iceborne_damage_meter.dll");
    Modules modules{};
    if(hp) {
        assert(loadPanel(std::filesystem::absolute(L"hp.dll"),"monster-info",modules));
        setMonsterHealthAlways(true);
    }
    auto timer=buffs?loadTimers(std::filesystem::absolute(L"buff.dll")):nullptr;
    auto crown=crowns?loadCrowns(std::filesystem::absolute(L"crown.dll")):nullptr;
    assert(bool(timer)==buffs && bool(crown)==crowns);
    Input input;
    constexpr uint64_t base=0x140000000, manager=0x20000000, component=0x30000000, kulu=0xfa4f0080;
    input.set(base+0x0500cf40,manager);
    input.set(manager+0x38,std::array<uint64_t,128>{component});
    input.monster(component,kulu,27,750.f,821.5f);
    input.set(kulu+0x184,.88f);input.set(kulu+0x7730,1.f);
    input.set(base+0x0500eca0,uint64_t{0x40000000});input.set(0x4000aed0,101);
    input.set(base+0x050139a0,uint64_t{0x45000000});input.set(0x45000050,uint64_t{0x46000000});
    input.set(0x46007d20,uint64_t{0x47000000});
    struct Attack {float remaining; int32_t variant;};
    input.set(0x470006a0,Attack{179.7f,10});
    IcebornFrame frame{sizeof(frame),421810,66102,2,1,base,&input,readMem};
    CrownFeed feed;
    auto sample=[&] {
        auto scene=samplePanels(modules,frame,timer);
        if(crown) {
            std::array<IcebornCrownRow,ICEBORN_MAX_CROWNS> rows{};
            const auto n=crown->sample(&frame,rows.data(),static_cast<uint32_t>(rows.size()));
            assert(n>=0 && n<=rows.size());
            feed.observe(frame,{rows.data(),static_cast<size_t>(n)},1000);
            feed.show(scene,CrownSettings{},1000);
        }
        assert(bool(scene.count)==hp && bool(scene.timerCount)==buffs && bool(scene.crownCount)==crowns);
        publish(scene);
    };
    sample();
    input.reads=0;
    for(int i=0;i<100;++i)sample();
    HWND window=CreateWindowExW(0,L"STATIC",L"HUD benchmark",WS_OVERLAPPEDWINDOW,0,0,800,600,
        nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(window);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc={800,600,{},DXGI_FORMAT_R8G8B8A8_UNORM};
    desc.SampleDesc.Count=1;desc.BufferCount=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect=DXGI_SWAP_EFFECT_SEQUENTIAL;desc.OutputWindow=window;desc.Windowed=TRUE;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> ctx;ComPtr<IDXGISwapChain> chain;
    const auto driver=GetEnvironmentVariableW(L"ICEBORN_FIXTURE_HARDWARE",nullptr,0)?
        D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP;
    require(D3D11CreateDeviceAndSwapChain(nullptr,driver,nullptr,0,nullptr,0,D3D11_SDK_VERSION,
        &desc,&chain,&device,nullptr,&ctx));
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapterDesc{};
    require(device.As(&dxgi));
    require(dxgi->GetAdapter(&adapter));
    require(adapter->GetDesc(&adapterDesc));
    printf("ADAPTER %ls\n",adapterDesc.Description);
    initializeSettings(std::filesystem::absolute("bench.ini"),hp,crowns);
    assert(installHooks());
    require(chain->Present(0,0));
    findDamageSettings();
    std::vector<double> times;
    for(int i=0;i<360;++i) {
        const auto start=Clock::now();
        require(chain->Present(0,0));
        const auto us=std::chrono::duration<double,std::micro>(Clock::now()-start).count();
        if(i>=60)times.push_back(us);
    }
    std::sort(times.begin(),times.end());
    printf("RESULT %ls reads/sample=%u present_us_p50=%.2f p95=%.2f\n",std::wstring(mode).c_str(),
        input.reads/100,times[149],times[284]);
    times.clear();
    const auto before=fixtureDrawCounts();
    for(int i=0;i<300;++i) {
        const auto start=Clock::now();
        drawOverlay(chain.Get(),0);
        times.push_back(std::chrono::duration<double,std::micro>(Clock::now()-start).count());
        ctx->Flush();
    }
    std::sort(times.begin(),times.end());
    const auto after=fixtureDrawCounts();
    printf("HUD %ls cpu_us_p50=%.2f p95=%.2f composites=%u rasters=%u\n",std::wstring(mode).c_str(),
        times[149],times[284],after[1]-before[1],after[0]-before[0]);
    if(mode==L"all" || mode==L"core") {
        auto processTime=[] {
            FILETIME created,exited,kernel,user;
            assert(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user));
            auto ticks=[](FILETIME ft){return (uint64_t(ft.dwHighDateTime)<<32)|ft.dwLowDateTime;};
            return ticks(kernel)+ticks(user);
        };
        const auto counts=fixtureDrawCounts();
        times.clear();
        const auto first=processTime();
        for(unsigned i=0;i<1200;++i) {
            const auto start=Clock::now();
            if(hp && i%12==0) {
                input.monster(component,kulu,27,700.f+(i/12)%100,821.5f);
                sample();
            }
            require(chain->Present(0,0));
            times.push_back(std::chrono::duration<double,std::micro>(Clock::now()-start).count());
        }
        const auto cpu=processTime()-first;
        PROCESS_MEMORY_COUNTERS mem{};
        assert(GetProcessMemoryInfo(GetCurrentProcess(),&mem,sizeof(mem)));
        std::sort(times.begin(),times.end());
        const auto changed=fixtureDrawCounts();
        printf("UPDATES p50_us=%.2f p95_us=%.2f cpu_ms=%.1f working_set_MiB=%.1f composites=%u rasters=%u\n",
            times[599],times[1139],cpu/10000.,mem.WorkingSetSize/1048576.,changed[1]-counts[1],changed[0]-counts[0]);
    }
    return 0;
}
