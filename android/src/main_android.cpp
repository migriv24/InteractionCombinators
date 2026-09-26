/* main_android.cpp — the Android shell: NativeActivity glue + EGL/GLES3 +
 * ImGui android backend around CombinatorsApp. Platform glue only; the
 * application lives in ../src/app.cpp (the substrate split).
 *
 * Touch is an input modality, not a geometry (okf/concepts/substrates.md):
 * one finger goes to ImGui as the pointer (drag nodes, pull wires, tap
 * menus — the exact desktop gestures); two fingers are the CAMERA (pan +
 * pinch zoom), routed to the app's touch_pan/touch_zoom, which mark the
 * camera dirty so the same `config set view.camera` command logs on gesture
 * end. Both substrates compile to identical dispatcher commands.
 *
 * v1 limits (deliberate; see okf/log.md 2026-07-14): no soft keyboard — the
 * Save As dialog pre-fills a timestamp name so projects work untyped; the
 * command bar is desktop-only until an IME shim lands. No long-press context
 * menu yet. */
#include "app.hpp"

#include "voidmaiz/mobile.hpp"        // the safe area: status bar, cutout, gesture strip
#include "voidmaiz/textinputview.hpp" // Android's own keyboard (the text-input holiday)
#include "voidmaiz/widgets.hpp"

#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"

#include <android/configuration.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <cmath>
#include <filesystem>
#include <memory>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "voidmaiz", __VA_ARGS__)

namespace {

struct Shell {
    android_app* aapp = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool backends_ready = false; // window + EGL + imgui backends live
    bool app_ready = false;      // CombinatorsApp::init ran (once per process)
    CombinatorsApp app;

    // two-finger camera gesture (consumed here, never forwarded to ImGui)
    // the system keyboard: null on a plain NativeActivity, then the drawn one stays
    std::unique_ptr<maiz::TextInputPlatform> text_input;
    maiz::TextInputSession text_session;

    // the system's edges (Void Maiz's SafeArea), re-read twice a second: on a
    // gesture-navigation phone the bottom strip takes every touch as "go home"
    maiz::SafeArea safe;
    int safe_age = 1 << 20;

    bool gesture2 = false;
    float g_x0 = 0, g_y0 = 0, g_x1 = 0, g_y1 = 0;
};

bool egl_init(Shell& s) {
    s.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s.display == EGL_NO_DISPLAY || !eglInitialize(s.display, nullptr, nullptr))
        return false;
    const EGLint attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                              EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                              EGL_NONE};
    EGLConfig config;
    EGLint num = 0;
    if (!eglChooseConfig(s.display, attribs, &config, 1, &num) || num < 1) return false;
    EGLint format;
    eglGetConfigAttrib(s.display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(s.aapp->window, 0, 0, format);
    s.surface = eglCreateWindowSurface(s.display, config, s.aapp->window, nullptr);
    const EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    s.context = eglCreateContext(s.display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (s.surface == EGL_NO_SURFACE || s.context == EGL_NO_CONTEXT) return false;
    return eglMakeCurrent(s.display, s.surface, s.surface, s.context) == EGL_TRUE;
}

void egl_term(Shell& s) {
    if (s.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s.context != EGL_NO_CONTEXT) eglDestroyContext(s.display, s.context);
        if (s.surface != EGL_NO_SURFACE) eglDestroySurface(s.display, s.surface);
        eglTerminate(s.display);
    }
    s.display = EGL_NO_DISPLAY;
    s.surface = EGL_NO_SURFACE;
    s.context = EGL_NO_CONTEXT;
}

void backends_up(Shell& s) {
    if (s.backends_ready || !s.aapp->window) return;
    if (!egl_init(s)) {
        LOGE("EGL init failed");
        return;
    }
    ImGui_ImplAndroid_Init(s.aapp->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    if (!s.app_ready) {
        // density scaling once: 160dpi = 1x, then ×1.3 on top — physical
        // density alone still reads small on glass (author: "a bit bigger")
        float density = (float)AConfiguration_getDensity(s.aapp->config);
        float scale = density > 0 ? density / 160.0f : 2.0f;
        scale *= 1.3f;
        scale = scale < 1.3f ? 1.3f : (scale > 5.0f ? 5.0f : scale);
        maiz::apply_touch_metrics(scale); // chrome grows; canvas text doesn't
        s.app.touch_mode = true;
        s.app.ui_scale = scale; // the layout is decided in dp
        s.app.android_activity = s.aapp->activity; // JNI: updates over HTTP, the package installer
        s.app.base_dir = s.aapp->activity->internalDataPath
                             ? std::filesystem::path(s.aapp->activity->internalDataPath)
                             : std::filesystem::path("/data/local/tmp");
        android_app* aapp = s.aapp;
        s.app.on_quit = [aapp] { ANativeActivity_finish(aapp->activity); };
        s.app.init();
        s.text_input = maiz::android_text_input(s.aapp->activity);
        s.app.system_keyboard = s.text_input != nullptr;
        s.app_ready = true;
    }
    s.backends_ready = true;
}

void backends_down(Shell& s) {
    if (!s.backends_ready) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    egl_term(s);
    s.backends_ready = false; // the app (and ImGui context) survive backgrounding
}

void on_cmd(android_app* a, int32_t cmd) {
    Shell& s = *(Shell*)a->userData;
    switch (cmd) {
    case APP_CMD_INIT_WINDOW: backends_up(s); break;
    case APP_CMD_TERM_WINDOW: backends_down(s); break;
    default: break;
    }
}

int32_t on_input(android_app* a, AInputEvent* ev) {
    Shell& s = *(Shell*)a->userData;
    if (!s.backends_ready) return 0;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t pc = (int32_t)AMotionEvent_getPointerCount(ev);
        int32_t action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        if (pc >= 2) {
            float x0 = AMotionEvent_getX(ev, 0), y0 = AMotionEvent_getY(ev, 0);
            float x1 = AMotionEvent_getX(ev, 1), y1 = AMotionEvent_getY(ev, 1);
            if (!s.gesture2) {
                s.gesture2 = true;
                // release ImGui's pointer so any in-flight canvas drag aborts
                ImGui::GetIO().AddMouseButtonEvent(0, false);
            } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                float fdx = ((x0 + x1) - (s.g_x0 + s.g_x1)) * 0.5f;
                float fdy = ((y0 + y1) - (s.g_y0 + s.g_y1)) * 0.5f;
                s.app.touch_pan(fdx, fdy);
                float d_prev = std::hypot(s.g_x1 - s.g_x0, s.g_y1 - s.g_y0);
                float d_now = std::hypot(x1 - x0, y1 - y0);
                if (d_prev > 40.0f && d_now > 40.0f)
                    s.app.touch_zoom(d_now / d_prev, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f);
            }
            s.g_x0 = x0; s.g_y0 = y0; s.g_x1 = x1; s.g_y1 = y1;
            return 1; // the camera consumed it
        }
        if (s.gesture2) {
            // fingers lifting out of the gesture: swallow until all are up
            if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL)
                s.gesture2 = false;
            return 1;
        }
    }
    return ImGui_ImplAndroid_HandleInputEvent(ev);
}

void render_frame(Shell& s) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    maiz::text_input_frame(s.text_session, s.text_input.get()); // first, as documented
    if (++s.safe_age > 30) {
        s.safe = maiz::android_safe_area(s.aapp->activity);
        s.safe_age = 0;
    }
    maiz::reserve_safe_area(s.safe);
    s.app.frame();
    ImGui::Render();
    EGLint w = 0, h = 0;
    eglQuerySurface(s.display, s.surface, EGL_WIDTH, &w);
    eglQuerySurface(s.display, s.surface, EGL_HEIGHT, &h);
    glViewport(0, 0, w, h);
    if (s.app.light_mode) glClearColor(0.93f, 0.93f, 0.94f, 1.0f);
    else glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers(s.display, s.surface);
}

} // namespace

void android_main(android_app* a) {
    Shell s;
    s.aapp = a;
    a->userData = &s;
    a->onAppCmd = on_cmd;
    a->onInputEvent = on_input;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // window layout is ours, not imgui's

    while (true) {
        int events;
        android_poll_source* source;
        // render-driven: poll without blocking while we can draw
        while (ALooper_pollOnce(s.backends_ready ? 0 : -1, nullptr, &events,
                                (void**)&source) >= 0) {
            if (source) source->process(a, source);
            if (a->destroyRequested) {
                backends_down(s);
                ImGui::DestroyContext();
                return;
            }
            if (s.backends_ready) break; // drain later; keep frames coming
        }
        if (s.backends_ready) render_frame(s);
    }
}
