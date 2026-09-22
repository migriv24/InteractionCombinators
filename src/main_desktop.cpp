/* main_desktop.cpp — the desktop shell: GLFW window + OpenGL 3 + ImGui
 * backends around CombinatorsApp. Platform glue only; the application lives
 * in app.cpp (the substrate split — okf/concepts/substrates.md). */
#include "app.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "voidmaiz/widgets.hpp" // apply_touch_metrics

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

/* --phone / --phone-landscape: a PREVIEW of the Android layout on the desktop.
 * A 360 x 740 dp phone (the common size) at scale 2, touch mode on, and the
 * same apply_touch_metrics call the Android shell makes, so the layout code the
 * APK runs is the layout code on screen. The input is still a mouse, which is
 * why it is a preview and not a test of touch. */
int main(int argc, char** argv) {
    int phone = 0; // 0 = desktop, 1 = portrait, 2 = landscape
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--phone")) phone = 1;
        if (!std::strcmp(argv[i], "--phone-landscape")) phone = 2;
    }
    // test-only: two real processes over real sockets (see app.hpp test_lan)
    std::string test_lan, probe_out;
    bool add_box = false, save_as = false;
    bool auto_allow = false, lan_panel = false;
    double wire_at = 0;
    long long quit_after = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--lan-share")) test_lan = "share";
        if (!std::strcmp(argv[i], "--lan-join")) test_lan = "join";
        if (!std::strcmp(argv[i], "--lan-auto-allow")) auto_allow = true;
        if (!std::strcmp(argv[i], "--lan-panel")) lan_panel = true;
        if (!std::strcmp(argv[i], "--add-box")) add_box = true;
        if (!std::strcmp(argv[i], "--save-as")) save_as = true;
        if (!std::strcmp(argv[i], "--wire-at") && i + 1 < argc) wire_at = std::atof(argv[++i]);
        if (!std::strcmp(argv[i], "--quit-after-ms") && i + 1 < argc) quit_after = std::atoll(argv[++i]);
        if (!std::strcmp(argv[i], "--probe-out") && i + 1 < argc) probe_out = argv[++i];
    }
    const float phone_scale = 2.0f;
    int win_w = 1360, win_h = 800;
    if (phone) {
        win_w = (int)((phone == 1 ? 360 : 740) * phone_scale);
        win_h = (int)((phone == 1 ? 740 : 360) * phone_scale);
    }
    glfwSetErrorCallback([](int code, const char* desc) {
        std::fprintf(stderr, "glfw error %d: %s\n", code, desc);
    });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window =
        glfwCreateWindow(win_w, win_h, "Interaction Combinators — Void Maiz", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    CombinatorsApp app;
    if (phone) {
        maiz::apply_touch_metrics(phone_scale);
        app.touch_mode = true;
        app.ui_scale = phone_scale;
    }
    app.base_dir = std::filesystem::current_path();
    // the folder this executable runs from: an update unpacks BESIDE it
    std::error_code ec;
    app.install_dir = std::filesystem::weakly_canonical(std::filesystem::absolute(argv[0]), ec).parent_path();
    app.on_title = [&](const std::string& t) { glfwSetWindowTitle(window, t.c_str()); };
    app.on_quit = [&] { glfwSetWindowShouldClose(window, 1); };
    app.test_lan = test_lan;
    app.test_auto_allow = auto_allow;
    app.test_wire_at = wire_at;
    if (!test_lan.empty()) app.updates_enabled = false; // a test run asks nobody anything
    app.init();
    if (lan_panel) app.open_lan_panel();
    if (add_box) app.open_add_box();
    if (save_as) app.open_save_as();
    const double started = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) { glfwWaitEvents(); continue; }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.frame();
        if (quit_after > 0 && (glfwGetTime() - started) * 1000.0 > (double)quit_after)
            glfwSetWindowShouldClose(window, 1);

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        if (app.light_mode) glClearColor(0.93f, 0.93f, 0.94f, 1.0f);
        else glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    if (!probe_out.empty()) { // what this process ended up holding
        auto p = app.probe();
        std::FILE* f = std::fopen(probe_out.c_str(), "wb");
        if (f) {
            /* `status` is a snapshot at QUIT, so whichever process outlives the
             * other correctly reports it gone — which reads like a lost link
             * unless the links line is there to say what actually happened. */
            std::fprintf(f,
                         "status=%s\nnodes=%d wires=%d contested=%d questions=%d\n"
                         "links=%s\nshape=%s\n",
                         p.status.c_str(), p.nodes, p.wires, p.contested, p.questions,
                         p.links.c_str(), p.shape.c_str());
            std::fclose(f);
        }
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
