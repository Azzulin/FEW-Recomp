// The build window: shows the steps and progress of a Fire Emblem Warriors build while gerar-few.ps1 (Windows) or
// build-linux.sh (Linux, Steam Deck) runs in its terminal. It only reads the events file the build writes
// (tools/build_events.py documents it), so closing it never stops the build; at the end it offers to start the game.
//
//     few-build-ui --events <file> [--pid <build process>]
//
// SDL3 + Dear ImGui on SDL's renderer. Usable with a mouse, a keyboard or a controller. The look is our own (deep
// blue, gold rules, diamond markers, a crest drawn here): no logo, art or text of the game is used.
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

#include "font_data.h"  // Roboto Medium (Apache-2.0), from third_party/imgui/misc/fonts at configure time

namespace {

// ---- Colours ----------------------------------------------------------------------------------------------------

constexpr ImU32 rgba(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }
const ImU32 kSkyTop = rgba(9, 18, 42), kSkyBottom = rgba(21, 44, 92);
const ImU32 kPanel = rgba(12, 24, 54, 225), kPanelEdge = rgba(201, 166, 70, 150);
const ImU32 kGold = rgba(222, 186, 92), kGoldDeep = rgba(168, 124, 40), kGoldLight = rgba(246, 222, 146);
const ImU32 kIvory = rgba(242, 236, 221), kMuted = rgba(150, 165, 196), kFaint = rgba(84, 101, 140);
const ImU32 kCrimson = rgba(214, 74, 74), kTrack = rgba(6, 12, 30, 235);
ImVec4 vec(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// ---- Minimal JSON: the events file only --------------------------------------------------------------------------

struct Json {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;
    const Json& operator[](const char* key) const {
        static const Json none;
        for (auto& [k, v] : o)
            if (k == key) return v;
        return none;
    }
    std::string str() const { return kind == String ? s : std::string(); }
    double num(double fallback = 0) const { return kind == Number ? n : fallback; }
};

struct Parser {
    const char* p;
    const char* end;
    void ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p; }
    bool lit(const char* word) {
        size_t n = strlen(word);
        if (size_t(end - p) < n || memcmp(p, word, n)) return false;
        p += n;
        return true;
    }
    static void put_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += char(cp);
        else if (cp < 0x800) out += char(0xC0 | cp >> 6), out += char(0x80 | (cp & 0x3F));
        else if (cp < 0x10000) out += char(0xE0 | cp >> 12), out += char(0x80 | (cp >> 6 & 0x3F)), out += char(0x80 | (cp & 0x3F));
        else out += char(0xF0 | cp >> 18), out += char(0x80 | (cp >> 12 & 0x3F)), out += char(0x80 | (cp >> 6 & 0x3F)),
             out += char(0x80 | (cp & 0x3F));
    }
    bool hex4(unsigned& v) {
        if (end - p < 4) return false;
        v = 0;
        for (int i = 0; i < 4; i++) {
            char c = *p++;
            v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99);
            if (v > 0xFFFF) return false;
        }
        return true;
    }
    bool string(std::string& out) {
        if (p >= end || *p != '"') return false;
        ++p;
        while (p < end && *p != '"') {
            if (*p != '\\') { out += *p++; continue; }
            if (++p >= end) return false;
            char c = *p++;
            switch (c) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                unsigned cp;
                if (!hex4(cp)) return false;
                if (cp >= 0xD800 && cp < 0xDC00 && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                    p += 2;
                    unsigned low;
                    if (!hex4(low)) return false;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                put_utf8(out, cp);
                break;
            }
            default: out += c;  // \" \\ \/
            }
        }
        if (p >= end) return false;
        ++p;
        return true;
    }
    bool parse(Json& out) {
        ws();
        if (p >= end) return false;
        if (*p == '{') {
            out.kind = Json::Object;
            ++p;
            ws();
            if (p < end && *p == '}') { ++p; return true; }
            while (true) {
                std::string key;
                ws();
                if (!string(key)) return false;
                ws();
                if (p >= end || *p++ != ':') return false;
                Json value;
                if (!parse(value)) return false;
                out.o.emplace_back(std::move(key), std::move(value));
                ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == '}') { ++p; return true; }
                return false;
            }
        }
        if (*p == '[') {
            out.kind = Json::Array;
            ++p;
            ws();
            if (p < end && *p == ']') { ++p; return true; }
            while (true) {
                Json value;
                if (!parse(value)) return false;
                out.a.push_back(std::move(value));
                ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == ']') { ++p; return true; }
                return false;
            }
        }
        if (*p == '"') { out.kind = Json::String; return string(out.s); }
        if (lit("true")) { out.kind = Json::Bool; out.b = true; return true; }
        if (lit("false")) { out.kind = Json::Bool; return true; }
        if (lit("null")) return true;
        char* stop = nullptr;
        std::string number(p, std::min<size_t>(size_t(end - p), 64));
        out.n = strtod(number.c_str(), &stop);
        if (stop == number.c_str()) return false;
        out.kind = Json::Number;
        p += stop - number.c_str();
        return true;
    }
};

// ---- What the build has said so far --------------------------------------------------------------------------------

enum class Outcome { Running, Done, Failed, Gone };

struct Build {
    struct Step { std::string id, label; };
    std::vector<Step> steps;
    std::string log_path, play, folder, error;
    int current = -1;
    Outcome outcome = Outcome::Running;
    bool started = false;
    double start_t = 0, last_t = 0;
    double fraction = -1;  // of the current step; < 0: not known
    std::string detail;
    int done = 0, total = 0;
    double eta = -1;
    std::deque<std::string> recent;  // files that just finished compiling

    int index_of(const std::string& id) const {
        for (size_t i = 0; i < steps.size(); i++)
            if (steps[i].id == id) return int(i);
        return -1;
    }
    const char* current_label() const {
        return current >= 0 && current < int(steps.size()) ? steps[current].label.c_str() : "Starting";
    }

    void apply(const Json& e) {
        std::string type = e["type"].str();
        double t = e["t"].num(last_t);
        last_t = std::max(last_t, t);
        if (type == "start") {
            *this = Build{};
            started = true;
            start_t = last_t = t;
            log_path = e["log"].str();
            for (auto& s : e["steps"].a) steps.push_back({s["id"].str(), s["label"].str()});
        } else if (type == "step") {
            int i = index_of(e["id"].str());
            if (i >= 0 && i != current) {
                current = i;
                fraction = -1, eta = -1, detail.clear(), done = total = 0;
                recent.clear();
            }
        } else if (type == "progress") {
            int i = index_of(e["id"].str());
            if (i >= 0 && i != current) apply_step(i);
            fraction = std::clamp(e["fraction"].num(-1), -1.0, 1.0);
            detail = e["text"].str();
            done = int(e["done"].num(done)), total = int(e["total"].num(total));
            eta = e["eta"].num(-1);
            for (auto& f : e["files"].a) {
                recent.push_front(f.str());
                if (recent.size() > 6) recent.pop_back();
            }
        } else if (type == "done") {
            outcome = Outcome::Done;
            play = e["play"].str();
            folder = e["folder"].str();
            current = int(steps.size());
        } else if (type == "error") {
            outcome = Outcome::Failed;
            error = e["text"].str();
        }
    }
    void apply_step(int i) {
        current = i;
        fraction = -1, eta = -1, detail.clear(), done = total = 0;
        recent.clear();
    }
};

// Reads the events file as it grows. A file that got shorter means a new build started: read it again from the top.
struct EventsFile {
    std::string path;
    Sint64 offset = 0;
    std::string partial;
    bool seen = false;

    void poll(Build& build) {
        SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
        if (!io) return;
        seen = true;
        Sint64 size = SDL_GetIOSize(io);
        if (size < offset) { offset = 0, partial.clear(); build = Build{}; }
        if (size > offset && SDL_SeekIO(io, offset, SDL_IO_SEEK_SET) == offset) {
            std::string chunk(size_t(size - offset), '\0');
            size_t got = SDL_ReadIO(io, chunk.data(), chunk.size());
            chunk.resize(got);
            offset += Sint64(got);
            partial += chunk;
            size_t start = 0, nl;
            while ((nl = partial.find('\n', start)) != std::string::npos) {
                std::string line = partial.substr(start, nl - start);
                start = nl + 1;
                if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB) line.erase(0, 3);
                Json event;
                Parser parser{line.data(), line.data() + line.size()};
                if (parser.parse(event) && event.kind == Json::Object) build.apply(event);
            }
            partial.erase(0, start);
        }
        SDL_CloseIO(io);
    }
};

// ---- The build process -------------------------------------------------------------------------------------------

bool process_alive(long pid) {
    if (pid <= 0) return true;
#ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED;
    bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
#else
    return kill(pid_t(pid), 0) == 0 || errno == EPERM;
#endif
}

#ifdef _WIN32
std::wstring wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
#endif

// Starts the game the build made (play), in its folder; false if it could not be started.
bool start_game(const std::string& play, const std::string& folder) {
    if (play.empty()) return false;
#ifdef _WIN32
    auto r = (INT_PTR)ShellExecuteW(nullptr, L"open", wide(play).c_str(), nullptr,
                                    folder.empty() ? nullptr : wide(folder).c_str(), SW_SHOWNORMAL);
    return r > 32;
#else
    pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        setsid();
        if (!folder.empty() && chdir(folder.c_str()) != 0) _exit(127);
        execl(play.c_str(), play.c_str(), (char*)nullptr);
        _exit(127);
    }
    return true;
#endif
}

void open_folder(const std::string& folder) {
    if (folder.empty()) return;
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", wide(folder).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    std::string url = "file://";
    for (unsigned char c : folder) {
        if (isalnum(c) || strchr("/-_.~", c)) url += char(c);
        else { char hex[4]; snprintf(hex, sizeof hex, "%%%02X", c); url += hex; }
    }
    SDL_OpenURL(url.c_str());
#endif
}

// ---- Drawing -----------------------------------------------------------------------------------------------------

double now_seconds() {
    SDL_Time ns = 0;
    SDL_GetCurrentTime(&ns);
    return double(ns) / 1e9;
}

std::string duration(double seconds) {
    long s = long(seconds + 0.5);
    char buf[64];
    if (s < 60) snprintf(buf, sizeof buf, "%ld s", s);
    else if (s < 3600) snprintf(buf, sizeof buf, "%ld min", s / 60);
    else snprintf(buf, sizeof buf, "%ld h %02ld min", s / 3600, s / 60 % 60);
    return buf;
}

std::string time_left(double eta) {
    if (eta < 0) return "estimating...";
    if (eta < 60) return "under a minute";
    return "about " + duration(eta);
}

// A shield with an upright sword and a ring: the window's own emblem.
void draw_crest(ImDrawList* dl, ImVec2 c, float size, float alpha) {
    auto col = [&](ImU32 base) {
        ImVec4 v = vec(base);
        v.w *= alpha;
        return ImGui::ColorConvertFloat4ToU32(v);
    };
    float w = size * 0.5f, h = size * 0.62f;
    ImVec2 tl(c.x - w, c.y - h * 0.86f), tr(c.x + w, c.y - h * 0.86f), bottom(c.x, c.y + h);
    auto shield = [&](float inset) {
        dl->PathClear();
        dl->PathLineTo(ImVec2(tl.x + inset, tl.y + inset));
        dl->PathLineTo(ImVec2(c.x, tl.y + inset - size * 0.07f));
        dl->PathLineTo(ImVec2(tr.x - inset, tr.y + inset));
        dl->PathLineTo(ImVec2(tr.x - inset, c.y - h * 0.05f));
        dl->PathBezierCubicCurveTo(ImVec2(tr.x - inset, c.y + h * 0.45f), ImVec2(c.x + w * 0.35f, c.y + h * 0.8f),
                                   ImVec2(bottom.x, bottom.y - inset));
        dl->PathBezierCubicCurveTo(ImVec2(c.x - w * 0.35f, c.y + h * 0.8f), ImVec2(tl.x + inset, c.y + h * 0.45f),
                                   ImVec2(tl.x + inset, c.y - h * 0.05f));
    };
    shield(0);
    dl->PathFillConcave(col(rgba(16, 34, 78)));
    shield(0);
    dl->PathStroke(col(kGold), size * 0.045f, ImDrawFlags_Closed);
    shield(size * 0.09f);
    dl->PathStroke(col(kGoldDeep), size * 0.018f, ImDrawFlags_Closed);
    // Ring behind the crossguard
    dl->AddCircle(ImVec2(c.x, c.y - h * 0.12f), size * 0.2f, col(kGoldDeep), 40, size * 0.03f);
    // Sword: blade with a point, crossguard, grip, pommel
    float bw = size * 0.05f;
    ImVec2 tip(c.x, c.y - h * 0.78f);
    dl->AddTriangleFilled(tip, ImVec2(c.x - bw, tip.y + size * 0.1f), ImVec2(c.x + bw, tip.y + size * 0.1f), col(kGoldLight));
    dl->AddRectFilled(ImVec2(c.x - bw, tip.y + size * 0.1f), ImVec2(c.x + bw, c.y + h * 0.18f), col(kGoldLight));
    dl->AddLine(ImVec2(c.x, tip.y + size * 0.12f), ImVec2(c.x, c.y + h * 0.16f), col(kGoldDeep), size * 0.012f);
    float gy = c.y + h * 0.18f;
    dl->AddRectFilled(ImVec2(c.x - size * 0.22f, gy), ImVec2(c.x + size * 0.22f, gy + size * 0.055f), col(kGold),
                      size * 0.02f);
    dl->AddRectFilled(ImVec2(c.x - bw * 0.8f, gy + size * 0.055f), ImVec2(c.x + bw * 0.8f, gy + size * 0.2f), col(kGoldDeep));
    dl->AddCircleFilled(ImVec2(c.x, gy + size * 0.24f), size * 0.045f, col(kGold));
}

void diamond(ImDrawList* dl, ImVec2 c, float r, ImU32 color, bool filled, float thickness = 2.0f) {
    ImVec2 p[4] = {ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y)};
    if (filled) dl->AddConvexPolyFilled(p, 4, color);
    else dl->AddPolyline(p, 4, color, thickness, ImDrawFlags_Closed);
}

// A gold rule with a diamond in the middle.
void ornament(ImDrawList* dl, float x0, float x1, float y, float scale) {
    float mid = (x0 + x1) * 0.5f, gap = 14 * scale;
    dl->AddLine(ImVec2(x0, y), ImVec2(mid - gap, y), kPanelEdge, 1.5f * scale);
    dl->AddLine(ImVec2(mid + gap, y), ImVec2(x1, y), kPanelEdge, 1.5f * scale);
    diamond(dl, ImVec2(mid, y), 6 * scale, kGold, true);
    diamond(dl, ImVec2(mid - gap * 0.6f, y), 2.5f * scale, kGold, true);
    diamond(dl, ImVec2(mid + gap * 0.6f, y), 2.5f * scale, kGold, true);
}

void panel(ImDrawList* dl, ImVec2 a, ImVec2 b, float scale, ImU32 edge = kPanelEdge) {
    dl->AddRectFilled(a, b, kPanel, 6 * scale);
    dl->AddRect(a, b, edge, 6 * scale, 1.5f * scale);
    float k = 10 * scale;  // gold corner ticks
    for (ImVec2 corner : {a, ImVec2(b.x, a.y), ImVec2(a.x, b.y), b}) {
        float sx = corner.x == a.x ? 1.f : -1.f, sy = corner.y == a.y ? 1.f : -1.f;
        dl->AddLine(ImVec2(corner.x + sx * 3 * scale, corner.y + sy * 3 * scale),
                    ImVec2(corner.x + sx * k, corner.y + sy * 3 * scale), kGold, 2 * scale);
        dl->AddLine(ImVec2(corner.x + sx * 3 * scale, corner.y + sy * 3 * scale),
                    ImVec2(corner.x + sx * 3 * scale, corner.y + sy * k), kGold, 2 * scale);
    }
}

// The progress bar: a gold fill with a light that runs along it; with no fraction known, a gold segment that sweeps.
void progress_bar(ImDrawList* dl, ImVec2 a, ImVec2 b, double fraction, double t, float scale) {
    float r = 4 * scale;
    dl->AddRectFilled(a, b, kTrack, r);
    dl->AddRect(a, b, kGoldDeep, r, 1.2f * scale);
    float inner = 3 * scale;
    ImVec2 ia(a.x + inner, a.y + inner), ib(b.x - inner, b.y - inner);
    float width = ib.x - ia.x;
    if (fraction < 0) {
        float seg = width * 0.22f, pos = float(fmod(t * 0.45, 1.0)) * (width + seg) - seg;
        float x0 = std::max(ia.x, ia.x + pos), x1 = std::min(ib.x, ia.x + pos + seg);
        if (x1 > x0) dl->AddRectFilledMultiColor(ImVec2(x0, ia.y), ImVec2(x1, ib.y), kGoldDeep, kGold, kGold, kGoldDeep);
        return;
    }
    float fx = ia.x + width * float(std::clamp(fraction, 0.0, 1.0));
    if (fx > ia.x + 1) {
        dl->AddRectFilledMultiColor(ia, ImVec2(fx, ib.y), kGoldDeep, kGold, kGold, kGoldDeep);
        dl->AddLine(ImVec2(ia.x, ia.y + 1), ImVec2(fx, ia.y + 1), kGoldLight, 1.5f * scale);
        float shine = 60 * scale, sx = ia.x + float(fmod(t * 0.35, 1.0)) * (fx - ia.x + shine) - shine;
        float s0 = std::max(ia.x, sx), s1 = std::min(fx, sx + shine);
        if (s1 > s0) {
            float m = std::clamp(sx + shine * 0.5f, s0, s1);
            dl->AddRectFilledMultiColor(ImVec2(s0, ia.y), ImVec2(m, ib.y), rgba(255, 255, 255, 0), rgba(255, 248, 220, 90),
                                        rgba(255, 248, 220, 90), rgba(255, 255, 255, 0));
            dl->AddRectFilledMultiColor(ImVec2(m, ia.y), ImVec2(s1, ib.y), rgba(255, 248, 220, 90), rgba(255, 255, 255, 0),
                                        rgba(255, 255, 255, 0), rgba(255, 248, 220, 90));
        }
    }
}

void text_at(ImDrawList* dl, ImVec2 p, float size, ImU32 color, const char* text) {
    dl->AddText(nullptr, size, p, color, text);
}

ImVec2 text_size(float size, const char* text) {  // size in pixels, as text_at takes it
    return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
}

// A large gold button that a controller can reach; returns true when pressed.
bool gold_button(const char* label, ImVec2 size, bool primary) {
    ImGui::PushStyleColor(ImGuiCol_Button, primary ? vec(kGoldDeep) : vec(rgba(24, 44, 92)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, primary ? vec(kGold) : vec(rgba(36, 62, 122)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, primary ? vec(kGoldLight) : vec(rgba(48, 80, 150)));
    ImGui::PushStyleColor(ImGuiCol_Text, primary ? vec(rgba(14, 20, 38)) : vec(kIvory));
    ImGui::PushStyleColor(ImGuiCol_Border, vec(kGold));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
    return pressed;
}

struct App {
    Build build;
    EventsFile events;
    long pid = 0;
    double next_pid_check = 0;
    bool quit = false, focus_play = true;
    std::string message;  // a note under the buttons (e.g. the game could not be started)
};

void draw(App& app, float scale) {
    Build& b = app.build;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    ImVec2 o = vp->WorkPos, sz = vp->WorkSize;
    double t = ImGui::GetTime();
    bg->AddRectFilledMultiColor(o, ImVec2(o.x + sz.x, o.y + sz.y), kSkyTop, kSkyTop, kSkyBottom, kSkyBottom);
    draw_crest(bg, ImVec2(o.x + sz.x * 0.86f, o.y + sz.y * 0.66f), sz.y * 0.62f, 0.07f);  // watermark

    ImGui::SetNextWindowPos(o);
    ImGui::SetNextWindowSize(sz);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("build", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // fb: the base font size for ImGui's own text (it applies the scale itself); fs: pixels, for the draw list.
    float pad = 28 * scale, fb = ImGui::GetStyle().FontSizeBase, fs = fb * scale;

    // Header: crest, title, elapsed time
    float head = 112 * scale;
    draw_crest(dl, ImVec2(o.x + pad + 34 * scale, o.y + head * 0.5f + 2 * scale), 64 * scale, 1.0f);
    float tx = o.x + pad + 84 * scale;
    text_at(dl, ImVec2(tx, o.y + 22 * scale), fs * 1.75f, kGold, "FIRE EMBLEM WARRIORS");
    text_at(dl, ImVec2(tx + 2 * scale, o.y + 22 * scale + fs * 1.95f), fs * 0.95f, kMuted,
            "Building the PC version from your own copy of the game");
    if (b.started) {
        double end = b.outcome == Outcome::Running ? now_seconds() : b.last_t;
        std::string elapsed = "Elapsed  " + duration(std::max(0.0, end - b.start_t));
        ImVec2 es = text_size(fs, elapsed.c_str());
        text_at(dl, ImVec2(o.x + sz.x - pad - es.x, o.y + 30 * scale), fs, kMuted, elapsed.c_str());
    }
    ornament(dl, o.x + pad, o.x + sz.x - pad, o.y + head, scale);

    // Steps (left)
    float top = o.y + head + 22 * scale, bottom = o.y + sz.y - 64 * scale;
    float left_w = std::max(260 * scale, sz.x * 0.32f);
    ImVec2 la(o.x + pad, top), lb(o.x + pad + left_w, bottom);
    panel(dl, la, lb, scale);
    text_at(dl, ImVec2(la.x + 22 * scale, la.y + 16 * scale), fs * 0.85f, kGold, "STEPS");
    float row = std::min(54 * scale, (lb.y - la.y - 60 * scale) / std::max<size_t>(b.steps.size(), 1));
    float y0 = la.y + 58 * scale;
    for (size_t i = 0; i < b.steps.size(); i++) {
        ImVec2 m(la.x + 32 * scale, y0 + row * float(i) + fs * 0.5f);
        bool done = int(i) < b.current || b.outcome == Outcome::Done;
        bool now = int(i) == b.current && b.outcome != Outcome::Done;
        bool failed = now && (b.outcome == Outcome::Failed || b.outcome == Outcome::Gone);
        if (i + 1 < b.steps.size())
            dl->AddLine(ImVec2(m.x, m.y + 9 * scale), ImVec2(m.x, m.y + row - 9 * scale), done ? kGoldDeep : kFaint, 2 * scale);
        if (failed) {
            diamond(dl, m, 9 * scale, kCrimson, true);
        } else if (done) {
            diamond(dl, m, 9 * scale, kGold, true);
        } else if (now) {
            float pulse = 0.5f + 0.5f * float(sin(t * 4.0));
            diamond(dl, m, (9 + 3 * pulse) * scale, kGoldLight, false, 2 * scale);
            diamond(dl, m, 5 * scale, kGold, true);
        } else {
            diamond(dl, m, 8 * scale, kFaint, false, 1.5f * scale);
        }
        ImU32 color = failed ? kCrimson : now ? kGoldLight : done ? kIvory : kMuted;
        text_at(dl, ImVec2(m.x + 24 * scale, m.y - fs * 0.5f), fs, color, b.steps[i].label.c_str());
    }
    if (!b.started) text_at(dl, ImVec2(la.x + 22 * scale, y0), fs, kMuted, "Waiting for the build to start...");

    // Main panel (right)
    ImVec2 ra(lb.x + 22 * scale, top), rb(o.x + sz.x - pad, bottom);
    bool failed = b.outcome == Outcome::Failed || b.outcome == Outcome::Gone;
    panel(dl, ra, rb, scale, failed ? kCrimson : kPanelEdge);
    float cx = ra.x + 30 * scale, cw = rb.x - ra.x - 60 * scale;
    // The panel's content in a child window, so that lines of text and buttons start at its left edge.
    ImGui::SetCursorScreenPos(ImVec2(cx, ra.y + 26 * scale));
    ImGui::BeginChild("main", ImVec2(cw, rb.y - ra.y - 40 * scale), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    dl = ImGui::GetWindowDrawList();
    ImGui::PushTextWrapPos(cw);

    if (b.outcome == Outcome::Done) {
        ImGui::PushFont(nullptr, fb * 1.6f);
        ImGui::TextColored(vec(kGoldLight), "Your game is ready");
        ImGui::PopFont();
        ImGui::TextColored(vec(kMuted), "Built in %s. It can now be started from here or from %s.",
                           duration(std::max(0.0, b.last_t - b.start_t)).c_str(),
#ifdef _WIN32
                           "few.exe in the game folder"
#else
                           "Steam (the build added it to your library)"
#endif
        );
        float by = ImGui::GetCursorScreenPos().y + 26 * scale;
        progress_bar(dl, ImVec2(cx, by), ImVec2(cx + cw, by + 34 * scale), 1.0, t, scale);
        ImGui::SetCursorScreenPos(ImVec2(cx, by + 70 * scale));
        // Three buttons that always fit the panel: Play the widest, then the folder, then Close.
        float gap = 16 * scale, bh = 56 * scale, room = cw - 2 * gap;
        float w_play = std::min(room * 0.36f, 260 * scale), w_open = std::min(room * 0.40f, 300 * scale);
        float w_close = std::min(room - w_play - w_open, 160 * scale);
        if (app.focus_play) { ImGui::SetKeyboardFocusHere(); app.focus_play = false; }
        if (gold_button("Play", ImVec2(w_play, bh), true)) {
            if (start_game(b.play, b.folder)) app.quit = true;
            else app.message = "The game could not be started from here: start it from " + b.play;
        }
        ImGui::SameLine(0, gap);
        if (gold_button("Open the game folder", ImVec2(w_open, bh), false)) open_folder(b.folder);
        ImGui::SameLine(0, gap);
        if (gold_button("Close", ImVec2(w_close, bh), false)) app.quit = true;
        if (!app.message.empty()) ImGui::TextColored(vec(kCrimson), "%s", app.message.c_str());
    } else if (failed) {
        ImGui::PushFont(nullptr, fb * 1.6f);
        ImGui::TextColored(vec(kCrimson), b.outcome == Outcome::Gone ? "The build is no longer running" : "The build stopped");
        ImGui::PopFont();
        ImGui::Spacing();
        if (b.outcome == Outcome::Gone)
            ImGui::TextColored(vec(kIvory), "Its terminal window was closed, or it was stopped, during \"%s\".",
                               b.current_label());
        else
            ImGui::TextColored(vec(kIvory), "%s", b.error.empty() ? "See the terminal window for the reason." : b.error.c_str());
        ImGui::Spacing();
        ImGui::TextColored(vec(kMuted), "Running it again continues where it stopped.");
        if (!b.log_path.empty()) ImGui::TextColored(vec(kMuted), "Everything it printed is in %s", b.log_path.c_str());
        else ImGui::TextColored(vec(kMuted), "Everything it printed is in its terminal window.");
        ImGui::Dummy(ImVec2(0, 20 * scale));
        if (app.focus_play) { ImGui::SetKeyboardFocusHere(); app.focus_play = false; }
        if (gold_button("Close", ImVec2(160 * scale, 52 * scale), true)) app.quit = true;
    } else {
        ImGui::PushFont(nullptr, fb * 1.45f);
        ImGui::TextColored(vec(kGoldLight), "%s", b.current_label());
        ImGui::PopFont();
        float by = ImGui::GetCursorScreenPos().y + 18 * scale;
        progress_bar(dl, ImVec2(cx, by), ImVec2(cx + cw, by + 34 * scale), b.fraction, t, scale);
        if (b.fraction >= 0) {
            char pct[16];
            snprintf(pct, sizeof pct, "%d%%", int(b.fraction * 100));
            ImVec2 ps = text_size(fs, pct);
            ImVec2 at(cx + (cw - ps.x) * 0.5f, by + (34 * scale - ps.y) * 0.5f);
            text_at(dl, ImVec2(at.x + 1, at.y + 1), fs, rgba(0, 0, 0, 160), pct);
            text_at(dl, at, fs, kIvory, pct);
        }
        // Figures under the bar
        float fy = by + 52 * scale, col = cw / 3;
        auto figure = [&](int i, const char* name, const std::string& value) {
            text_at(dl, ImVec2(cx + col * i, fy), fs * 0.8f, kMuted, name);
            text_at(dl, ImVec2(cx + col * i, fy + fs * 1.0f), fs * 1.15f, kIvory, value.c_str());
        };
        bool compiling = b.current >= 0 && b.current < int(b.steps.size()) && b.steps[b.current].id == "compile";
        if (compiling && b.total > 0) {
            figure(0, "BUILD STEPS", std::to_string(b.done) + " of " + std::to_string(b.total));
            figure(1, "TIME LEFT", time_left(b.eta));
        } else {
            figure(0, "STATUS", b.detail.empty() ? std::string(b.started ? "Working..." : "Waiting...") : b.detail);
        }
        figure(2, "THIS STEP", b.current >= 0 ? "step " + std::to_string(b.current + 1) + " of " +
                                                    std::to_string(b.steps.size()) : std::string("-"));
        float ly = fy + fs * 3.2f;
        if (compiling && !b.recent.empty()) {
            text_at(dl, ImVec2(cx, ly), fs * 0.8f, kMuted, "JUST COMPILED");
            float yy = ly + fs * 1.2f;
            for (size_t i = 0; i < b.recent.size() && yy < rb.y - fs * 1.5f; i++, yy += fs * 1.25f) {
                diamond(dl, ImVec2(cx + 5 * scale, yy + fs * 0.5f), 3 * scale, i == 0 ? kGold : kGoldDeep, true);
                text_at(dl, ImVec2(cx + 18 * scale, yy), fs * 0.95f, i == 0 ? kIvory : kMuted, b.recent[i].c_str());
            }
        } else if (compiling) {
            text_at(dl, ImVec2(cx, ly), fs * 0.9f, kMuted,
                    "The first build compiles the whole game: from under an hour on a desktop PC to a few hours on a Steam Deck.");
        }
    }
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    dl = ImGui::GetWindowDrawList();

    // Footer
    const char* note = b.outcome == Outcome::Running
        ? "Closing this window does not stop the build. Every detail is in its terminal window."
        : "You can close this window.";
    text_at(dl, ImVec2(o.x + pad, bottom + 22 * scale), fs * 0.85f, kFaint, note);
    ImGui::End();
}

}  // namespace

int main(int argc, char** argv) {
    App app;
    for (int i = 1; i + 1 < argc; i++) {
        if (!strcmp(argv[i], "--events")) app.events.path = argv[++i];
        else if (!strcmp(argv[i], "--pid")) app.pid = strtol(argv[++i], nullptr, 10);
    }
    if (app.events.path.empty()) {
        fprintf(stderr, "usage: few-build-ui --events <file> [--pid <build process>]\n");
        return 2;
    }
#ifndef _WIN32
    unsetenv("LD_PRELOAD");  // Steam's overlay library, when started from a Steam session
#endif
    SDL_SetHint(SDL_HINT_APP_NAME, "Fire Emblem Warriors build");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        fprintf(stderr, "few-build-ui: %s (the build goes on in its terminal)\n", SDL_GetError());
        return 3;
    }
    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    bool game_mode = getenv("GAMESCOPE_WAYLAND_DISPLAY") || (desktop && !SDL_strcasecmp(desktop, "gamescope"));
    // No high pixel density: on KDE with fractional scaling it put the drawing and the pointer in different places
    // (HWDER's setup window). The compositor scales the window; on X11 we grow by the display scale ourselves.
    SDL_Window* window = SDL_CreateWindow("Fire Emblem Warriors - PC build", 1120, 700,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | (game_mode ? SDL_WINDOW_FULLSCREEN : 0));
    float display_scale = window ? std::clamp(SDL_GetWindowDisplayScale(window), 1.0f, 3.0f) : 1.0f;
    SDL_Rect usable{};
    if (window && !game_mode && SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window), &usable)) {
        SDL_SetWindowSize(window, std::min(int(1120 * display_scale), int(usable.w * 0.95f)),
                          std::min(int(700 * display_scale), int(usable.h * 0.92f)));
        SDL_SetWindowMinimumSize(window, 720, 460);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        fprintf(stderr, "few-build-ui: %s (the build goes on in its terminal)\n", SDL_GetError());
        return 3;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)kFontData, int(sizeof kFontData), 20.0f, &font_config);
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 20.0f;
    style.FrameRounding = 4;
    style.FramePadding = ImVec2(16, 10);
    style.ItemSpacing = ImVec2(12, 10);
    style.WindowPadding = ImVec2(0, 0);
    style.Colors[ImGuiCol_NavCursor] = vec(kGoldLight);
    style.ScaleAllSizes(display_scale);
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // FEW_BUILD_UI_SCREENSHOT=<file.bmp>: saves the window after a second, then quits (tests, documentation).
    const char* screenshot = getenv("FEW_BUILD_UI_SCREENSHOT");
    int frames = 0;
    double next_poll = 0;
    while (!app.quit) {
        SDL_Event event;
        bool waited = SDL_WaitEventTimeout(&event, 33);
        while (waited || SDL_PollEvent(&event)) {
            waited = false;
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) app.quit = true;
        }
        double now = SDL_GetTicks() / 1000.0;
        if (now >= next_poll) {
            Outcome before = app.build.outcome;
            app.events.poll(app.build);
            if (app.build.outcome == Outcome::Running && app.build.started && now >= app.next_pid_check) {
                app.next_pid_check = now + 2.0;
                if (!process_alive(app.pid)) {
                    app.events.poll(app.build);  // its last words, written just before it ended
                    if (app.build.outcome == Outcome::Running) app.build.outcome = Outcome::Gone;
                }
            }
            if (app.build.outcome != before && app.build.outcome != Outcome::Running) {
                app.focus_play = true;
                SDL_FlashWindow(window, SDL_FLASH_UNTIL_FOCUSED);
            }
            next_poll = now + 0.25;
        }
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        float fit = std::min(w / (1120.0f * display_scale), h / (700.0f * display_scale));
        float scale = std::clamp(fit, 0.7f, 2.0f) * display_scale;
        style.FontScaleMain = scale;
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        draw(app, scale);
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 9, 18, 42, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        if (screenshot && ++frames == 60) {
            if (SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr)) {
                SDL_SaveBMP(shot, screenshot);
                SDL_DestroySurface(shot);
            }
            app.quit = true;
        }
        SDL_RenderPresent(renderer);
    }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
