#pragma once
#include <cstddef>
namespace meter {
enum class DxgiMethod {Present,Present1,Resize,Resize1,Execute,Create,CreateHwnd};
bool bindDxgiMethod(void* object,size_t index,void* callback,DxgiMethod method);
void* incomingDxgiOriginal(size_t index);
void disableDxgiMethods();
}
