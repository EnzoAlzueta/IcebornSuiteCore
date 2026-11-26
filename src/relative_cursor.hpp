#pragma once
#include <algorithm>
namespace meter {
class RelativeCursor {
    float x_ = 0, y_ = 0, width_ = 1, height_ = 1;
    bool left_ = false;
public:
    void reset(float x, float y, float width, float height) {
        width_ = (std::max)(1.0f, width); height_ = (std::max)(1.0f, height);
        absolute(x, y); left_ = false;
    }
    void absolute(float x, float y) { x_ = std::clamp(x, 0.0f, width_-1); y_ = std::clamp(y, 0.0f, height_-1); }
    void move(float dx, float dy) { absolute(x_+dx, y_+dy); }
    void button(bool left) { left_ = left; }
    float x() const { return x_; }
    float y() const { return y_; }
    bool left() const { return left_; }
};
}
