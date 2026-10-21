#include "platform.hpp"
#include <GL/gl.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace atelie {
namespace {
constexpr wchar_t windowClass[]=L"AtelieFileOpenGL33Window";
std::string winError(const char* action){return std::string(action)+" (Win32 "+std::to_string(GetLastError())+")";}
bool validProc(PROC p){auto v=reinterpret_cast<std::intptr_t>(p);return p&&v!=1&&v!=2&&v!=3&&v!=-1;}
using CreateContext=HGLRC (WINAPI*)(HDC,HGLRC,const int*);
using SwapInterval=BOOL (WINAPI*)(int);
}

Window::Window(int requestedWidth,int requestedHeight,bool visible):width(std::max(1,requestedWidth)),height(std::max(1,requestedHeight)){
    try{
        // Resolve dynamically so older Windows can still launch the executable.
        using SetDpiAwareness=BOOL (WINAPI*)(HANDLE);
        auto setDpi=reinterpret_cast<SetDpiAwareness>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext"));
        if(setDpi)setDpi(reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-4)));
        HINSTANCE instance=GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
        wc.lpfnWndProc=procedure;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
        wc.lpszClassName=windowClass;
        if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error(winError("RegisterClassEx"));
        RECT bounds{0,0,width,height};AdjustWindowRectEx(&bounds,WS_OVERLAPPEDWINDOW,FALSE,0);
        window_=CreateWindowExW(0,windowClass,L"Ateli\u00ea do Fil\u00e9 | OpenGL",WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,
            nullptr,nullptr,instance,this);
        if(!window_)throw std::runtime_error(winError("CreateWindowEx"));
        dc_=GetDC(window_);if(!dc_)throw std::runtime_error(winError("GetDC"));
        PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;
        pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
        pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cDepthBits=24;pfd.cStencilBits=8;pfd.iLayerType=PFD_MAIN_PLANE;
        int format=ChoosePixelFormat(dc_,&pfd);
        if(!format||!SetPixelFormat(dc_,format,&pfd))throw std::runtime_error(winError("SetPixelFormat"));
        context_=wglCreateContext(dc_);
        if(!context_||!wglMakeCurrent(dc_,context_))throw std::runtime_error(winError("Bootstrap OpenGL context"));
        PROC createProc=wglGetProcAddress("wglCreateContextAttribsARB");
        if(!validProc(createProc))throw std::runtime_error("OpenGL 3.3 Core is unavailable. Install the graphics driver from your GPU manufacturer.");
        auto create=reinterpret_cast<CreateContext>(createProc);
        const int attributes[]={0x2091,3,0x2092,3,0x9126,0x00000001,0};
        HGLRC modern=create(dc_,nullptr,attributes);
        if(!modern)throw std::runtime_error("The graphics driver could not create an OpenGL 3.3 Core context.");
        wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context_);context_=modern;
        if(!wglMakeCurrent(dc_,context_))throw std::runtime_error(winError("wglMakeCurrent"));
        auto swapProc=wglGetProcAddress("wglSwapIntervalEXT");
        if(validProc(swapProc))reinterpret_cast<SwapInterval>(swapProc)(visible?1:0);
        RAWINPUTDEVICE mouse{0x01,0x02,0,window_};
        if(!RegisterRawInputDevices(&mouse,1,sizeof(mouse)))throw std::runtime_error(winError("RegisterRawInputDevices"));
        if(visible){ShowWindow(window_,SW_SHOW);UpdateWindow(window_);SetFocus(window_);}
        focused=GetFocus()==window_;
        RECT client{};GetClientRect(window_,&client);width=std::max(1,static_cast<int>(client.right));height=std::max(1,static_cast<int>(client.bottom));
        POINT position{};GetCursorPos(&position);ScreenToClient(window_,&position);mouseX=static_cast<float>(position.x);mouseY=static_cast<float>(position.y);
    }catch(...){dispose();throw;}
}
Window::~Window(){dispose();}
void Window::dispose(){
    capture(false);
    if(context_){if(wglGetCurrentContext()==context_)wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context_);context_=nullptr;}
    if(dc_&&window_){ReleaseDC(window_,dc_);dc_=nullptr;}
    if(window_){DestroyWindow(window_);window_=nullptr;}
}
void Window::clipCursor(){
    if(!window_||!focused)return;
    RECT r{};GetClientRect(window_,&r);POINT a{r.left,r.top},b{r.right,r.bottom};
    ClientToScreen(window_,&a);ClientToScreen(window_,&b);r={a.x,a.y,b.x,b.y};ClipCursor(&r);
}
void Window::capture(bool enabled){
    enabled=enabled&&focused&&window_;
    if(captured_==enabled){if(enabled)clipCursor();return;}
    captured_=enabled;
    if(enabled){SetCapture(window_);clipCursor();SetCursor(nullptr);}
    else{ClipCursor(nullptr);if(GetCapture()==window_)ReleaseCapture();SetCursor(LoadCursorW(nullptr,IDC_ARROW));}
}
void Window::poll(){
    std::fill(std::begin(pressed),std::end(pressed),false);
    std::fill(std::begin(mousePressed),std::end(mousePressed),false);
    mouseDX=mouseDY=scroll=0;
    MSG message{};
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
        if(message.message==WM_QUIT){running=false;continue;}
        TranslateMessage(&message);DispatchMessageW(&message);
    }
}
void Window::swap(){if(!SwapBuffers(dc_))throw std::runtime_error(winError("SwapBuffers"));++swaps_;}
LRESULT CALLBACK Window::procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    Window* self=reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(hwnd,message,wp,lp);
    switch(message){
    case WM_CLOSE:self->running=false;return 0;
    case WM_DESTROY:self->running=false;return 0;
    case WM_SIZE:
        self->width=static_cast<int>(LOWORD(lp));self->height=static_cast<int>(HIWORD(lp));
        if(self->captured_)self->clipCursor();
        return 0;
    case WM_GETMINMAXINFO:{
        RECT minimum{0,0,1000,680};
        AdjustWindowRectEx(&minimum,static_cast<DWORD>(GetWindowLongPtrW(hwnd,GWL_STYLE)),FALSE,static_cast<DWORD>(GetWindowLongPtrW(hwnd,GWL_EXSTYLE)));
        auto* limits=reinterpret_cast<MINMAXINFO*>(lp);
        limits->ptMinTrackSize.x=minimum.right-minimum.left;limits->ptMinTrackSize.y=minimum.bottom-minimum.top;
        return 0;
    }
    case WM_MOVE:if(self->captured_)self->clipCursor();return 0;
    case WM_SETCURSOR:if(self->captured_){SetCursor(nullptr);return TRUE;}break;
    case WM_SETFOCUS:self->focused=true;return 0;
    case WM_KILLFOCUS:
        self->focused=false;self->capture(false);std::fill(std::begin(self->keys),std::end(self->keys),false);return 0;
    case WM_CAPTURECHANGED:if(reinterpret_cast<HWND>(lp)!=hwnd&&self->captured_){self->captured_=false;ClipCursor(nullptr);}return 0;
    case WM_KEYDOWN:case WM_SYSKEYDOWN:
        if(wp<256){if(!self->keys[wp])self->pressed[wp]=true;self->keys[wp]=true;}
        if(message==WM_SYSKEYDOWN&&wp==VK_F4&&(lp&(1u<<29)))self->running=false;
        return 0;
    case WM_KEYUP:case WM_SYSKEYUP:if(wp<256)self->keys[wp]=false;return 0;
    case WM_LBUTTONDOWN:self->mousePressed[0]=true;self->keys[VK_LBUTTON]=true;return 0;
    case WM_RBUTTONDOWN:self->mousePressed[1]=true;self->keys[VK_RBUTTON]=true;return 0;
    case WM_MBUTTONDOWN:self->mousePressed[2]=true;self->keys[VK_MBUTTON]=true;return 0;
    case WM_LBUTTONUP:self->keys[VK_LBUTTON]=false;return 0;
    case WM_RBUTTONUP:self->keys[VK_RBUTTON]=false;return 0;
    case WM_MBUTTONUP:self->keys[VK_MBUTTON]=false;return 0;
    case WM_MOUSEMOVE:self->mouseX=static_cast<float>(static_cast<short>(LOWORD(lp)));self->mouseY=static_cast<float>(static_cast<short>(HIWORD(lp)));return 0;
    case WM_MOUSEWHEEL:self->scroll+=static_cast<float>(static_cast<short>(HIWORD(wp)))/WHEEL_DELTA;return 0;
    case WM_INPUT:{
        if(!self->focused)break;
        UINT bytes=0;HRAWINPUT input=reinterpret_cast<HRAWINPUT>(lp);
        if(GetRawInputData(input,RID_INPUT,nullptr,&bytes,sizeof(RAWINPUTHEADER))==0&&bytes){
            std::vector<BYTE> data(bytes);
            if(GetRawInputData(input,RID_INPUT,data.data(),&bytes,sizeof(RAWINPUTHEADER))==bytes){
                auto raw=reinterpret_cast<const RAWINPUT*>(data.data());
                if(raw->header.dwType==RIM_TYPEMOUSE&&!(raw->data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE)){
                    self->mouseDX+=static_cast<float>(raw->data.mouse.lLastX);self->mouseDY+=static_cast<float>(raw->data.mouse.lLastY);
                }
            }
        }
        break; // DefWindowProc performs the required foreground raw-input cleanup.
    }
    case WM_ERASEBKGND:return 1;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
}
