#ifdef _WIN32
#include "app.hpp"
#include "gl_api.hpp"
#include "runtime_paths.hpp"
#include <windowsx.h>
#include <chrono>
#include <exception>
#include <thread>

namespace {
tumpuk::App* app=nullptr;
bool closing=false, minimized=false;

LRESULT CALLBACK windowProc(HWND win,UINT msg,WPARAM w,LPARAM l) {
    switch(msg) {
    case WM_CLOSE: closing=true; return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_GETMINMAXINFO: {
        auto* limits=reinterpret_cast<MINMAXINFO*>(l);
        limits->ptMinTrackSize={666,410};
        return 0;
    }
    case WM_SIZE:
        minimized=(w==SIZE_MINIMIZED);
        if(app&&!minimized) app->resize(LOWORD(l),HIWORD(l));
        return 0;
    case WM_KILLFOCUS:
        if(app&&app->screen()==tumpuk::Screen::Playing) app->key('p');
        return 0;
    case WM_KEYDOWN:
        if(app && !(l & (1L<<30))) {
            if(w==VK_ESCAPE) app->key(27);
            else if(w==VK_RETURN) app->key('\r');
            else if(w==VK_SPACE || (w>='A'&&w<='Z')) app->key(static_cast<unsigned char>(w));
        }
        return 0;
    case WM_MOUSEMOVE:
        if(app) app->pointer(GET_X_LPARAM(l),GET_Y_LPARAM(l));
        return 0;
    case WM_LBUTTONDOWN:
        if(app) app->click(GET_X_LPARAM(l),GET_Y_LPARAM(l));
        return 0;
    }
    return DefWindowProcW(win,msg,w,l);
}

int error(const wchar_t* message) {
    MessageBoxW(nullptr,message,L"TUMPUK",MB_OK|MB_ICONERROR);
    return 1;
}

struct Window {
    HWND handle=nullptr;
    HDC dc=nullptr;
    HGLRC gl=nullptr;
    ~Window() {
        if(gl) { wglMakeCurrent(nullptr,nullptr); wglDeleteContext(gl); }
        if(dc&&handle) ReleaseDC(handle,dc);
        if(handle) DestroyWindow(handle);
    }
};
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    try {
        WNDCLASSW wc{};
        wc.style=CS_OWNDC;
        wc.lpfnWndProc=windowProc;
        wc.hInstance=instance;
        wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
        wc.lpszClassName=L"TumpukOpenGLWindow";
        if(!RegisterClassW(&wc)) return error(L"Jendela game tidak bisa disiapkan.");
        constexpr DWORD style=WS_OVERLAPPEDWINDOW;
        const int w=GetSystemMetrics(SM_CXSCREEN)<1310?960:1280;
        const int h=w*9/16;
        RECT bounds{0,0,w,h};
        AdjustWindowRect(&bounds,style,FALSE);
        Window win;
        win.handle=CreateWindowExW(0,wc.lpszClassName,L"TUMPUK - Stack Arcade",style,
            CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,
            nullptr,nullptr,instance,nullptr);
        if(!win.handle) return error(L"Jendela game tidak bisa dibuka.");
        win.dc=GetDC(win.handle);
        if(!win.dc) return error(L"Perangkat grafis tidak tersedia.");
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize=sizeof(pfd);
        pfd.nVersion=1;
        pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
        pfd.iPixelType=PFD_TYPE_RGBA;
        pfd.cColorBits=32;
        pfd.cDepthBits=24;
        pfd.iLayerType=PFD_MAIN_PLANE;
        const int format=ChoosePixelFormat(win.dc,&pfd);
        if(!format||!SetPixelFormat(win.dc,format,&pfd)) return error(L"Format OpenGL tidak tersedia.");
        win.gl=wglCreateContext(win.dc);
        if(!win.gl||!wglMakeCurrent(win.dc,win.gl)) return error(L"Konteks OpenGL gagal dibuat.");

        const auto folder=executableFolder();
        tumpuk::App game(folder/"credits.txt",tumpuk::scoreLocation(folder));
        game.initGraphics();
        app=&game;
        RECT client{};
        GetClientRect(win.handle,&client);
        game.resize(client.right,client.bottom);
        ShowWindow(win.handle,show);
        using Clock=std::chrono::steady_clock;
        auto last=Clock::now();
        while(!closing&&!game.shouldExit()) {
            MSG msg{};
            while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
                if(msg.message==WM_QUIT) { closing=true; break; }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if(closing||game.shouldExit()) break;
            if(minimized) { WaitMessage(); last=Clock::now(); continue; }
            const auto frame=Clock::now();
            game.update(std::chrono::duration<float>(frame-last).count());
            last=frame;
            game.render();
            SwapBuffers(win.dc);
            std::this_thread::sleep_until(frame+std::chrono::microseconds(16667));
        }
        app=nullptr;
        return 0;
    } catch(const std::exception&) {
        app=nullptr;
        return error(L"Game berhenti karena terjadi kesalahan saat memulai.");
    }
}
#endif
