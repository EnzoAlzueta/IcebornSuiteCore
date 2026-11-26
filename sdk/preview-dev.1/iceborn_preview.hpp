#pragma once
#include <algorithm>
#include <cstdint>
// Borrowed for one DX11 Present. x/y are a temporary move in percent or -1.
// The exclusion rectangle uses back-buffer coordinates. An empty rect allows free placement.
struct IcebornPreviewArea {
    float left,top,right,bottom;
    int32_t x,y;
    float displayTop(float px,float py,float width,float height) const {
        if(left<right && px<right && px+width>left && py<bottom && py+height>top)
            return std::max(0.f,top-height-6.f);
        return py;
    }
};
using IcebornDrawPreview=int (*)(void* swapchain,const IcebornPreviewArea*);
