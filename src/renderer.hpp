#pragma once
#include "core.hpp"
#include <memory>
#include <string>

namespace atelie {
class Window;
class Renderer {
public:
    Renderer(Window&,const std::wstring& assetRoot);
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    void upload(const Scene&);
    void render(const std::vector<DrawItem>&,const Camera&,bool lampOn,Vec3 lampPosition,const Hud&);
    // BMP image, bottom-up, 24-bit. Works immediately before or after Window::swap().
    void screenshot(const std::wstring& file);
    std::string info()const;
    std::string diagnostics()const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
