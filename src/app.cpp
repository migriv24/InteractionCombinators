/* app.cpp — the Interaction Combinators application (see app.hpp).
 *
 * The whole stack in one place: the model is a Void Core mantle; the canvas
 * is Void Maiz's projection; every gesture — moves, wires, and the REWRITES
 * themselves — is a dispatched, logged, undoable command. A rewrite step runs
 * the maiz::reduce executor (the conformance-proven port) on the exported
 * mantle, then compiles the net difference back into ONE `batch`: `undo`
 * literally un-rewrites the net.
 *
 * The six rules (γγ, δδ, εε annihilate; γδ, γε, δε commute) live in a reduce
 * Spec — the same `config.transform.reduce` data form the contract pins. */
#include "app.hpp"

#include "voidmaiz/inspector.hpp"
#include "voidmaiz/project.hpp"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

namespace red = maiz::reduce;
namespace fs = std::filesystem;

namespace {

red::Spec make_spec() {
    // γγ annihilation is the index-SWAPPED one (x_i ≡ y_{n+1-i} — draws
    // parallel between the mirrored triangles); δδ is the straight one
    // (draws crossing). The author caught the demo doing δδ's wiring for
    // both (2026-07-13) — the asymmetry is what makes the calculus universal.
    return red::spec_from_json(R"({
        "signatures": {"gamma": 2, "delta": 2, "epsilon": 0},
        "rules": [
            {"glyphs": ["gamma", "gamma"],   "rule": "annihilate", "swap": true},
            {"glyphs": ["delta", "delta"],   "rule": "annihilate"},
            {"glyphs": ["epsilon", "epsilon"], "rule": "annihilate"},
            {"glyphs": ["gamma", "delta"],   "rule": "commute"},
            {"glyphs": ["gamma", "epsilon"], "rule": "commute"},
            {"glyphs": ["delta", "epsilon"], "rule": "commute"}
        ]})");
}

/* ── Tag pigments: tags ARE colors, mixed by a background interaction net ────
 * The author's design (2026-07-13): a "red" tag tints the node red; add
 * "yellow" and it becomes orange — RYB paint mixing, computed by interaction
 * nets running invisibly behind the UI: each color tag is an arity-0 agent,
 * two colors wired principal-to-principal form a redex, and a FUSE rule
 * rewrites the pair into the mixed pigment. */

const std::map<std::string, unsigned>& pigment_rgb() {
    static const std::map<std::string, unsigned> m = {
        {"red", 0xd43a3a},   {"yellow", 0xe6c34a}, {"blue", 0x3a62c4},
        {"orange", 0xe0862e}, {"green", 0x3a9a4a},  {"purple", 0x8a4ac4},
        {"brown", 0x7a5238},
    };
    return m;
}

red::Spec make_pigment_spec() {
    // primaries mix to secondaries; everything else muddies to brown
    // (one rule per unordered pair — the confluence guard)
    std::string json = R"({"signatures":{"red":0,"yellow":0,"blue":0,)"
                       R"("orange":0,"green":0,"purple":0,"brown":0},"rules":[)";
    auto rule = [](const char* a, const char* b, const char* into) {
        return std::string(R"({"glyphs":[")") + a + R"(",")" + b +
               R"("],"rule":"fuse","into":")" + into + R"("})";
    };
    std::vector<std::string> rules = {
        rule("red", "yellow", "orange"), rule("yellow", "blue", "green"),
        rule("red", "blue", "purple"),
    };
    const char* all[] = {"red", "yellow", "blue", "orange", "green", "purple", "brown"};
    std::set<std::pair<std::string, std::string>> covered = {
        {"red", "yellow"}, {"blue", "yellow"}, {"blue", "red"}};
    for (int i = 0; i < 7; ++i)
        for (int j = i + 1; j < 7; ++j) {
            // materialize before the temporaries die — minmax returns REFERENCES
            std::pair<std::string, std::string> key =
                std::minmax<std::string>(all[i], all[j]);
            if (covered.count(key)) continue;
            rules.push_back(rule(all[i], all[j], "brown"));
        }
    for (size_t i = 0; i < rules.size(); ++i) json += (i ? "," : "") + rules[i];
    json += "]}";
    return red::spec_from_json(json);
}

/* Fold a node's color tags through the pigment net: build the two-agent net,
 * let the redex fire, read the survivor, repeat. */
bool mix_tags(const std::vector<std::string>& tags, unsigned& out_rgb) {
    const auto& palette = pigment_rgb();
    static const red::Spec spec = make_pigment_spec();
    std::string current;
    for (const auto& t : tags) {
        if (!palette.count(t)) continue;
        if (current.empty()) {
            current = t;
            continue;
        }
        red::Net net;
        net.add({"a", current, 0, "{}", {}});
        net.add({"b", t, 0, "{}", {}});
        net.connect({"a", 0}, {"b", 0}); // the active pair
        red::Net done = red::reduce(spec, net);
        current = done.agents.begin()->second.glyph; // the fused survivor
    }
    if (current.empty()) return false;
    out_rgb = palette.at(current);
    return true;
}

void register_glyphs(maiz::Core& core) {
    // The notation itself: γ/δ are triangles whose apex IS the principal and
    // auto-rotates toward its wire partner; ε is a circle. (node-geometry.md)
    core.register_glyph(
        R"({"glyph":"gamma","label":"constructor","fields":[],)"
        R"("hints":{"color":"#c08a2e","face":{"w":96,"h":96},"shape":{"kind":"triangle"},)"
        R"("ports":[{"name":"prin","principal":true},)"
        R"({"name":"a","dir":"in"},{"name":"b","dir":"out"}]}})");
    core.register_glyph(
        R"({"glyph":"delta","label":"duplicator","fields":[],)"
        R"("hints":{"color":"#2e7d8a","face":{"w":96,"h":96},"shape":{"kind":"triangle"},)"
        R"("ports":[{"name":"prin","principal":true},)"
        R"({"name":"a","dir":"in"},{"name":"b","dir":"out"}]}})");
    core.register_glyph(
        R"({"glyph":"epsilon","label":"eraser","fields":[],)"
        R"("hints":{"color":"#8a3a3a","face":{"w":60,"h":60},"shape":{"kind":"circle"},)"
        R"("ports":[{"name":"prin","principal":true}]}})");
    // Connections are WIRE RUNES (voidmaiz/wires.hpp; Palabra SPEC 5.11), so two
    // devices reducing adjacent pairs at once still compose into one net. They
    // never draw as nodes: reproject collapses them into ordinary wires.
    core.register_glyph(R"({"glyph":"wire","label":"wire","fields":[]})");
}

/* The starter net: one γδ commute pair (feeding two ε erasers and two γ
 * anchors) and one γγ annihilation pair whose aux partners will be visibly
 * cross-linked. Every wire is a dispatched `link` with explicit "i:j". */
void build_starter(maiz::Core& core) {
    core.dispatch("mantle new lafont");
    auto mk = [&](const char* glyph, const char* name, int x, int y) {
        core.dispatch(std::string("rune new ") + glyph + " " + name);
        core.dispatch(maiz::compile_move(name, (float)x, (float)y));
        const char* pigment = std::string(glyph) == "gamma"   ? "orange"
                              : std::string(glyph) == "delta" ? "blue"
                                                              : "red";
        core.dispatch(maiz::compile_tag(name, pigment, true));
    };
    auto wire = [&](const char* a, int i, const char* b, int j) {
        core.dispatch("link " + std::string(a) + " " + b + " --relation " +
                      std::to_string(i) + ":" + std::to_string(j) + " --undirected");
    };
    mk("delta", "dup", 260, 120);
    mk("gamma", "con", 460, 120);
    mk("gamma", "anchor1", 60, 260);
    mk("gamma", "anchor2", 260, 300);
    mk("epsilon", "era1", 460, 300);
    mk("epsilon", "era2", 620, 260);
    wire("dup", 0, "con", 0); // the active pair (fettuccine)
    wire("dup", 1, "anchor1", 1);
    wire("dup", 2, "anchor2", 1);
    wire("con", 1, "era1", 0);
    wire("con", 2, "era2", 0);
    mk("gamma", "g1", 60, 470);
    mk("gamma", "g2", 300, 470);
    mk("delta", "left", 60, 620);
    mk("delta", "right", 300, 620);
    wire("g1", 0, "g2", 0);
    wire("g1", 1, "left", 1);
    wire("g2", 1, "right", 1);
}

/* What a fired step looked like — the animation replays it view-side. */
struct StepReport {
    std::string a, b;
    std::vector<std::string> minted;
};

/* One rewrite step: run the executor on the exported mantle, diff the nets,
 * compile the difference into ONE batch. See main.cpp history for the full
 * commentary; unchanged in the app split. */
/* How a step writes its wires. A boundary the rewrite inherits gets a NEW
 * segment fused onto the old wire; the old wire is never edited in place. That
 * is what makes two devices' adjacent steps compose (collaborative-canvas §4.2):
 * each writes a fact about its own end, and the merged class reads the wire
 * neither of them wrote. */
struct StepWiring {
    const std::vector<maiz::WireClass>* classes = nullptr;
    const maiz::WireEncoding* enc = nullptr;
    std::function<std::string()> fresh; // a new segment name
    std::string tag;                    // scopes minted agent names ("" solo)
};

bool compile_step(const red::Spec& spec, const maiz::Scene& scene, const StepWiring& wiring,
                  std::string& out_batch, const std::string& chosen = {},
                  const std::string& chosen_b = {}, StepReport* report = nullptr) {
    red::Net net = red::to_net(scene, spec.signatures); // the net as drawn
    auto pairs = red::active_pairs(spec, net);
    if (pairs.empty()) return false;
    auto redex = pairs.front();
    if (!chosen.empty()) {
        bool found = false;
        for (const auto& p : pairs) {
            bool match = chosen_b.empty()
                             ? p.first == chosen || p.second == chosen
                             : (p.first == chosen && p.second == chosen_b) ||
                                   (p.first == chosen_b && p.second == chosen);
            if (match) {
                redex = p;
                found = true;
                break;
            }
        }
        if (!found) return false; // the chosen agent(s) aren't an active pair
    }
    red::Net after = red::step(spec, net, redex);
    if (report) {
        report->a = redex.first;
        report->b = redex.second;
        report->minted.clear();
    }

    std::vector<std::string> cmds{"rm " + redex.first, "rm " + redex.second};

    float bx = 0, by = 0;
    if (const maiz::SceneNode* n = scene.find(redex.first)) { bx += n->x; by += n->y; }
    if (const maiz::SceneNode* n = scene.find(redex.second)) { bx += n->x; by += n->y; }
    bx *= 0.5f;
    by *= 0.5f;

    std::map<std::string, std::string> rename;
    auto taken = [&](const std::string& nm) {
        if (after.agents.count(nm) || net.agents.count(nm)) return true;
        for (const auto& [k, v] : rename)
            if (v == nm) return true;
        return false;
    };
    int minted = 0;
    for (const auto& [id, agent] : after.agents) {
        if (net.agents.count(id)) continue;
        std::string name;
        for (int k = 1;; ++k) {
            name = agent.glyph + "-" + wiring.tag + std::to_string(k);
            if (!taken(name)) break;
        }
        rename[id] = name;
        if (report) report->minted.push_back(name);
        cmds.push_back("rune new " + agent.glyph + " " + name);
        // Pigment inheritance (demo policy — the CONTRACT's copies start
        // tagless; the app re-tags minted copies from their same-glyph
        // redex parent). Visible in the batch.
        {
            const red::Agent& ra = net.agents.at(redex.first);
            const red::Agent& rb = net.agents.at(redex.second);
            const red::Agent& parent = agent.glyph == ra.glyph ? ra : rb;
            for (const auto& t : parent.tags)
                cmds.push_back(maiz::compile_tag(name, t, true));
        }
        float px = bx - 90.0f + 150.0f * (minted % 2); // fallback fan
        float py = by + 130.0f + 90.0f * (minted / 2);
        if (const red::Port* pp = after.partner({id, 0})) {
            if (net.agents.count(pp->first)) {
                if (const maiz::SceneNode* partner = scene.find(pp->first)) {
                    px = bx + (partner->x + 45.0f - bx) * 0.55f - 45.0f;
                    py = by + (partner->y + 45.0f - by) * 0.55f - 45.0f;
                }
            }
        }
        cmds.push_back(maiz::compile_move(name, px, py));
        ++minted;
    }
    auto model = [&](const std::string& id) {
        auto it = rename.find(id);
        return it == rename.end() ? id : it->second;
    };

    auto wire_set = [](const red::Net& n) {
        std::set<std::string> out;
        for (const auto& [p, q] : n.link) {
            if (!(p <= q)) continue;
            out.insert(p.first + "\x1f" + std::to_string(p.second) + "\x1f" + q.first +
                       "\x1f" + std::to_string(q.second));
        }
        return out;
    };
    std::set<std::string> before = wire_set(net);
    // the segment an OLD agent's port hangs on (none: a free port, or a plain
    // edge from a document not yet upgraded)
    auto seg_of = [&](const red::Port& port) -> std::string {
        if (!wiring.classes) return {};
        maiz::WireEnd e{port.first, port.second};
        for (const auto& c : *wiring.classes)
            for (std::size_t k = 0; k < c.ends.size(); ++k)
                if (c.ends[k] == e) return c.end_segment[k];
        return {};
    };
    const maiz::WireEncoding& enc = *wiring.enc;
    for (const auto& [p, q] : after.link) {
        if (!(p <= q)) continue;
        std::string key = p.first + "\x1f" + std::to_string(p.second) + "\x1f" + q.first +
                          "\x1f" + std::to_string(q.second);
        if (before.count(key)) continue;
        maiz::WireEnd pe{model(p.first), p.second}, qe{model(q.first), q.second};
        bool p_old = net.agents.count(p.first) > 0, q_old = net.agents.count(q.first) > 0;
        std::string sp = p_old ? seg_of(p) : std::string{};
        std::string sq = q_old ? seg_of(q) : std::string{};
        if (p_old && q_old && !sp.empty() && !sq.empty()) {
            // an annihilation joining two boundaries: the two wires are now one
            cmds.push_back(maiz::compile_fuse(enc, sp, sq));
            continue;
        }
        // otherwise a new segment carries the new end(s), fused onto any old wire
        std::string seg = wiring.fresh();
        cmds.push_back(maiz::compile_segment(enc, seg));
        for (auto [end, old_seg] : {std::pair{pe, sp}, std::pair{qe, sq}}) {
            if (!old_seg.empty()) cmds.push_back(maiz::compile_fuse(enc, seg, old_seg));
            else cmds.push_back(maiz::compile_attach(seg, end));
        }
    }
    out_batch = maiz::compile_batch(cmds);
    return true;
}

std::string json_str(const std::string& s) { // escape a path for JSON
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

/* Parse "a b" pairs out of a config value (tolerates JSON quotes). */
bool parse_two(std::string_view v, float& a, float& b) {
    while (!v.empty() && (v.front() == '"' || v.front() == ' ')) v.remove_prefix(1);
    while (!v.empty() && (v.back() == '"' || v.back() == ' ')) v.remove_suffix(1);
    std::string owned(v);
    return std::sscanf(owned.c_str(), "%f %f", &a, &b) == 2;
}

float ease(float t) { return t * t * (3.0f - 2.0f * t); } // smoothstep

void node_center(const maiz::SceneNode& n, float& cx, float& cy) {
    float w = n.w > 0 ? n.w : 84.0f;
    float h = n.h > 0 ? n.h : 84.0f;
    cx = n.x + w * 0.5f;
    cy = n.y + h * 0.5f;
}

/* A keyboard-free fallback name: net-YYYYMMDD-HHMMSS (mobile has no soft
 * keyboard in v1; desktop users just overtype it). */
std::string auto_name() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[48];
    std::strftime(buf, sizeof buf, "net-%Y%m%d-%H%M%S", &tm);
    return buf;
}

} // namespace

// ── wires as runes ───────────────────────────────────────────────────────────

std::string CombinatorsApp::fresh_wire() {
    return maiz::fresh_wire_name(device_tag.empty() ? "l" : device_tag, wire_counter++);
}

void CombinatorsApp::upgrade_wires() {
    std::vector<std::string> cmds =
        maiz::compile_upgrade(raw_scene, wire_enc, [this] { return fresh_wire(); });
    if (cmds.empty()) return;
    core.dispatch(maiz::compile_batch(cmds));
    reproject();
}

long long CombinatorsApp::clock_ms() {
    if (now_ms) return now_ms();
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ── projects on disk ─────────────────────────────────────────────────────────

fs::path CombinatorsApp::projects_dir() {
    fs::path p = (base_dir.empty() ? fs::current_path() : base_dir) / "projects";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

fs::path CombinatorsApp::recents_file() { return projects_dir() / ".recent"; }

void CombinatorsApp::push_recent(const std::string& path) {
    recents.erase(std::remove(recents.begin(), recents.end(), path), recents.end());
    recents.insert(recents.begin(), path);
    if (recents.size() > 8) recents.resize(8);
    std::ofstream out(recents_file(), std::ios::trunc);
    for (const auto& r : recents) out << r << "\n";
}

// ── host seams on the core (survive core replacement) ────────────────────────

void CombinatorsApp::install_host() {
    core.set_log_sink([this](std::string_view level, std::string_view op,
                             std::string_view msg) {
        log.push_back({std::string(level), std::string(op), std::string(msg)});
    });
    // the effect seam: `save` hands the host the exported state; writing it
    // to disk is the world-facing half the core can't do itself
    core.set_effect_handler([this](std::string_view op, std::string_view args)
                                -> std::string {
        if (op != "save") return {};
        if (current_path.empty()) return {}; // guarded before dispatch
        std::ofstream out(current_path, std::ios::binary | std::ios::trunc);
        out << args;
        if (!out) {
            log.push_back({"error", "save", "write failed: " + current_path.string()});
            return {};
        }
        return json_str(current_path.string());
    });
}

void CombinatorsApp::apply_theme() {
    if (light_mode) {
        ImGui::StyleColorsLight();
        canvas_style.theme = maiz::CanvasTheme::light();
    } else {
        ImGui::StyleColorsDark();
        canvas_style.theme = maiz::CanvasTheme::dark();
    }
}

void CombinatorsApp::read_view_config() {
    // touch starts zoomed in: 96px-world bodies are finger targets, not dots
    ed.cam = {-10, -10, touch_mode ? 1.6f : 1.0f};
    if (maiz::Camera saved;
        maiz::parse_camera(core.dispatch("config get view.camera").data, saved))
        ed.cam = saved;
    float a, b;
    if (parse_two(core.dispatch("config get view.panels").data, a, b)) {
        canvas_frac = std::clamp(a, 0.4f, 0.92f);
        log_frac = std::clamp(b, 0.3f, 0.92f);
    }
    if (parse_two(core.dispatch("config get view.anim").data, a, b)) {
        anim_on = a != 0.0f;
        anim_speed = std::clamp(b, 0.25f, 3.0f);
    }
}

void CombinatorsApp::flush_panels() {
    char buf[96];
    std::snprintf(buf, sizeof buf, "config set view.panels \"%.3f %.3f\"", canvas_frac,
                  log_frac);
    core.dispatch(buf);
}

void CombinatorsApp::flush_anim_cfg() {
    char buf[96];
    std::snprintf(buf, sizeof buf, "config set view.anim \"%d %.2f\"", anim_on ? 1 : 0,
                  anim_speed);
    core.dispatch(buf);
}

// ── projection + net analysis ────────────────────────────────────────────────

void CombinatorsApp::reproject() {
    raw_scene = maiz::project_scene(core);
    scene = maiz::collapse_wires(raw_scene, wire_enc, &wire_classes);
    wire_counter = std::max(wire_counter,
                            maiz::next_wire_counter(raw_scene, device_tag.empty() ? "l" : device_tag));
    model_pos.clear();
    for (const auto& n : scene.nodes) model_pos[n.name] = {n.x, n.y};
    phys.clear(); // the model moved: physics restages from truth
    settle_frames = 0;
    active_count = 0;
    hot_agents.clear();
    hot_pairs.clear();
    // guarded: net analysis failures degrade to "no hot pairs", never a crash
    try {
        red::Net net = red::to_net(scene, spec.signatures);
        hot_pairs = red::active_pairs(spec, net);
        active_count = (int)hot_pairs.size();
        for (const auto& [a, b] : hot_pairs) {
            hot_agents.insert(a);
            hot_agents.insert(b);
        }
        std::set<std::pair<std::string, std::string>> hot(hot_pairs.begin(),
                                                          hot_pairs.end());
        for (auto& w : scene.wires)
            if (w.kind == maiz::SceneWire::Kind::Fettuccine) {
                auto key = w.from <= w.to ? std::make_pair(w.from, w.to)
                                          : std::make_pair(w.to, w.from);
                w.active = hot.count(key) != 0;
            }
        last_net_err.clear();
    } catch (const std::exception& e) {
        if (last_net_err != e.what()) {
            last_net_err = e.what();
            log.push_back({"error", "net", e.what()});
        }
    }
    for (auto& n : scene.nodes) {
        unsigned rgb = 0;
        if (mix_tags(n.tags, rgb)) {
            n.rgb = rgb;
            n.has_color = true;
        }
    }
    maiz::Result h = core.dispatch("history");
    undo_depth =
        (h.lines.size() == 1 && h.lines[0] == "(no history)") ? 0 : (int)h.lines.size();
}

std::string CombinatorsApp::physics_pending() {
    maiz::MoveList moves;
    for (const auto& n : scene.nodes) {
        auto it = phys.find(n.name);
        auto mt = model_pos.find(n.name);
        if (it == phys.end() || mt == model_pos.end()) continue;
        float dx = it->second.first - mt->second.first;
        float dy = it->second.second - mt->second.second;
        if (dx * dx + dy * dy < 1.0f) continue;
        moves.push_back({n.name, {it->second.first, it->second.second}});
    }
    return maiz::compile_moves(moves);
}

void CombinatorsApp::flush_physics() {
    std::string cmd = physics_pending();
    if (!cmd.empty()) {
        core.dispatch(cmd);
        reproject();
    }
}

maiz::Result CombinatorsApp::dispatch_and_reproject(const std::string& cmd) {
    if (physics_on) flush_physics(); // a settle never interleaves another story
    maiz::Result r = core.dispatch(cmd);
    reproject();
    return r;
}

// ── project lifecycle ────────────────────────────────────────────────────────

void CombinatorsApp::set_title_now() {
    if (!on_title) return;
#ifndef IC_VERSION
#define IC_VERSION "dev"
#endif
    on_title("Interaction Combinators " IC_VERSION " — " +
             (current_path.empty() ? std::string("untitled")
                                   : current_path.stem().string()));
}

void CombinatorsApp::reset_session() {
    ed = maiz::EditorState{};
    anim = StepAnim{};
    auto_reduce = false;
    physics_on = false;
    phys.clear();
    read_view_config();
    reproject();
    set_title_now();
}

void CombinatorsApp::load_project(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        log.push_back({"error", "open", "cannot read " + p.string()});
        return;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    core = maiz::Core(ss.str()); // replay the saved state document
    install_host();
    register_glyphs(core); // glyphs are host config, not exported state
    current_path = p;
    push_recent(p.string());
    log.push_back({">", "open", p.string()});
    reset_session();
    upgrade_wires(); // a project saved with plain edges opens as wire runes
}

void CombinatorsApp::new_project() {
    core = maiz::Core();
    install_host();
    core.dispatch("config set actor " + maiz::arg("human:" + profile_name));
    register_glyphs(core);
    core.dispatch("mantle new lafont"); // an empty net; add agents from the menu
    current_path.clear();
    log.push_back({">", "new", "fresh project"});
    reset_session();
}

void CombinatorsApp::do_save() {
    if (current_path.empty()) {
        want_save_as = true; // no path yet: route through Save As
        return;
    }
    dispatch_and_reproject("save"); // the effect handler writes the file
    push_recent(current_path.string());
}

// ── one rewrite step, animated ───────────────────────────────────────────────

bool CombinatorsApp::try_step(const std::string& a, const std::string& b) {
    anim.active = false; // a new fire snaps any running replay
    try {
        std::string batch;
        StepReport rep;
        StepWiring wiring{&wire_classes, &wire_enc, [this] { return fresh_wire(); }, device_tag};
        if (compile_step(spec, scene, wiring, batch, a, b, &rep)) {
            StepAnim next;
            if (const maiz::SceneNode* n = scene.find(rep.a)) {
                next.ghost_a = *n;
                node_center(*n, next.ax, next.ay);
            }
            if (const maiz::SceneNode* n = scene.find(rep.b)) {
                next.ghost_b = *n;
                node_center(*n, next.bx, next.by);
            }
            next.mx = (next.ax + next.bx) * 0.5f;
            next.my = (next.ay + next.by) * 0.5f;
            float deg =
                std::atan2(next.by - next.ay, next.bx - next.ax) * 180.0f / 3.14159265f;
            next.ghost_a.rot_auto = false;
            next.ghost_a.rot = deg;
            next.ghost_b.rot_auto = false;
            next.ghost_b.rot = deg + 180.0f;
            dispatch_and_reproject(batch);
            if (anim_on) {
                for (const auto& name : rep.minted) {
                    if (const maiz::SceneNode* n = scene.find(name)) {
                        StepAnim::Minted m;
                        m.name = name;
                        node_center(*n, m.tx, m.ty);
                        next.minted.push_back(m);
                    }
                }
                next.active = true;
                anim = next;
            }
            return true;
        }
    } catch (const std::exception& e) {
        log.push_back({"error", "reduce", e.what()});
    }
    return false;
}

// ── touch camera (the mobile shell's pinch/pan lands here) ───────────────────

void CombinatorsApp::touch_pan(float dx_px, float dy_px) {
    ed.cam.x -= dx_px / ed.cam.zoom;
    ed.cam.y -= dy_px / ed.cam.zoom;
    ed.cam_dirty = true;
    ed.last_zoom_time = ImGui::GetTime(); // reuse the wheel-idle flush
}

void CombinatorsApp::touch_zoom(float factor, float focal_sx, float focal_sy) {
    // world anchor under the fingers, via last frame's canvas pane rect
    float wx = ed.cam.x + (focal_sx - canvas_org_x) / ed.cam.zoom;
    float wy = ed.cam.y + (focal_sy - canvas_org_y) / ed.cam.zoom;
    ed.cam.zoom = std::clamp(ed.cam.zoom * factor, canvas_style.min_zoom,
                             canvas_style.max_zoom);
    ed.cam.x = wx - (focal_sx - canvas_org_x) / ed.cam.zoom;
    ed.cam.y = wy - (focal_sy - canvas_org_y) / ed.cam.zoom;
    ed.cam_dirty = true;
    ed.last_zoom_time = ImGui::GetTime();
}

// ── updating itself ──────────────────────────────────────────────────────────

/* Who this app is to the update feed Void Mago writes, and where its answer to
 * "may I check?" lives: per machine, outside every project, so it survives the
 * update it was given for (never in the document: config rides the document). */
void CombinatorsApp::init_updates() {
    if (!updates_enabled) return;
    maiz::update::AppIdentity self;
    self.app = "interactioncombinators";
    self.display = "Interaction Combinators";
    self.version = IC_VERSION;
    self.feed_url =
        "https://github.com/migriv24/InteractionCombinators/releases/latest/download/void-updates.json";
    self.install_dir = install_dir;
#ifdef _WIN32
    self.executable = "interaction_combinators.exe";
#else
    self.executable = "interaction_combinators";
#endif
    fs::path dir = prefs_dir;
    if (dir.empty()) {
        if (android_activity) {
            dir = base_dir / "updates"; // internal storage survives an APK update
        } else {
#ifdef _WIN32
            const char* base = std::getenv("LOCALAPPDATA");
            dir = fs::path(base ? base : ".") / "InteractionCombinators";
#else
            const char* base = std::getenv("XDG_CONFIG_HOME");
            const char* home = std::getenv("HOME");
            dir = base ? fs::path(base) / "interactioncombinators"
                       : fs::path(home ? home : ".") / ".config" / "interactioncombinators";
#endif
        }
    }
    self.prefs_dir = dir;
    maiz::update::Http http = android_activity ? maiz::update::android_http(android_activity)
                                               : maiz::update::curl_http();
    updater = std::make_unique<maiz::update::Updater>(self, http);
    updater->start_up(); // checks ONLY if the person already chose that
}

// ── boot ─────────────────────────────────────────────────────────────────────

void CombinatorsApp::init() {
    apply_theme();
    if (touch_mode) {
        // fat-finger targets: port grabs and wire hovers need real radius
        canvas_style.port_hit_radius = 24.0f;
        canvas_style.click_slop = 10.0f;
        canvas_style.hover_tooltips = false; // the pointer never leaves a tap
    }
    install_host();
    core.dispatch("config set actor " + maiz::arg("human:" + profile_name));
    register_glyphs(core);
    // a JOINER starts empty and adopts the shared mantle when it arrives (E12)
    if (role != Role::Join) build_starter(core);
    spec = make_spec();
    canvas_style.wires = maiz::reified_writer(
        wire_enc, [this] { return fresh_wire(); },
        [this]() -> const std::vector<maiz::WireClass>& { return wire_classes; });
    palette.entries = {{"gamma", "constructor γ", ""}, {"delta", "duplicator δ", ""},
                       {"epsilon", "eraser ε", ""}}; // one family: no categories
    recents = [this] {
        std::vector<std::string> out;
        std::ifstream in(recents_file());
        std::string line;
        while (std::getline(in, line))
            if (!line.empty() && fs::exists(line)) out.push_back(line);
        return out;
    }();
    read_view_config();
    reproject();
    if (role != Role::Join) upgrade_wires(); // the starter is written with plain links
    init_updates();
    net_status = "solo";
#ifdef IC_NET
    if (role != Role::Solo) {
        voidpalabra::Replica r;
        std::string why;
        if (!voidpalabra::Replica::create(replica_id, r, &why)) {
            log.push_back({"error", "net", "replica: " + why});
        } else {
            maiz::NetOptions o;
            fs::path store = (base_dir.empty() ? fs::current_path() : base_dir) /
                             ("replica-" + (device_tag.empty() ? std::string("l") : device_tag) + ".bin");
            o.persist = [store](const std::string& bytes) {
                // atomic: write beside, then rename over
                fs::path tmp = store;
                tmp += ".tmp";
                { std::ofstream out(tmp, std::ios::binary | std::ios::trunc); out << bytes; }
                std::error_code ec;
                fs::rename(tmp, store, ec);
            };
            o.timing.coalesce = 100; // "reduce all" emits a step per frame
            net = std::make_unique<maiz::Network>(core, std::move(r), std::move(o));
            net->settings().self.name = profile_name;
            net->settings().self.rgb = profile_rgb;
            net_status = role == Role::Host ? "hosting, nobody here yet" : "looking for the host";
        }
    }
#endif
    set_title_now();
}

// ── the session ──────────────────────────────────────────────────────────────

void CombinatorsApp::connect(const std::string& link) {
#ifdef IC_NET
    if (net) net->connect(link, clock_ms());
#else
    (void)link;
#endif
}

void CombinatorsApp::receive(const std::string& link, const std::string& frame) {
#ifdef IC_NET
    if (net) net->receive(link, frame, clock_ms());
#else
    (void)link;
    (void)frame;
#endif
}

void CombinatorsApp::disconnect(const std::string& link) {
#ifdef IC_NET
    if (net) net->disconnect(link, clock_ms());
#else
    (void)link;
#endif
}

#ifdef IC_NET
void CombinatorsApp::net_frame() {
    if (!net) return;
    long long now = clock_ms();
    net->tick(now, maiz::selection_ids(scene, ed.selection), surfaces);
    if (send)
        for (auto& out : net->take_outgoing()) send(out.link, out.frame);
    for (auto& n : net->take_notes()) log.push_back({n.level, "net", n.text});

    if (net->take_spliced()) {
        maiz::Scene before = scene;
        reproject();
        if (!adopted && role == Role::Join && core.dispatch("use lafont").ok) {
            adopted = true;
            reproject();
        }
        play_remote_change(before);
    }

    // the status pill
    int open = 0, synced = 0;
    for (const auto& l : net->links())
        if (l.open && !l.closed) {
            ++open;
            synced += l.in_sync ? 1 : 0;
        }
    std::size_t questions = net->conflicts().size() + net->anomalies().size();
    if (questions) net_status = std::to_string(questions) + " to resolve";
    else if (!open) net_status = role == Role::Host ? "hosting, nobody here yet" : "looking for the host";
    else if (synced == open) net_status = "in sync with " + std::to_string(open);
    else net_status = "syncing";
}
#endif

/* A merge changed the net: play it instead of teleporting. A remote STEP (an
 * active pair gone, agents minted) plays the same rewrite animation a local one
 * does; anything else tweens — moved nodes glide, removed ones shrink out, new
 * ones grow in. (collaborative-canvas §3.4, R1/R2; 250 ms per the UI decisions.) */
void CombinatorsApp::play_remote_change(const maiz::Scene& before) {
    std::set<std::string> was, now;
    for (const auto& n : before.nodes) was.insert(n.name);
    for (const auto& n : scene.nodes) now.insert(n.name);
    std::vector<const maiz::SceneNode*> gone;
    std::vector<std::string> born;
    for (const auto& n : before.nodes)
        if (!now.count(n.name)) gone.push_back(&n);
    for (const auto& n : scene.nodes)
        if (!was.count(n.name)) born.push_back(n.name);

    // a remote step: exactly the two agents of a pair that faced each other
    if (anim_on && gone.size() == 2) {
        bool faced = false;
        for (const auto& w : before.wires)
            if (w.kind == maiz::SceneWire::Kind::Fettuccine &&
                ((w.from == gone[0]->name && w.to == gone[1]->name) ||
                 (w.from == gone[1]->name && w.to == gone[0]->name)))
                faced = true;
        if (faced) {
            StepAnim next;
            next.ghost_a = *gone[0];
            next.ghost_b = *gone[1];
            node_center(*gone[0], next.ax, next.ay);
            node_center(*gone[1], next.bx, next.by);
            next.mx = (next.ax + next.bx) * 0.5f;
            next.my = (next.ay + next.by) * 0.5f;
            float deg = std::atan2(next.by - next.ay, next.bx - next.ax) * 180.0f / 3.14159265f;
            next.ghost_a.rot_auto = false;
            next.ghost_a.rot = deg;
            next.ghost_b.rot_auto = false;
            next.ghost_b.rot = deg + 180.0f;
            for (const auto& name : born)
                if (const maiz::SceneNode* n = scene.find(name)) {
                    StepAnim::Minted m;
                    m.name = name;
                    node_center(*n, m.tx, m.ty);
                    next.minted.push_back(m);
                }
            next.active = true;
            anim = next; // a newer remote step snaps an older one (R5)
            return;
        }
    }

    RemoteTween t;
    for (const auto& n : scene.nodes) {
        const maiz::SceneNode* b = before.find(n.name);
        if (!b) continue;
        float fx, fy, tx, ty;
        node_center(*b, fx, fy);
        node_center(n, tx, ty);
        if ((fx - tx) * (fx - tx) + (fy - ty) * (fy - ty) > 1.0f) t.moves.push_back({n.name, fx, fy, tx, ty});
    }
    for (const auto* g : gone) t.gone.push_back(*g);
    t.born = born;
    t.active = !t.moves.empty() || !t.gone.empty() || !t.born.empty();
    tween = t;
}

CombinatorsApp::Probe CombinatorsApp::probe() const {
    Probe p;
    p.nodes = (int)scene.nodes.size();
    p.wires = (int)scene.wires.size();
    for (const auto& w : scene.wires) p.contested += w.contested ? 1 : 0;
    p.pairs = active_count;
    p.status = net_status;
    p.net_err = last_net_err;
#ifdef IC_NET
    if (net) p.questions = (int)(net->conflicts().size() + net->anomalies().size());
#endif
    std::vector<std::string> parts;
    for (const auto& n : scene.nodes) parts.push_back("n:" + n.name);
    for (const auto& w : scene.wires) {
        std::string a = w.from + "." + std::to_string(w.from_port);
        std::string b = w.to + "." + std::to_string(w.to_port);
        if (b < a) std::swap(a, b);
        parts.push_back("w:" + a + "-" + b);
    }
    std::sort(parts.begin(), parts.end());
    for (const auto& x : parts) p.shape += x + " ";
    return p;
}

void CombinatorsApp::draw_menus() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New project")) new_project();
        if (ImGui::MenuItem("Open project…")) want_open = true;
        if (ImGui::BeginMenu("Recent projects", !recents.empty())) {
            for (const auto& r : recents)
                if (ImGui::MenuItem(fs::path(r).stem().string().c_str())) {
                    load_project(r);
                    break; // recents just changed; the copy is stale
                }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save", touch_mode ? nullptr : "Ctrl+S")) do_save();
        if (ImGui::MenuItem("Save as…")) want_save_as = true;
        if (on_quit) {
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) on_quit();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", touch_mode ? nullptr : "Ctrl+Z", false,
                            undo_depth > 0))
            dispatch_and_reproject("undo");
        if (ImGui::MenuItem("Redo", touch_mode ? nullptr : "Ctrl+Y"))
            dispatch_and_reproject("redo");
        ImGui::Separator();
        if (ImGui::MenuItem("Clean view")) {
            std::string cmd = maiz::compile_clean(scene, 96.0f, 96.0f);
            if (!cmd.empty()) dispatch_and_reproject(cmd);
        }
        if (ImGui::MenuItem("Relax layout")) {
            std::string cmd = maiz::compile_relax(scene);
            if (!cmd.empty()) dispatch_and_reproject(cmd);
        }
        if (ImGui::MenuItem("Live physics", nullptr, physics_on)) {
            physics_on = !physics_on;
            if (!physics_on) flush_physics(); // leaving: land the picture
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Selection")) {
        if (ImGui::MenuItem("Select all")) {
            ed.selection.clear();
            for (const auto& n : scene.nodes) ed.selection.push_back(n.name);
        }
        if (ImGui::MenuItem("Select none", nullptr, false, !ed.selection.empty()))
            ed.selection.clear();
        if (ImGui::MenuItem("Invert selection")) {
            std::vector<std::string> inv;
            for (const auto& n : scene.nodes)
                if (!ed.selected(n.name)) inv.push_back(n.name);
            ed.selection = inv;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete selection", touch_mode ? nullptr : "Del", false,
                            !ed.selection.empty())) {
            dispatch_and_reproject(maiz::compile_deletes(ed.selection));
            ed.selection.clear();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Add")) {
        // touch has no Shift+A and no right-click: minting from the menu
        // lands the node at the center of the visible canvas
        for (const auto& entry : palette.entries)
            if (ImGui::MenuItem(entry.label.c_str())) {
                float wx = ed.cam.x + canvas_w_px * 0.5f / ed.cam.zoom;
                float wy = ed.cam.y + canvas_h_px * 0.5f / ed.cam.zoom;
                dispatch_and_reproject(maiz::compile_add(
                    entry.glyph, maiz::unique_name(scene, entry.glyph), wx, wy));
            }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Light theme", nullptr, light_mode)) {
            light_mode = !light_mode;
            apply_theme();
        }
        if (ImGui::MenuItem("Reset camera")) {
            ed.cam = {-10, -10, 1.0f};
            dispatch_and_reproject(maiz::compile_camera(ed.cam));
        }
        ImGui::Separator();
        if (updater && ImGui::MenuItem("Check for updates…"))
            maiz::open_update_prompt(*updater, update_view, true);
        if (ImGui::MenuItem("Settings…")) want_settings = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Net")) {
        if (ImGui::MenuItem("Step", touch_mode ? nullptr : "Space", false,
                            active_count > 0))
            try_step();
        if (ImGui::MenuItem("Reduce all", nullptr, false, active_count > 0))
            auto_reduce = true;
        if (ImGui::MenuItem("Stop reducing", nullptr, false, auto_reduce))
            auto_reduce = false;
        if (!touch_mode) {
            ImGui::Separator();
            ImGui::MenuItem("Space fires the hovered pair, else a random one",
                            nullptr, false, false);
        }
        ImGui::EndMenu();
    }
}

/* Frame the whole net in the canvas pane. A phone does this when the canvas
 * first appears and every time it is rotated: a fixed starting camera left half
 * the starter net off the edge of an upright phone, and a rotation would strand
 * the view wherever it happened to be. It is an ordinary camera change, logged
 * through the config tier like a pinch. */
void CombinatorsApp::fit_camera() {
    if (scene.nodes.empty() || canvas_w_px < 10 || canvas_h_px < 10) return;
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (const auto& n : scene.nodes) {
        float w = n.w > 0 ? n.w : 84.0f, h = n.h > 0 ? n.h : 84.0f;
        x0 = std::min(x0, n.x);
        y0 = std::min(y0, n.y);
        x1 = std::max(x1, n.x + w);
        y1 = std::max(y1, n.y + h + 18.0f); // the tag label under a body
    }
    const float margin = 24.0f;
    x0 -= margin; y0 -= margin; x1 += margin; y1 += margin;
    float zoom = std::min(canvas_w_px / (x1 - x0), canvas_h_px / (y1 - y0));
    zoom = std::clamp(zoom, canvas_style.min_zoom, std::min(canvas_style.max_zoom, 1.6f));
    ed.cam.zoom = zoom;
    ed.cam.x = x0 - (canvas_w_px / zoom - (x1 - x0)) * 0.5f;
    ed.cam.y = y0 - (canvas_h_px / zoom - (y1 - y0)) * 0.5f;
    ed.cam_dirty = true;
    ed.last_zoom_time = ImGui::GetTime(); // the idle flush logs it
}

void CombinatorsApp::draw_identity() {
    if (role == Role::Solo) return;
    // the dot is drawn, not typed: the bundled font has no U+25CF
    ImVec4 col(((profile_rgb >> 16) & 255) / 255.0f, ((profile_rgb >> 8) & 255) / 255.0f,
               (profile_rgb & 255) / 255.0f, 1.0f);
    float r = ImGui::GetTextLineHeight() * 0.32f;
    ImVec2 at = ImGui::GetCursorScreenPos();
    float cy = at.y + ImGui::GetFrameHeight() * 0.5f;
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(at.x + r, cy), r,
                                                ImGui::ColorConvertFloat4ToU32(col));
    ImGui::Dummy(ImVec2(r * 2.0f, ImGui::GetFrameHeight()));
    ImGui::SameLine(0, 4);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(col, "%s", profile_name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", net_status.c_str());
}

/* One action bar for both substrates. It fits the width it is given and puts
 * the rest behind "more", most important kept visible (maiz::action_bar), so a
 * phone upright, a phone sideways and a half-width duo window all get every
 * action, none of them clipped off the edge (the 0.2.0 APK's bug). */
void CombinatorsApp::draw_actions(float width) {
    bool can_step = active_count > 0;
    enum { Step, Reduce, Undo, Redo, Add, Delete, Clean, Relax };
    std::vector<maiz::BarAction> acts = {
        {"step", 0, can_step, false, true},
        {auto_reduce ? "stop" : "reduce all", 3, can_step || auto_reduce},
        {"undo", 1, undo_depth > 0},
        {"redo", 2, true},
        {"add", 4, true},
        {"delete", 5, !ed.selection.empty()},
        {"clean", 6, true},
        {"relax", 7, true},
    };
    switch (maiz::action_bar("actions", acts, touch_mode, width)) {
    case Step: try_step(); break;
    case Reduce: auto_reduce = !auto_reduce; break;
    case Undo: dispatch_and_reproject("undo"); break;
    case Redo: dispatch_and_reproject("redo"); break;
    case Add: ImGui::OpenPopup("##add-agent"); break;
    case Delete:
        dispatch_and_reproject(maiz::compile_deletes(ed.selection));
        ed.selection.clear();
        break;
    case Clean:
        if (std::string cmd = maiz::compile_clean(scene, 96.0f, 96.0f); !cmd.empty())
            dispatch_and_reproject(cmd);
        break;
    case Relax:
        if (std::string cmd = maiz::compile_relax(scene); !cmd.empty()) dispatch_and_reproject(cmd);
        break;
    default: break;
    }
    // add: mint at the centre of the visible canvas (a phone has no Shift+A)
    if (ImGui::BeginPopup("##add-agent")) {
        for (const auto& entry : palette.entries)
            if (ImGui::MenuItem(entry.label.c_str())) {
                float wx = ed.cam.x + canvas_w_px * 0.5f / ed.cam.zoom;
                float wy = ed.cam.y + canvas_h_px * 0.5f / ed.cam.zoom;
                dispatch_and_reproject(maiz::compile_add(
                    entry.glyph, maiz::unique_name(scene, entry.glyph), wx, wy));
            }
        ImGui::EndPopup();
    }
}

// ── one frame ────────────────────────────────────────────────────────────────

void CombinatorsApp::frame() {
    ImGuiIO& io = ImGui::GetIO();
#ifdef IC_NET
    surfaces.begin_frame(); // the canvas declares into it; net_frame reads it
#endif

    // reduce-all chains one step per finished replay (or per frame when
    // animations are off) — stoppable, and each step stays one undo frame
    if (auto_reduce && !anim.active) {
        if (!try_step()) auto_reduce = false;
    }

    // ── live physics: stage like a drag, flush ONE batch on settle ───────────
    if (physics_on && !anim.active && ed.drag == maiz::EditorState::Drag::None) {
        float moved = maiz::relax_step(scene, phys);
        for (auto& n : scene.nodes) {
            auto it = phys.find(n.name);
            if (it != phys.end()) {
                n.x = it->second.first;
                n.y = it->second.second;
            }
        }
        if (moved < 0.15f) {
            if (++settle_frames > 30 && !physics_pending().empty()) flush_physics();
        } else {
            settle_frames = 0;
        }
    }

    // ── the menu ribbon: every model-touching entry is a command ────────────
    if (!touch_mode && ImGui::BeginMainMenuBar()) { // a phone keeps these behind ⋮
        draw_menus();
        ImGui::EndMainMenuBar();
    }
    if (io.KeyCtrl && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_S, false))
        do_save();

    // ── the workspace: three resizable panes around two splitters ───────────
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::Begin("Workspace", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    ImVec2 area = ImGui::GetContentRegionAvail();
    const float th = touch_mode ? 14.0f : 6.0f; // splitters need finger width
    /* Three arrangements, decided in dp (maiz::classify_layout), never by the
     * device name:
     *   desktop           canvas | inspector, log + command bar below (splitters)
     *   phone, upright    app bar, canvas, action bar; the inspector in a sheet
     *   phone, sideways   app bar, canvas + action bar | inspector at the side
     * The phone has no log pane: the transcript still records, errors toast. */
    const maiz::LayoutClass lc =
        maiz::classify_layout(vp->WorkSize.x, vp->WorkSize.y, ui_scale, touch_mode);
    const bool side_panel = touch_mode && lc.orientation == maiz::Orientation::Landscape;
    const float bar_h = ImGui::GetFrameHeightWithSpacing();
    float top_h, canvas_w;
    if (touch_mode) {
        // the app bar: who (in a session) or what, the pair count, and ⋮
        ImGui::AlignTextToFramePadding();
        if (role != Role::Solo) {
            draw_identity();
        } else {
            ImGui::TextUnformatted("Combinators");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%d pairs", active_count);
        if (updater && updater->stage() >= maiz::update::Updater::Stage::Offered &&
            updater->stage() <= maiz::update::Updater::Stage::Ready) {
            ImGui::SameLine();
            maiz::update_badge(*updater, update_view, true);
        }
        float dw = maiz::dots_button_width(true);
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - dw));
        if (maiz::begin_overflow_menu("app-menu", true)) {
            draw_menus();
            maiz::end_overflow_menu();
        }
        area = ImGui::GetContentRegionAvail();
        // upright, the sheet's peeking header takes the bottom of the screen
        float peek = side_panel ? 0.0f : vp->WorkSize.y * sheet.detents[0];
        canvas_w = side_panel ? std::floor(area.x * 0.62f) : area.x;
        top_h = std::max(120.0f, area.y - bar_h - peek);
    } else {
        top_h = std::max(64.0f, (area.y - th) * log_frac);
        canvas_w = std::max(120.0f, (area.x - th) * canvas_frac);
    }

    if (side_panel) ImGui::BeginGroup(); // canvas over action bar, beside the inspector
    ImGui::BeginChild("canvas-pane", ImVec2(canvas_w, top_h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);
    if (!touch_mode) {
        // the status readout is dev chrome; the actions fit the pane's width
        ImGui::Text("mantle: %s   agents: %d   pairs: %d   undo: %d", scene.mantle.c_str(),
                    (int)scene.nodes.size(), active_count, undo_depth);
        if (role != Role::Solo) {
            ImGui::SameLine(0, 20);
            draw_identity();
        }
        if (updater && updater->stage() >= maiz::update::Updater::Stage::Offered &&
            updater->stage() <= maiz::update::Updater::Stage::Ready) {
            ImGui::SameLine(0, 20);
            maiz::update_badge(*updater, update_view, false);
        }
        draw_actions(0.0f);
    }

    // errors must still reach the eye with the log hidden: a transient toast
    while (seen_log < log.size()) {
        if (log[seen_log].level == "error") {
            toast = log[seen_log].op + ": " + log[seen_log].msg;
            toast_until = ImGui::GetTime() + 6.0;
        }
        ++seen_log;
    }
    if (touch_mode && ImGui::GetTime() < toast_until) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.28f, 0.22f, 1.0f));
        ImGui::TextUnformatted(toast.c_str());
        ImGui::PopStyleColor();
    }

    auto ctx_menu = [&](const maiz::SceneNode* node, const maiz::SceneWire* wire,
                        maiz::CanvasIO&) {
        if (wire && wire->kind == maiz::SceneWire::Kind::Fettuccine && wire->active) {
            if (ImGui::MenuItem("interact — fire this pair"))
                pending_fire = {true, wire->from, wire->to};
            return;
        }
        if (!node || !hot_agents.count(node->name)) return;
        if (ImGui::MenuItem("interact — fire this pair"))
            pending_fire = {true, node->name, ""};
    };

    // this frame's animation overrides (view ephemera, rebuilt every frame)
    maiz::CanvasFx fx;
    bool use_fx = false;
    if (anim.active) {
        anim.t += io.DeltaTime * anim_speed / 0.9f; // 0.9s at speed 1
        if (anim.t >= 1.0f) {
            anim.active = false;
        } else {
            use_fx = true;
            float t = anim.t;
            float glide = ease(std::min(t / 0.45f, 1.0f));
            float gscale = 1.0f - 0.38f * glide; // 1 → 0.62 while gliding
            if (t > 0.45f) gscale = 0.62f * std::max(0.0f, 1.0f - (t - 0.45f) / 0.15f);
            if (gscale > 0.02f) {
                maiz::GhostFx ga{anim.ghost_a, anim.ax + (anim.mx - anim.ax) * glide,
                                 anim.ay + (anim.my - anim.ay) * glide, gscale};
                maiz::GhostFx gb{anim.ghost_b, anim.bx + (anim.mx - anim.bx) * glide,
                                 anim.by + (anim.my - anim.by) * glide, gscale};
                fx.ghosts.push_back(ga);
                fx.ghosts.push_back(gb);
            }
            for (const auto& m : anim.minted) {
                if (t < 0.42f) {
                    fx.nodes[m.name] = {anim.mx, anim.my, 0.0f}; // not yet
                } else {
                    float q = ease((t - 0.42f) / 0.58f);
                    fx.nodes[m.name] = {anim.mx + (m.tx - anim.mx) * q,
                                        anim.my + (m.ty - anim.my) * q, 0.2f + 0.8f * q};
                }
            }
        }
    }

    // remote changes that are not steps: glide, shrink out, grow in (250 ms)
    if (tween.active) {
        tween.t += io.DeltaTime / 0.25f;
        if (tween.t >= 1.0f) {
            tween.active = false;
        } else {
            use_fx = true;
            float q = ease(tween.t);
            for (const auto& m : tween.moves)
                if (!fx.nodes.count(m.name))
                    fx.nodes[m.name] = {m.fx + (m.tx - m.fx) * q, m.fy + (m.ty - m.fy) * q, 1.0f};
            for (const auto& g : tween.gone) {
                float cx, cy;
                node_center(g, cx, cy);
                fx.ghosts.push_back({g, cx, cy, 1.0f - q});
            }
            for (const auto& name : tween.born)
                if (const maiz::SceneNode* n = scene.find(name); n && !fx.nodes.count(name)) {
                    float cx, cy;
                    node_center(*n, cx, cy);
                    fx.nodes[name] = {cx, cy, 0.2f + 0.8f * q};
                }
        }
    }

    // remember the pane rect: the touch shell anchors pinch zoom to it
    {
        ImVec2 org = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::GetContentRegionAvail();
        canvas_org_x = org.x;
        canvas_org_y = org.y;
        canvas_w_px = sz.x;
        canvas_h_px = sz.y;
    }
    if (touch_mode) {
        int orientation = canvas_w_px >= canvas_h_px ? 1 : 0;
        if (orientation != fitted_orientation) {
            fit_camera();
            fitted_orientation = orientation;
        }
    }
    const maiz::CanvasNet* cnet = nullptr;
#ifdef IC_NET
    maiz::CanvasNet canvas_net;
    if (net) {
        canvas_net.surfaces = &surfaces;
        canvas_net.roster = &net->roster();
        canvas_net.display = net->settings().show;
        cnet = &canvas_net;
    }
#endif
    maiz::CanvasIO cio = maiz::edit_canvas("net-canvas", scene, ed, canvas_style,
                                           &palette, nullptr, ctx_menu,
                                           use_fx ? &fx : nullptr, cnet);
    for (const auto& cmd : cio.commands) dispatch_and_reproject(cmd);

    // Space fires the hovered active pair — or a random one. (Reassigned from
    // the pan chord 2026-07-14; Alt+drag still pans. Ignored while typing.)
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        if (ed.hover_wire_valid &&
            ed.hover_wire.kind == maiz::SceneWire::Kind::Fettuccine &&
            ed.hover_wire.active) {
            try_step(ed.hover_wire.from, ed.hover_wire.to);
        } else if (!hot_pairs.empty()) {
            const auto& p = hot_pairs[rng() % hot_pairs.size()];
            try_step(p.first, p.second);
        }
    }
    // Touch: DOUBLE-TAP is the fire gesture. A thumb is not a cursor, so the
    // target isn't hit-tested — the NEAREST active pair within a generous
    // reach (either agent's body or the wire midpoint) fires; beyond that, a
    // random one. Anywhere on the canvas reduces something.
    if (touch_mode && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ImVec2 mp = io.MousePos;
        bool in_canvas = mp.x >= canvas_org_x && mp.x <= canvas_org_x + canvas_w_px &&
                         mp.y >= canvas_org_y && mp.y <= canvas_org_y + canvas_h_px;
        if (in_canvas && active_count > 0) {
            // the second tap's press may have armed a drag — the fire wins
            ed.drag = maiz::EditorState::Drag::None;
            ed.staged.clear();
            auto to_screen = [&](float wx, float wy) {
                return ImVec2(canvas_org_x + (wx - ed.cam.x) * ed.cam.zoom,
                              canvas_org_y + (wy - ed.cam.y) * ed.cam.zoom);
            };
            auto d2_to = [&](ImVec2 s) {
                float dx = mp.x - s.x, dy = mp.y - s.y;
                return dx * dx + dy * dy;
            };
            const std::pair<std::string, std::string>* best = nullptr;
            float reach = 110.0f * std::max(1.0f, io.FontGlobalScale); // thumb radius
            float best_d2 = reach * reach;
            for (const auto& p : hot_pairs) {
                const maiz::SceneNode* na = scene.find(p.first);
                const maiz::SceneNode* nb = scene.find(p.second);
                if (!na || !nb) continue;
                float acx, acy, bcx, bcy;
                node_center(*na, acx, acy);
                node_center(*nb, bcx, bcy);
                ImVec2 sa = to_screen(acx, acy), sb = to_screen(bcx, bcy);
                ImVec2 mid((sa.x + sb.x) * 0.5f, (sa.y + sb.y) * 0.5f);
                float d2 = std::min({d2_to(sa), d2_to(sb), d2_to(mid)});
                if (d2 < best_d2) {
                    best_d2 = d2;
                    best = &p;
                }
            }
            if (best) {
                try_step(best->first, best->second);
            } else {
                const auto& p = hot_pairs[rng() % hot_pairs.size()];
                try_step(p.first, p.second);
            }
        }
    }
    if (pending_fire.valid) {
        try_step(pending_fire.a, pending_fire.b);
        pending_fire.valid = false;
    }
    ImGui::EndChild();

    bool layout_changed = false;
    if (touch_mode) {
        draw_actions(canvas_w); // the thumb's row, under the canvas
        if (side_panel) {
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginChild("inspector-pane", ImVec2(0, 0), ImGuiChildFlags_Borders);
            maiz::CanvasIO iio = maiz::draw_inspector(scene, ed);
            for (const auto& cmd : iio.commands) dispatch_and_reproject(cmd);
            ImGui::EndChild();
        }
        ImGui::End();
        ImGui::PopStyleVar();
        if (!side_panel) {
            // upright: the inspector lives in a sheet that peeks, drags up, and
            // does not steal the canvas until asked
            if (maiz::begin_bottom_sheet("inspector", sheet)) {
                maiz::CanvasIO iio = maiz::draw_inspector(scene, ed);
                for (const auto& cmd : iio.commands) dispatch_and_reproject(cmd);
            }
            maiz::end_bottom_sheet(sheet);
        }
        draw_modals();
#ifdef IC_NET
        net_frame();
#endif
        return;
    }

    ImGui::SameLine(0, 0);
    auto vs = maiz::splitter("##vsplit", true, canvas_frac, area.x - th, 0.4f, 0.92f, th,
                             top_h);
    ImGui::SameLine(0, 0);

    ImGui::BeginChild("inspector-pane", ImVec2(0, top_h));
    maiz::CanvasIO iio = maiz::draw_inspector(scene, ed);
    for (const auto& cmd : iio.commands) dispatch_and_reproject(cmd);
    ImGui::EndChild();

    layout_changed = vs.released;
    { // the log pane + command bar are desktop chrome
        auto hs =
            maiz::splitter("##hsplit", false, log_frac, area.y - th, 0.3f, 0.92f, th);
        layout_changed = layout_changed || hs.released;

        ImGui::BeginChild("log-pane", ImVec2(0, 0));
        if (ImGui::SmallButton("copy condensed"))
            ImGui::SetClipboardText(maiz::log_to_text(log, true).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("copy all"))
            ImGui::SetClipboardText(maiz::log_to_text(log, false).c_str());
        ImGui::SameLine(0, 16);
        ImGui::TextDisabled("condensed = structural changes only");
        float footer = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("##lines", ImVec2(0, -footer));
        maiz::draw_log_strip(log);
        ImGui::EndChild();
        maiz::CanvasIO bio = maiz::draw_command_bar(cmdbar);
        for (const auto& cmd : bio.commands) {
            maiz::Result r = dispatch_and_reproject(cmd);
            log.push_back(
                {">", cmd, r.text().empty() ? (r.ok ? "ok" : "failed") : r.text()});
        }
        ImGui::EndChild();
    }
    if (layout_changed) flush_panels(); // view state → config tier
    ImGui::End();
    ImGui::PopStyleVar();
#ifdef IC_NET
    net_frame();
#endif
    draw_modals();
}

// ── modals: Save As / Open / Settings (both substrates) ─────────────────────

void CombinatorsApp::draw_modals() {
    if (updater &&
        maiz::draw_update_modals(*updater, update_view, touch_mode) == maiz::UpdateChoice::Apply) {
        maiz::update::ApplyResult r =
            maiz::update::apply(updater->downloaded(), updater->self(), android_activity);
        update_view.message = r.message;
        log.push_back({r.ok ? "info" : "error", "update", r.message});
        if (r.quit_now && on_quit) on_quit(); // the new version is starting beside this one
    }
    if (want_save_as) {
        ImGui::OpenPopup("Save project as");
        if (!current_path.empty())
            std::snprintf(save_name, sizeof save_name, "%s",
                          current_path.stem().string().c_str());
        else if (!save_name[0]) // keyboard-free default (mobile has no IME yet)
            std::snprintf(save_name, sizeof save_name, "%s", auto_name().c_str());
        want_save_as = false;
    }
    if (ImGui::BeginPopupModal("Save project as", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsWindowAppearing() && !touch_mode) ImGui::SetKeyboardFocusHere();
        bool entered =
            ImGui::InputTextWithHint("##name", "project name…", save_name,
                                     sizeof save_name, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::TextDisabled(
            "→ %s",
            (projects_dir() / (std::string(save_name) + ".json")).string().c_str());
        bool ok = ImGui::Button("save");
        ImGui::SameLine();
        bool cancel = ImGui::Button("cancel");
        if ((entered || ok) && save_name[0]) {
            current_path = projects_dir() / (std::string(save_name) + ".json");
            do_save();
            set_title_now();
            ImGui::CloseCurrentPopup();
        }
        if (cancel) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (want_open) {
        ImGui::OpenPopup("Open project");
        want_open = false;
    }
    if (ImGui::BeginPopupModal("Open project", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("%s", projects_dir().string().c_str());
        bool any = false;
        for (const auto& entry : fs::directory_iterator(projects_dir())) {
            if (entry.path().extension() != ".json") continue;
            any = true;
            if (ImGui::Selectable(entry.path().stem().string().c_str())) {
                load_project(entry.path());
                ImGui::CloseCurrentPopup();
                break; // the iterator's world just changed under us
            }
        }
        if (!any) ImGui::TextDisabled("(no saved projects yet)");
        if (ImGui::Button("cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (want_settings) {
        settings_open = true;
        want_settings = false;
    }
    if (settings_open) {
        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
        if (ImGui::Begin("Settings", &settings_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SeparatorText("Rewrite animation");
            if (ImGui::Checkbox("animate interactions", &anim_on)) flush_anim_cfg();
            if (!anim_on) ImGui::BeginDisabled();
            ImGui::SliderFloat("speed", &anim_speed, 0.25f, 3.0f, "%.2fx");
            if (ImGui::IsItemDeactivatedAfterEdit()) flush_anim_cfg();
            if (!anim_on) ImGui::EndDisabled();
            if (updater) {
                ImGui::SeparatorText("Updates");
                maiz::draw_update_settings(*updater, update_view);
            }
            ImGui::SeparatorText("Layout physics");
            if (ImGui::Checkbox("live physics", &physics_on))
                if (!physics_on) flush_physics();
            ImGui::TextDisabled("nodes repel, wires pull; settles as one\n"
                                "undoable batch — same as Edit > Relax layout");
        }
        ImGui::End();
    }
}
