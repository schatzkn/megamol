/*
 * Window.cpp
 *
 * Copyright (C) 2008, 2016 MegaMol Team
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "gl/Window.h"
#include "utility/HotFixes.h"
#include "vislib/sys/Log.h"
#include "WindowManager.h"
#include <cassert>
#include <algorithm>
#include <sstream>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"
#endif

//#include "HotKeyButtonParam.h"
//#include "vislib/RawStorage.h"
//#include "vislib/types.h"
//#include "WindowManager.h"
//#include <chrono>
//#include <vector>
//#include <algorithm>

using namespace megamol;
using namespace megamol::console;

gl::Window::Window(const char* title, const utility::WindowPlacement & placement, GLFWwindow* share)
        : glfw(), hView(), hWnd(nullptr), width(-1), height(-1), renderContext(), uiLayers(), mouseCapture(),
        name(title), fpsCntr(), fps(1000.0f), fpsList(), showFpsInTitle(true), fpsSyncTime(), topMost(false) {

    if (::memcmp(name.c_str(), WindowManager::TitlePrefix, WindowManager::TitlePrefixLength) == 0) {
        name = name.substr(WindowManager::TitlePrefixLength);
    }
    for (float& f : fpsList) f = 0.0f;

    memset(&renderContext, 0, sizeof(mmcRenderViewContext));
    renderContext.Size = sizeof(mmcRenderViewContext);
    renderContext.ContinuousRedraw = true;
    renderContext.GpuAffinity = nullptr;
    renderContext.Direct3DRenderTarget = nullptr;
    renderContext.InstanceTime = 0.0; // will be generated by core
    renderContext.Time = 0.0; // will be generated by core

    glfw = glfwInst::Instance(); // we use glfw
    if (glfw->OK()) {
        if (utility::HotFixes::Instance().IsHotFixed("usealphabuffer")) {
            ::glfwWindowHint(GLFW_ALPHA_BITS, 8);
        }

//#ifndef NOWINDOWPOSFIX
//        if (wndX != predictedX || wndY != predictedY ||
//            wndW != predictedWidth || wndH != predictedHeight) {
//            Log::DefaultLog.WriteMsg(Log::LEVEL_WARN, "The actual "
//                "view window location reported by the core (%d, %d), "
//                "size (%d, %d) is "
//                "different from the one predicted. GPU affinity "
//                "may have been set incorrectly.", wndX, wndY, wndW,
//                wndH);
//        }
//#endif

        this->topMost = placement.topMost;
        if (!placement.fullScreen) {
            // window mode
            ::glfwWindowHint(GLFW_DECORATED, placement.noDec ? GL_FALSE : GL_TRUE);
            ::glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

            int w = placement.w;
            int h = placement.h;
            if (!placement.size || (w <= 0) || (h <= 0)) {
                GLFWmonitor* primary = ::glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = ::glfwGetVideoMode(primary);
                w = mode->width * 3 / 4;
                h = mode->height * 3 / 4;
            }

            hWnd = ::glfwCreateWindow(w, h, title, nullptr, share);
            if (hWnd != nullptr) {
                if (placement.pos) ::glfwSetWindowPos(hWnd, placement.x, placement.y);
            }

        } else {
            // fullscreen mode
            int monCnt = 0;
            GLFWmonitor **mons = ::glfwGetMonitors(&monCnt);
            GLFWmonitor *mon = mons[std::min<int>(monCnt - 1, placement.mon)];
            const GLFWvidmode* mode = glfwGetVideoMode(mon);

            if (placement.pos) vislib::sys::Log::DefaultLog.WriteWarn("Ignoring window placement position when requesting fullscreen.");
            if (placement.size) {
                if ((placement.w != mode->width) || (placement.h != mode->height)) {
                    vislib::sys::Log::DefaultLog.WriteWarn("Changing screen resolution is currently not supported.");
                }
            }
            if (placement.noDec) vislib::sys::Log::DefaultLog.WriteWarn("Ignoring no-decorations setting when requesting fullscreen.");

            ::glfwWindowHint(GLFW_DECORATED, GL_FALSE);
            ::glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
            ::glfwWindowHint(GLFW_RED_BITS, mode->redBits);
            ::glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
            ::glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
            ::glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            // this only works since we are NOT setting a monitor
            ::glfwWindowHint(GLFW_FLOATING, GL_TRUE);

            /* note we do not use a real fullscrene mode, since then we would have focus-iconify problems */
            hWnd = ::glfwCreateWindow(mode->width, mode->height, title, nullptr, share);
            int x, y;
            ::glfwGetMonitorPos(mon, &x, &y);
            ::glfwSetWindowPos(hWnd, x, y);
        }

        if (hWnd != nullptr) {
            ::glfwSetWindowUserPointer(hWnd, this); // this is ok, as long as no one derives from Window at this point
            ::glfwShowWindow(hWnd);
            ::glfwMakeContextCurrent(hWnd);
            if ((placement.fullScreen || placement.noDec) && (!utility::HotFixes::Instance().IsHotFixed("DontHideCursor"))) {
                ::glfwSetInputMode(hWnd, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            vislib::graphics::gl::LoadAllGL();
            ::glfwSetKeyCallback(hWnd, &Window::glfw_onKey_func);
            ::glfwSetMouseButtonCallback(hWnd, &Window::glfw_onMouseButton_func);
            ::glfwSetCursorPosCallback(hWnd, &Window::glfw_onMouseMove_func);
            ::glfwSetScrollCallback(hWnd, &Window::glfw_onMouseWheel_func);
            ::glfwSetCharCallback(hWnd, &Window::glfw_onChar_func);
        }
    }

    fpsSyncTime = std::chrono::system_clock::now();
}

gl::Window::~Window() {
    assert(hWnd == nullptr);
}

void gl::Window::EnableVSync() {
    if (hWnd != nullptr) {
        ::glfwMakeContextCurrent(hWnd);
        ::glfwSwapInterval(0);
    }
}

void gl::Window::AddUILayer(std::shared_ptr<AbstractUILayer> uiLayer) {
    auto it = std::find(uiLayers.begin(), uiLayers.end(), uiLayer);
    if (it != uiLayers.end()) {
        vislib::sys::Log::DefaultLog.WriteWarn("uiLayer already part of the window");
        return;
    }
    uiLayers.push_back(uiLayer);
}

void gl::Window::RemoveUILayer(std::shared_ptr<AbstractUILayer> uiLayer) {
    auto it = std::find(uiLayers.begin(), uiLayers.end(), uiLayer);
    if (it == uiLayers.end()) return;
    uiLayers.erase(it);
}

void gl::Window::SetShowFPSinTitle(bool show) {
    showFpsInTitle = show;
    if (!showFpsInTitle) {
        ::glfwSetWindowTitle(hWnd, (std::string(WindowManager::TitlePrefix) + name).c_str());
    }
}

void gl::Window::RequestClose() {
    if (hWnd != nullptr) {
        ::glfwSetWindowShouldClose(hWnd, true);
    }
}

void gl::Window::Update() {
    if (hWnd == nullptr) return;

    // this also issues the callbacks, which might close this window
    ::glfwPollEvents();

    if (hWnd == nullptr) return;
    if (::glfwWindowShouldClose(hWnd)) {
        uiLayers.clear();

        hView.DestroyHandle();

        ::glfwDestroyWindow(hWnd);
        hWnd = nullptr;
        return;
    }

    ::glfwMakeContextCurrent(hWnd);
    int frame_width, frame_height;
    ::glfwGetFramebufferSize(hWnd, &frame_width, &frame_height);
    if ((frame_width != width) || (frame_height != height)) {
        on_resize(frame_width, frame_height);
        width = frame_width;
        height = frame_height;
    }

    fpsCntr.FrameBegin();
    if ((width > 0) && (height > 0)) {
        ::mmcRenderView(hView, &renderContext);
    }

    for (std::shared_ptr<AbstractUILayer> uil : this->uiLayers) {
        if (!uil->Enabled()) continue;
        uil->onDraw();
    }

    // done rendering. swap and next turn
    ::glfwSwapBuffers(hWnd);
    fpsCntr.FrameEnd();

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    if (now - fpsSyncTime > std::chrono::seconds(1)) {
        on_fps_value(fpsCntr.FPS());
        fpsSyncTime = now;
#ifdef _WIN32
        if (this->topMost) {
            vislib::sys::Log::DefaultLog.WriteInfo("Periodic reordering of windows.");
            SetWindowPos(glfwGetWin32Window(this->hWnd), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        }
#endif
    }

}

void gl::Window::glfw_onKey_func(GLFWwindow* wnd, int k, int s, int a, int m) {
    ::glfwMakeContextCurrent(wnd);
    Window* that = static_cast<Window*>(::glfwGetWindowUserPointer(wnd));

    AbstractUILayer::Key key = static_cast<AbstractUILayer::Key>(k);

    AbstractUILayer::KeyAction action(AbstractUILayer::KeyAction::RELEASE);
    switch (a) {
    case GLFW_PRESS: action = AbstractUILayer::KeyAction::PRESS; break;
    case GLFW_REPEAT: action = AbstractUILayer::KeyAction::REPEAT; break;
    case GLFW_RELEASE: action = AbstractUILayer::KeyAction::RELEASE; break;
    }

    AbstractUILayer::Modifiers mods = AbstractUILayer::KEY_MOD_NONE;
    if ((m & GLFW_MOD_SHIFT) == GLFW_MOD_SHIFT) mods = static_cast<AbstractUILayer::Modifiers>(mods | AbstractUILayer::KEY_MOD_SHIFT);
    if ((m & GLFW_MOD_CONTROL) == GLFW_MOD_CONTROL) mods = static_cast<AbstractUILayer::Modifiers>(mods | AbstractUILayer::KEY_MOD_CTRL);
    if ((m & GLFW_MOD_ALT) == GLFW_MOD_ALT) mods = static_cast<AbstractUILayer::Modifiers>(mods | AbstractUILayer::KEY_MOD_ALT);

    for (std::shared_ptr<AbstractUILayer> uil : that->uiLayers) {
        if (!uil->Enabled()) continue;
        if (uil->onKey(key, s, action, mods)) break;
    }
}

void gl::Window::glfw_onChar_func(GLFWwindow* wnd, unsigned int charcode) {
    ::glfwMakeContextCurrent(wnd);
    Window* that = static_cast<Window*>(::glfwGetWindowUserPointer(wnd));
    for (std::shared_ptr<AbstractUILayer> uil : that->uiLayers) {
        if (!uil->Enabled()) continue;
        if (uil->onChar(charcode)) break;
    }
}

void gl::Window::glfw_onMouseMove_func(GLFWwindow* wnd, double x, double y) {
    ::glfwMakeContextCurrent(wnd);
    Window* that = static_cast<Window*>(::glfwGetWindowUserPointer(wnd));
    if (that->mouseCapture) {
        that->mouseCapture->onMouseMove(x, y);
    } else {
        for (std::shared_ptr<AbstractUILayer> uil : that->uiLayers) {
            if (!uil->Enabled()) continue;
            if (uil->onMouseMove(x, y)) break;
        }
    }
}

void gl::Window::glfw_onMouseButton_func(GLFWwindow* wnd, int b, int a, int m) {
    ::glfwMakeContextCurrent(wnd);
    Window* that = static_cast<Window*>(::glfwGetWindowUserPointer(wnd));

    AbstractUILayer::MouseButton btn = static_cast<AbstractUILayer::MouseButton>(b);

    AbstractUILayer::MouseButtonAction action =
        (a == GLFW_PRESS) ? AbstractUILayer::MouseButtonAction::PRESS
            : AbstractUILayer::MouseButtonAction::RELEASE;

    AbstractUILayer::Modifiers mods = AbstractUILayer::KEY_MOD_NONE;
    if ((m & GLFW_MOD_SHIFT) == GLFW_MOD_SHIFT) mods = static_cast<AbstractUILayer::Modifiers>(mods | AbstractUILayer::KEY_MOD_SHIFT);
    if ((m & GLFW_MOD_CONTROL) == GLFW_MOD_CONTROL) mods = static_cast<AbstractUILayer::Modifiers>(mods | AbstractUILayer::KEY_MOD_CTRL);
    if ((m & GLFW_MOD_ALT) == GLFW_MOD_ALT) mods = static_cast<AbstractUILayer::Modifiers>(mods | AbstractUILayer::KEY_MOD_ALT);

    if (that->mouseCapture) {
        that->mouseCapture->onMouseButton(btn, action, mods);
    } else {
        for (std::shared_ptr<AbstractUILayer> uil : that->uiLayers) {
            if (!uil->Enabled()) continue;
            if (uil->onMouseButton(btn, action, mods)) {
                if (action == AbstractUILayer::MouseButtonAction::PRESS) {
                    that->mouseCapture = uil;
                }
                break;
            }
        }
    }

    if (that->mouseCapture) {
        bool anyPressed = false;
        for (int mbi = GLFW_MOUSE_BUTTON_1; mbi <= GLFW_MOUSE_BUTTON_LAST; ++mbi) {
            if (::glfwGetMouseButton(wnd, mbi) == GLFW_PRESS) {
                anyPressed = true;
                break;
            }
        }
        if (!anyPressed) {
            that->mouseCapture.reset();
            double x, y;
            ::glfwGetCursorPos(wnd, &x, &y);
            glfw_onMouseMove_func(wnd, x, y); // to inform all of the new location
        }
    }
}

void gl::Window::glfw_onMouseWheel_func(GLFWwindow* wnd, double x, double y) {
    ::glfwMakeContextCurrent(wnd);
    Window* that = static_cast<Window*>(::glfwGetWindowUserPointer(wnd));
    if (that->mouseCapture) {
        that->mouseCapture->onMouseWheel(x, y);
    } else {
        for (std::shared_ptr<AbstractUILayer> uil : that->uiLayers) {
            if (!uil->Enabled()) continue;
            if (uil->onMouseWheel(x, y)) break;
        }
    }
}

void gl::Window::on_resize(int w, int h) {
    ::glfwMakeContextCurrent(hWnd);
    if ((w > 0) && (h > 0)) {
        ::glViewport(0, 0, w, h);
        ::mmcResizeView(hView, w, h);
        for (std::shared_ptr<AbstractUILayer> uil : uiLayers) {
            // we inform even disabled layers, since we would need to know and update as soon as they get enabled.
            uil->onResize(w, h);
        }
    }
}

void gl::Window::on_fps_value(float fps_val) {
    fps = fps_val;

    auto i1 = fpsList.begin();
    auto i2 = i1 + 1;
    auto e = fpsList.end();
    while (i2 != e) {
        *i1 = *i2;
        ++i1;
        ++i2;
    }
    fpsList[fpsList.size() - 1] = fps;

    if (showFpsInTitle) {
        std::stringstream title;
        title << WindowManager::TitlePrefix << name << " - [" << fps << " fps]";
        ::glfwSetWindowTitle(hWnd, title.str().c_str());
    }
}

/****************************************************************************/

//#ifdef HAS_ANTTWEAKBAR
//
//namespace {
//    void MEGAMOLCORE_CALLBACK collectParams(const char *paramName, void *contextPtr) {
//        std::vector<vislib::StringA> *paramNames = static_cast<std::vector<vislib::StringA>* >(contextPtr);
//        assert(paramNames != nullptr);
//        paramNames->push_back(paramName);
//    }
//}
//
///*
// * megamol::console::Window::InitGUI
// */
//void megamol::console::Window::InitGUI(CoreHandle& hCore) {
//    this->gui.BeginInitialisation();
//
//    std::vector<vislib::StringA> params;
//    ::mmcEnumParametersA(hCore, &collectParams, &params);
//
//    for (const vislib::StringA& paramName : params) {
//        vislib::SmartPtr<megamol::console::CoreHandle> hParam = new megamol::console::CoreHandle();
//        vislib::RawStorage desc;
//        if (!::mmcGetParameterA(hCore, paramName, *hParam)) continue;
//
//        unsigned int len = 0;
//        ::mmcGetParameterTypeDescription(*hParam, NULL, &len);
//        desc.AssertSize(len);
//        ::mmcGetParameterTypeDescription(*hParam, desc.As<unsigned char>(), &len);
//
//        this->gui.AddParameter(hParam, paramName, desc.As<unsigned char>(), len);
//    }
//
//    this->gui.EndInitialisation();
//}
//
//
//void megamol::console::Window::UpdateGUI(CoreHandle& hCore) {
//    std::vector<vislib::StringA> params;
//    std::vector<vislib::StringA> deadParams = gui.ParametersNames();
//
//    ::mmcEnumParametersA(hCore, &collectParams, &params);
//
//    for (const vislib::StringA& paramName : params) {
//
//        // search if param already exist
//        auto dpi = std::find(deadParams.begin(), deadParams.end(), vislib::StringA(paramName));
//        if (dpi != deadParams.end()) {
//            deadParams.erase(dpi); // this gui parameter is in use and will not be deleted
//            continue;
//        }
//
//        // parameter does not yet exist
//        vislib::SmartPtr<megamol::console::CoreHandle> hParam = new megamol::console::CoreHandle();
//        vislib::RawStorage desc;
//        if (!::mmcGetParameterA(hCore, paramName, *hParam)) continue;
//
//        unsigned int len = 0;
//        ::mmcGetParameterTypeDescription(*hParam, NULL, &len);
//        desc.AssertSize(len);
//        ::mmcGetParameterTypeDescription(*hParam, desc.As<unsigned char>(), &len);
//
//        this->gui.AddParameter(hParam, paramName, desc.As<unsigned char>(), len);
//
//    }
//
//    // now we delete all the orphaned gui parameters
//    for (const vislib::StringA& paramName : deadParams) {
//        this->gui.RemoveParameter(paramName);
//    }
//
//}
//#endif /* HAS_ANTTWEAKBAR */
//
///*
// * megamol::console::Window::Update
// */
//void megamol::console::Window::Update(CoreHandle& hCore) {
//#ifdef HAS_ANTTWEAKBAR
//    // update GUI once a second
//    static std::chrono::system_clock::time_point last = std::chrono::system_clock::now();
//    if (gui.IsActive()) {
//        std::chrono::system_clock::time_point n = std::chrono::system_clock::now();
//        if (std::chrono::duration_cast<std::chrono::seconds>(n - last).count() > 0) {
//            last = n;
//            UpdateGUI(hCore);
//        }
//    }
//#endif /* HAS_ANTTWEAKBAR */
//
//}
