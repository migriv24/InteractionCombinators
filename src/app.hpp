/* app.hpp — the Interaction Combinators application, platform-free.
 *
 * Everything the demo IS lives here: the core, the projection, the gesture
 * loop, projects, the rewrite animation, physics. The platform shells
 * (main_desktop.cpp: GLFW; android/src/main_android.cpp: NativeActivity)
 * own only the window, the GL context, and the input source — they call
 * frame() once per ImGui frame and provide the seams below. This split is
 * the substrate axis from okf/concepts/substrates.md made literal: a touch
 * pinch and a mouse wheel land on the same camera, and compile to the same
 * `config set view.camera` command.
 */
#pragma once

#include "voidmaiz/canvas.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/mobile.hpp"
#include "voidmaiz/updateview.hpp"
#include "voidmaiz/reduce.hpp"
#include "voidmaiz/widgets.hpp"
#include "voidmaiz/wires.hpp"

#ifdef IC_NET
#include "voidmaiz/net.hpp"
#include "voidmaiz/rnslink.hpp" // brings lanlink.hpp: the interface hints, the multicast lock
#endif

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

struct CombinatorsApp {
    // ── platform seams (set before init) ────────────────────────────────────
    std::filesystem::path base_dir;   // projects/ + the recents dotfile live here
    std::function<void(const std::string&)> on_title; // window title (optional)
    std::function<void()> on_quit;                    // File > Quit (optional)
    bool touch_mode = false; // mobile: fat-finger hit targets, no keyboard hints
    /* The shell's density scale (the Android shell: dpi/160 x 1.3). The layout is
     * decided in dp = px / ui_scale, so a 1080-px phone is laid out as the
     * ~317-dp screen it is (maiz::classify_layout). 1 on the desktop. */
    float ui_scale = 1.0f;

    // ── updating itself (VoidMaiz okf/concepts/updates.md) ───────────────────
    bool updates_enabled = true;            // the duo bench turns it off: a test rig
    std::filesystem::path install_dir;      // the running copy's folder (desktop shell)
    std::filesystem::path prefs_dir;        // per machine; default: the platform's app-data folder
    struct ANativeActivity* android_activity = nullptr; // the Android shell's, for JNI
    bool system_keyboard = false; // the shell drives Android's keyboard; draw none of our own

    // ── collaboration (VoidMaiz okf/concepts/collaborative-canvas.md) ─────────
    // Set before init. Solo is the app as it always was. HOST shares the net it
    // starts with; JOIN starts EMPTY (no mantle, no starter net) and adopts
    // what the first merge brings — a joiner that seeded its own net would mint a
    // second mantle id under the same name and conflict on every merge (E12).
    enum class Role { Solo, Host, Join };
    Role role = Role::Solo;
    std::string profile_name = "lafont"; // the log's actor is human:<profile_name>
    unsigned profile_rgb = 0x4f86d9;
    /* Scopes every name this device mints (wire segments, reduced agents) so two
     * devices never choose one name (duplicate_name, E13). Empty in solo play. */
    std::string device_tag;
    std::string replica_id; // Host/Join: 16+ chars [A-Za-z0-9-], random per launch
    /* Host/Join: deliver `frame` to the peer at the other end of `link`. The
     * shell owns the transport (an in-memory wire in the duo shell; LAN later). */
    std::function<void(const std::string& link, const std::string& frame)> send;
    void connect(const std::string& link);                          // a peer is reachable
    void receive(const std::string& link, const std::string& frame); // bytes arrived
    void disconnect(const std::string& link);
    std::function<long long()> now_ms; // defaults to a steady clock

    // ── for the duo bench's --selftest (no mouse; the real code paths) ──────
    struct Probe {
        int nodes = 0, wires = 0, contested = 0, pairs = 0, questions = 0;
        std::string status, net_err;
        std::string shape; // sorted node names + drawn wires: equal ⇔ same net
        std::string links; // per link: open/connecting/closed, and whether in sync
    };
    Probe probe() const;
    std::vector<std::pair<std::string, std::string>> live_pairs() const { return hot_pairs; }
    bool fire(const std::string& a, const std::string& b) { return try_step(a, b); }
    /* Test-only (desktop launch flags --lan-share / --lan-join / --lan-auto-allow):
     * drive the LAN panel's buttons without a mouse, so two real processes can be
     * checked talking over real sockets. */
    std::string test_lan;      // "share" | "join"
    /* Two devices on ONE machine (--rns-dir, --rns-port, --rns-forward): each needs
     * its own Reticulum identity (it lives in settings_dir, which both share) and
     * its own UDP port, forwarding to the other's. Empty/0 = the defaults. */
    std::string rns_dir, rns_forward;
    std::uint16_t rns_port = 0;
    double test_wire_at = 0;   // seconds after start: wire a demo net (self-loop, cycle)
    bool test_auto_allow = false;
    void open_lan_panel() { lan_open = true; }
    // screenshot/test hooks for the two things a gesture opens on glass
    double test_add_at = 0;   // seconds after start: open the add palette (what a long press does)
    void open_add_box() { test_add_at = 2.0; }
    void open_save_as() { want_save_as = true; }              // a field, so: the keyboard

    void init();  // build the core, the starter net, read view config
    void frame(); // one ImGui frame (between NewFrame and Render)

    // touch camera (called by the mobile shell; desktop uses the wheel):
    // pan by screen pixels; zoom by a factor around a screen focal point.
    // Both mark the camera dirty so edit_canvas's idle flush logs it.
    void touch_pan(float dx_px, float dy_px);
    void touch_zoom(float factor, float focal_sx, float focal_sy);

    bool light_mode = true; // shells read this for the clear color

private:
    // ── the model + projection ───────────────────────────────────────────────
    maiz::Core core;
    maiz::reduce::Spec spec;
    maiz::Scene scene;     // what the canvas draws: wire runes collapsed to wires
    maiz::Scene raw_scene; // the projection as stored (wire runes are nodes)
    maiz::WireEncoding wire_enc;
    std::vector<maiz::WireClass> wire_classes;
    unsigned long wire_counter = 1;
    std::string wire_tag() const; // the device tag, without its trailing dash
    std::string fresh_wire();
    void upgrade_wires(); // plain i:j edges → wire runes (old projects, the starter)
    maiz::EditorState ed;
    maiz::CanvasStyle canvas_style;
    maiz::CommandBarState cmdbar;
    maiz::AddPalette palette;
    std::vector<maiz::LogEntry> log;
    int active_count = 0;
    int undo_depth = 0;
    std::set<std::string> hot_agents;
    std::vector<std::pair<std::string, std::string>> hot_pairs;
    std::string last_net_err;

    // ── projects ─────────────────────────────────────────────────────────────
    std::filesystem::path current_path; // "" = untitled
    std::vector<std::string> recents;
    bool want_save_as = false;
    bool want_open = false;
    bool want_settings = false;
    bool settings_open = false;
    char save_name[128] = {};

    // ── view settings (config tier) ──────────────────────────────────────────
    float canvas_frac = 0.78f;
    float log_frac = 0.76f;
    bool anim_on = true;
    float anim_speed = 1.0f;

    // ── the rewrite animation (view ephemera) ────────────────────────────────
    struct StepAnim {
        bool active = false;
        float t = 0.0f;
        maiz::SceneNode ghost_a, ghost_b;
        float ax = 0, ay = 0, bx = 0, by = 0;
        float mx = 0, my = 0;
        struct Minted {
            std::string name;
            float tx = 0, ty = 0;
        };
        std::vector<Minted> minted;
    };
    StepAnim anim;
    bool auto_reduce = false;

    /* ── live physics: a RULE of the mantle, not a switch on this device ─────
     * Turning it on turns it on for everyone looking at this net, because it is
     * the net that is behaving differently (voidmaiz/rules.hpp). Both flags are
     * read out of the document in reproject(); nothing writes them directly.
     * One device — the driver, normally whoever switched it on — actually runs
     * the simulation and commits the settled positions; the others receive
     * those as ordinary moves. That is why the positions are not a stream. */
    bool physics_on = false;      // the rule is on, for this net
    bool physics_driving = false; // …and this device is the one simulating
    std::string physics_driver;   // who is (a device tag; empty = nobody named)
    void toggle_physics();
    maiz::PositionMap phys;
    std::map<std::string, std::pair<float, float>> model_pos;
    int settle_frames = 0;

    // last frame's canvas pane rect (screen px) — the touch shell's zoom needs
    // it to anchor the world point under the fingers
    float canvas_org_x = 0, canvas_org_y = 0, canvas_w_px = 0, canvas_h_px = 0;

    struct {
        bool valid = false;
        std::string a, b;
    } pending_fire;
    std::mt19937 rng{std::random_device{}()};

    // touch mode hides the log pane; errors surface as a transient toast
    size_t seen_log = 0;
    std::string toast;
    double toast_until = 0.0;

    // ── the session (Host/Join) ──────────────────────────────────────────────
#ifdef IC_NET
    std::unique_ptr<maiz::Network> net;
    maiz::Surfaces surfaces;
    bool adopted = false; // a joiner has `use`d the shared mantle
    void net_frame();     // tick, deliver, splice, play what changed
    bool start_network(); // a Network for the current role, over the current document

    // ── the LAN, over Reticulum (VoidMaiz rnslink.hpp; Q37, 2026-09-24) ──────
    // Encrypted links, and "allowed" kept by proven identity. The author chose
    // Reticulum only (no LanSession fallback), 2026-09-24.
    std::unique_ptr<maiz::RnsSession> lan;
    std::unique_ptr<maiz::lan::MulticastLock> mlock; // Android, while the LAN is open
    std::string lan_id;          // this device on the LAN (stable for the run)
    std::string lan_error;
    std::string lan_host_name;   // the host this device joined
    void prepare_identity();
    void lan_share();
    void lan_discover();
    void lan_join(const std::string& destination, const std::string& name);
    maiz::RnsOptions rns_options(bool host);
    void lan_leave();
    void lan_frame();
    void draw_lan_panel();
    void draw_net_health();
    // the phone sleeps, Wi-Fi hands over, a lid closes: come back by ourselves
    std::string last_host_dest; // the host's Reticulum destination
    long long next_retry_ms = 0;
    long long retry_delay_ms = 2000;
    bool reconnecting = false;
#endif
    bool lan_open = false; // the LAN panel
    std::string net_status; // the status pill: "solo", "in sync with 1", …
    long long clock_ms();

    // remote changes play instead of teleporting (view ephemera)
    struct RemoteTween {
        bool active = false;
        float t = 0.0f;
        struct Move { std::string name; float fx, fy, tx, ty; };
        std::vector<Move> moves;
        std::vector<maiz::SceneNode> gone;   // shrink out where they were
        std::vector<std::string> born;       // grow in
    };
    RemoteTween tween;
    void play_remote_change(const maiz::Scene& before);

    // ── internals ────────────────────────────────────────────────────────────
    void install_host();
    void apply_theme();
    void read_view_config();
    void flush_panels();
    void flush_anim_cfg();
    void reproject();
    std::string physics_pending();
    void flush_physics();
    maiz::Result dispatch_and_reproject(const std::string& cmd);
    void set_title_now();
    void reset_session();
    void load_project(const std::filesystem::path& p);
    void new_project();
    void do_save();
    bool try_step(const std::string& a = {}, const std::string& b = {});
    void draw_modals();
    void draw_menus();          // the menu content: a desktop menu bar, a phone's ⋮ menu
    void draw_identity();       // the session's colour dot, name and status
    void draw_actions(float width); // the action bar (step, undo, …), fitted to width
    maiz::BottomSheetState sheet;   // the inspector, on a phone held upright
    std::unique_ptr<maiz::update::Updater> updater;
    maiz::UpdateViewState update_view;
    maiz::KeyboardState keys; // the drawn keyboard, on glass only
    void init_updates();
    std::filesystem::path settings_dir(); // per machine: profile, update answers
    void load_profile_once();
    void rename_net(const std::string& name); // the net IS a mantle; name it
    std::string net_name() const { return scene.mantle; }
    char net_name_buf[48] = {};
    void fit_camera();              // frame the whole net in the canvas pane
    int fitted_orientation = -1;    // a phone refits when it is rotated
    std::filesystem::path projects_dir();
    std::filesystem::path recents_file();
    void push_recent(const std::string& path);
};
