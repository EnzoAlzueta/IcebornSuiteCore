#include <windows.h>
#include <filesystem>
namespace {
DWORD WINAPI start(void* module) {
    try {
        wchar_t path[32768]{};
        auto n=GetModuleFileNameW(static_cast<HMODULE>(module),path,32768);
        if(!n || n>=32768) return 1;
        const auto nativePC=std::filesystem::path(path).parent_path().parent_path();
        const auto runtime=nativePC/L"IcebornSuite"/L"runtime";
        auto file=runtime/L"0.1.0-dev.17"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.16"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.15"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.14"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.13"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.12"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.11"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.10"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.9"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.8"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.7"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.6"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.5"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.4"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.3"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.2"/L"iceborn_suite_core.dll";
        if (!std::filesystem::exists(file)) file=runtime/L"0.1.0-dev.1"/L"iceborn_suite_core.dll";
        auto core=LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!core) return 1;
        auto entry=reinterpret_cast<void(*)()>(GetProcAddress(core,"IcebornStartCore"));
        if(!entry) {FreeLibrary(core); return 1;}
        entry();
    } catch(...) {return 1;}
    return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        if(auto thread=CreateThread(nullptr,0,start,module,0,nullptr)) CloseHandle(thread);
    }
    return TRUE;
}
