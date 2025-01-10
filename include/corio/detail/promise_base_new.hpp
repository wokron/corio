#pragma once

namespace corio::detail {

class Background;

class PromiseBase {
public:
    void set_background(const Background *background) {
        background_ = background;
    }

    const Background *background() const { return background_; }

private:
    const Background *background_ = nullptr;
};

} // namespace corio::detail