#include <windows.h>
#include <filesystem>
#include <cstdio>
int wmain(int argc,wchar_t** argv) {
    if(argc<2 || argc>3)return 2;
    HMODULE dinput=nullptr;
    if(std::wstring_view(argv[1])==L"combined") {
        const auto file=std::filesystem::absolute(L"dinput8.dll");
        dinput=LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!dinput)return 7;
    }
    auto root=std::filesystem::absolute(L"xinput9_1_0.dll");
    auto proxy=LoadLibraryExW(root.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!proxy)return 3;
    auto entry=reinterpret_cast<DWORD(WINAPI*)(DWORD,void*)>(GetProcAddress(proxy,"XInputGetState"));
    alignas(8) char state[64]{};
    if(!entry)return 4;
    entry(0,state);
    for(int i=0;i<150;++i) {
        MSG msg{};
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&msg);DispatchMessageW(&msg);}
        const bool adapter=GetModuleHandleW(L"iceborn_suite_adapter.dll")!=nullptr;
        const bool core=GetModuleHandleW(L"iceborn_suite_core.dll")!=nullptr;
        const bool foreign=std::wstring_view(argv[1])!=L"clean";
        if(adapter && core && (!foreign || (GetModuleHandleW(L"xinput9_1_0.chain.dll") &&
                                            GetModuleHandleW(L"iceborne_damage_meter.dll")))) {
            wchar_t selected[32768]{};
            if(!GetModuleFileNameW(GetModuleHandleW(L"iceborn_suite_core.dll"),selected,32768))return 8;
            if(argc==3 && std::filesystem::path(selected).parent_path().filename()!=argv[2])return 9;
            printf("Selected runtime %ls\n",selected);
            puts("PASS root, adapter and versioned runtime loaded in isolated host");
            return std::filesystem::exists(L"nativePC/IcebornSuite/user/core-dev5.log") ? 6 : 0;
        }
        Sleep(100);
    }
    fprintf(stderr,"adapter=%p core=%p chain=%p meter=%p\n",GetModuleHandleW(L"iceborn_suite_adapter.dll"),
            GetModuleHandleW(L"iceborn_suite_core.dll"),GetModuleHandleW(L"xinput9_1_0.chain.dll"),
            GetModuleHandleW(L"iceborne_damage_meter.dll"));
    return 5;
}
