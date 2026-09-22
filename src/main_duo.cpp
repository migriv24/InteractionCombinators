/* main_duo.cpp — two people on one desk: the collaboration test bench.
 *
 * Two windows in ONE process, "Ana" (the host) and "Bo" (who joins), each its
 * own CombinatorsApp with its own Core, replica and Palabra session, joined by an
 * in-memory link the tester can make slow, lossy or cut. Everything above the
 * byte pipe is the real thing: the sync session, the merge, the splice, the
 * presence, the wire runes. Only the sockets are simulated, which is why this
 * can exist before VoidMaiz Q36 (where the LAN socket layer lives) is answered.
 *
 * What it is for: VoidMaiz okf/testing/collaborative-canvas-user-tests.md —
 * one person running the two-person tests. Drag in one window, watch the other.
 *
 * Each window has its own Dear ImGui context (1.92's GLFW backend supports
 * several) and its own GL context; the loop makes each current in turn.
 */
#include "app.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <functional>
#include <random>
#include <string>

namespace fs = std::filesystem;

namespace {

long long now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

/* The simulated network between the two windows. */
struct Link {
    struct Frame {
        long long due;
        int to; // 0 = Ana, 1 = Bo
        std::string bytes;
    };
    std::deque<Frame> queue;
    int latency_ms = 40; // one way; a LAN is ~1-5 ms, Wi-Fi to a phone ~10-60 ms
    int loss_pct = 0;
    bool cut = false; // a partition: frames are dropped, both keep working
    std::mt19937 rng{std::random_device{}()};
    std::size_t sent = 0, dropped = 0;

    void push(int to, const std::string& bytes) {
        ++sent;
        if (cut || (int)(rng() % 100) < loss_pct) {
            ++dropped;
            return;
        }
        queue.push_back({now_ms() + latency_ms, to, bytes});
    }
};

std::string random_replica_id(const char* who) {
    std::mt19937_64 r{std::random_device{}()};
    char buf[64];
    std::snprintf(buf, sizeof buf, "duo-%s-%016llx", who, (unsigned long long)r());
    return buf;
}

struct Seat {
    GLFWwindow* window = nullptr;
    ImGuiContext* ctx = nullptr;
    CombinatorsApp app;
};

/* Side by side, each half of the primary monitor's work area, so neither window
 * starts off screen on a laptop (the first version assumed 1840 px). */
bool open_seat(Seat& seat, const char* title, int index, GLFWwindow* share) {
    int wx = 0, wy = 0, ww = 1600, wh = 900;
    if (GLFWmonitor* m = glfwGetPrimaryMonitor()) glfwGetMonitorWorkarea(m, &wx, &wy, &ww, &wh);
    const int gap = 8, title_bar = 32;
    int w = (ww - 3 * gap) / 2;
    int h = wh - title_bar - 2 * gap;
    seat.window = glfwCreateWindow(w, h, title, nullptr, share);
    if (!seat.window) return false;
    glfwSetWindowPos(seat.window, wx + gap + index * (w + gap), wy + title_bar + gap);
    glfwMakeContextCurrent(seat.window);
    seat.ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(seat.ctx);
    ImGui_ImplGlfw_InitForOpenGL(seat.window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    return true;
}

void render_seat(Seat& seat) {
    glfwMakeContextCurrent(seat.window);
    ImGui::SetCurrentContext(seat.ctx);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    seat.app.frame();
}

void finish_seat(Seat& seat) {
    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(seat.window, &w, &h);
    glViewport(0, 0, w, h);
    if (seat.app.light_mode) glClearColor(0.93f, 0.93f, 0.94f, 1.0f);
    else glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(seat.window);
}

} // namespace

/* --selftest: the bench drives itself in hidden windows and reports. It proves
 * the join, the splice, a remote step, and two concurrent steps healing into one
 * valid net — through the app's real code, with no mouse. Exit 0 = pass. */
static int failures = 0;
static void expect(bool ok, const char* what) {
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++failures;
}

int main(int argc, char** argv) {
    bool selftest = argc > 1 && std::string(argv[1]) == "--selftest";
    glfwSetErrorCallback([](int code, const char* desc) {
        std::fprintf(stderr, "glfw error %d: %s\n", code, desc);
    });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    if (selftest) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    IMGUI_CHECKVERSION();

    Link link;
    Seat seats[2];
    const char* names[2] = {"Ana", "Bo"};
    const unsigned colours[2] = {0x2f9e8f, 0xc2548a}; // teal, magenta: not a pigment
    const char* tags[2] = {"a", "b"};
    fs::path root = fs::current_path() / "duo";

    for (int i = 0; i < 2; ++i) {
        std::string title = std::string("Interaction Combinators — ") + names[i] +
                            (i == 0 ? " (host)" : " (joined)");
        if (!open_seat(seats[i], title.c_str(), i, i ? seats[0].window : nullptr))
            return 1;
        glfwSwapInterval(i == 0 ? 1 : 0); // one vsync paces the loop, not two
        CombinatorsApp& app = seats[i].app;
        app.base_dir = root / names[i];
        std::error_code ec;
        fs::create_directories(app.base_dir, ec);
        app.role = i == 0 ? CombinatorsApp::Role::Host : CombinatorsApp::Role::Join;
        app.updates_enabled = false; // a test rig does not ask to update itself
        app.profile_name = names[i];
        app.profile_rgb = colours[i];
        app.device_tag = tags[i];
        app.replica_id = random_replica_id(tags[i]);
        app.now_ms = now_ms;
        int other = 1 - i;
        app.send = [&link, other](const std::string&, const std::string& frame) {
            link.push(other, frame);
        };
        app.on_quit = [&seats] {
            for (auto& s : seats) glfwSetWindowShouldClose(s.window, 1);
        };
        app.init();
    }
    // links are named after the peer they reach
    seats[0].app.connect("Bo");
    seats[1].app.connect("Ana");

    auto run_frames = [&](int n) {
        for (int k = 0; k < n; ++k) {
            glfwPollEvents();
            long long now = now_ms();
            while (!link.queue.empty() && link.queue.front().due <= now) {
                Link::Frame f = std::move(link.queue.front());
                link.queue.pop_front();
                seats[f.to].app.receive(names[1 - f.to], f.bytes);
            }
            for (int i = 0; i < 2; ++i) {
                render_seat(seats[i]);
                finish_seat(seats[i]);
            }
        }
    };

    /* Wait on TIME, not frames: Palabra resends an unacknowledged document after
     * 2 s, so healing a partition legitimately takes that long. A frame count
     * passed in the unoptimized dev build and failed in the optimized release
     * build, whose hidden windows run far faster (found by the release script). */
    auto run_until = [&](const std::function<bool()>& done, int max_ms) {
        long long until = now_ms() + max_ms;
        while (now_ms() < until) {
            run_frames(1);
            if (done()) {
                run_frames(2); // let the last splice's playback start
                return true;
            }
        }
        return false;
    };

    if (selftest) {
        link.latency_ms = 5;
        for (auto& seat : seats) {
            ImGui::SetCurrentContext(seat.ctx);
            ImGui::GetIO().IniFilename = nullptr; // leave no imgui.ini behind
        }
        CombinatorsApp& ana = seats[0].app;
        CombinatorsApp& bo = seats[1].app;
        auto same = [&] { return ana.probe().shape == bo.probe().shape; };
        auto healthy = [&](CombinatorsApp& a) {
            auto p = a.probe();
            return p.contested == 0 && p.questions == 0 && p.net_err.empty();
        };
        std::printf("duo selftest\n");
        run_until(same, 5000);
        expect(ana.probe().nodes > 0, "the host starts with the starter net");
        expect(same(), "the joiner converged on the host's net");
        expect(healthy(ana) && healthy(bo), "no anomalies, conflicts, broken wires");
        std::printf("    ana: %s\n    bo:  %s\n", ana.probe().status.c_str(), bo.probe().status.c_str());

        auto pairs = ana.live_pairs();
        expect(!pairs.empty(), "there is an active pair to fire");
        if (!pairs.empty()) {
            expect(ana.fire(pairs[0].first, pairs[0].second), "Ana fires a step");
            run_until(same, 5000);
            expect(same(), "Bo follows Ana's step");
            expect(healthy(ana) && healthy(bo), "still healthy after a remote step");
        }

        // partition, one step each, heal
        link.cut = true;
        auto pa = ana.live_pairs();
        auto pb = bo.live_pairs();
        bool fired_a = false, fired_b = false;
        if (pa.size() >= 2 && pb.size() >= 2) {
            fired_a = ana.fire(pa[0].first, pa[0].second);
            fired_b = bo.fire(pb[1].first, pb[1].second); // a DIFFERENT pair
        }
        expect(fired_a && fired_b, "each side fires a different pair while partitioned");
        run_frames(20);
        expect(!same(), "while cut, the two nets differ");
        link.cut = false;
        run_until([&] { return same() && healthy(ana) && healthy(bo); }, 8000);
        expect(same(), "healed: one net on both sides");
        expect(healthy(ana) && healthy(bo), "healed: no anomalies, conflicts, broken wires");
        std::printf("    shape: %s\n", ana.probe().shape.c_str());

        // reduce to normal form on Ana; Bo follows
        for (int k = 0; k < 40 && !ana.live_pairs().empty(); ++k) {
            auto p = ana.live_pairs();
            ana.fire(p[0].first, p[0].second);
            run_frames(3);
        }
        run_until(same, 8000);
        expect(ana.live_pairs().empty(), "Ana reduced to normal form");
        expect(same(), "Bo holds the same normal form");
        expect(healthy(ana) && healthy(bo), "normal form: healthy on both");

        for (auto& s : seats) glfwSetWindowShouldClose(s.window, 1);
        std::printf(failures ? "duo selftest: %d FAILED\n" : "duo selftest: all ok\n", failures);
    }

    while (!glfwWindowShouldClose(seats[0].window) && !glfwWindowShouldClose(seats[1].window)) {
        glfwPollEvents();

        // deliver every frame whose latency has elapsed
        long long now = now_ms();
        while (!link.queue.empty() && link.queue.front().due <= now) {
            Link::Frame f = std::move(link.queue.front());
            link.queue.pop_front();
            seats[f.to].app.receive(names[1 - f.to], f.bytes);
        }

        for (int i = 0; i < 2; ++i) {
            render_seat(seats[i]);
            if (i == 0) {
                // the bench's own controls, in Ana's window
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->WorkSize.x - 300, 40),
                                        ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_FirstUseEver);
                ImGui::Begin("Duo link");
                ImGui::TextDisabled("the simulated network between the windows");
                ImGui::SliderInt("latency ms", &link.latency_ms, 0, 1000);
                ImGui::SliderInt("loss %", &link.loss_pct, 0, 90);
                ImGui::Checkbox("cut the link (partition)", &link.cut);
                ImGui::Text("frames: %zu sent, %zu dropped, %zu in flight", link.sent,
                            link.dropped, link.queue.size());
                ImGui::End();
            }
            finish_seat(seats[i]);
        }
    }

    for (auto& s : seats) {
        glfwMakeContextCurrent(s.window);
        ImGui::SetCurrentContext(s.ctx);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(s.ctx);
    }
    for (auto& s : seats) glfwDestroyWindow(s.window);
    glfwTerminate();
    return failures ? 1 : 0;
}
