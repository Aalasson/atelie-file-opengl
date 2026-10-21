#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif
#include <windows.h>
#include <cstdint>

namespace atelie {
// Thin native window/input layer. WGL owns a real OpenGL 3.3 Core context.
class Window {
public:
    Window(int width=1280,int height=800,bool visible=true);
    ~Window();
    Window(const Window&)=delete;
    Window& operator=(const Window&)=delete;
    bool running=true,focused=true;
    int width,height;
    bool keys[256]{},pressed[256]{},mousePressed[3]{};
    float mouseX=0,mouseY=0,mouseDX=0,mouseDY=0,scroll=0;
    void poll();
    void capture(bool enabled);
    void swap();
    HWND handle()const{return window_;}
    HDC dc()const{return dc_;}
    HGLRC context()const{return context_;}
    bool captured()const{return captured_;}
    std::uint64_t swapCount()const{return swaps_;}
private:
    HWND window_=nullptr;
    HDC dc_=nullptr;
    HGLRC context_=nullptr;
    bool captured_=false;
    std::uint64_t swaps_=0;
    static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
    void clipCursor();
    void dispose();
};
}
