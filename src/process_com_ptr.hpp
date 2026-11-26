#pragma once
#include <wrl/client.h>

namespace meter {
// Only for globals owned by this pinned DLL. Windows reclaims them at process
// exit: calling a driver through Release from CRT/DllMain teardown is unsafe
// after other modules have already dismantled their graphics state. Explicit
// Reset, reassignment and local ordinary ComPtr owners still release normally.
template<class T> class ProcessComPtr : public Microsoft::WRL::ComPtr<T> {
    using Base = Microsoft::WRL::ComPtr<T>;
public:
    using Base::Base;
    using Base::operator=;
    ~ProcessComPtr() noexcept { this->Detach(); }
};
}
