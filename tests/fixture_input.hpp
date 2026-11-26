#pragma once
struct Input {
    unsigned reads{};
    std::map<uint64_t,std::vector<char>> memory;
    template<class T> void set(uint64_t at,const T& data) {
        auto p=reinterpret_cast<const char*>(&data); memory[at]={p,p+sizeof(data)};
    }
    void monster(uint64_t c,uint64_t m,int species,float hp,float maximum) {
        auto status=m+0x40000,health=m+0x50000;
        set(c+0x138,m); set(m+0x1bc48,status); set(status+0x148,m); set(m+0x12280,species);
        set(m+0x7670,health); set(health+0x60,std::array<float,2>{maximum,hp});
    }
};
int32_t readMem(void* ctx,uint64_t at,void* out,uint32_t size) {
    ++static_cast<Input*>(ctx)->reads;
    auto& memory=static_cast<Input*>(ctx)->memory;
    auto i=memory.find(at);
    if(i==memory.end() || i->second.size()!=size) return 0;
    memcpy(out,i->second.data(),size);return 1;
}
void require(HRESULT hr) {
    if(FAILED(hr)) {fprintf(stderr,"HRESULT %08lx\n",hr);exit(2);}
}
void loadFixtureMeter(const std::filesystem::path& path) {
    const auto file=std::filesystem::absolute(path);
    const auto logfile=file.parent_path()/L"iceborne_damage_meter.log";
    auto offset=std::filesystem::exists(logfile)?std::filesystem::file_size(logfile):0;
    assert(LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32));
    bool ready=false;
    for(int i=0;i<1200 && !ready;++i) {
        MSG msg{};
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&msg);DispatchMessageW(&msg);}
        std::ifstream log(logfile,std::ios::binary);
        log.seekg(static_cast<std::streamoff>(offset));
        std::string line;
        while(std::getline(log,line)) if(line.find("Initialization complete.")!=std::string::npos)ready=true;
        log.close(); if(!ready)Sleep(50);
    }
    assert(ready);
}
