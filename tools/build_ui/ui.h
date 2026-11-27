// Shared by the build window (main.cpp) and the Windows setup (setup.cpp): the look (deep blue, gold rules, diamond
// markers, a crest drawn here; no logo, art or text of the game), the events file a build writes
// (tools/build_events.py) and the screen frame both draw into. Header-only, in an anonymous namespace: each source
// file gets its own copy, which keeps the two programs' builds independent.
#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>

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
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

namespace {

// ---- Colours ----------------------------------------------------------------------------------------------------

constexpr ImU32 rgba(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }
const ImU32 kSkyTop = rgba(9, 18, 42), kSkyBottom = rgba(21, 44, 92);
const ImU32 kPanel = rgba(12, 24, 54, 225), kPanelEdge = rgba(201, 166, 70, 150);
const ImU32 kGold = rgba(222, 186, 92), kGoldDeep = rgba(168, 124, 40), kGoldLight = rgba(246, 222, 146);
const ImU32 kIvory = rgba(242, 236, 221), kMuted = rgba(150, 165, 196), kFaint = rgba(84, 101, 140);
const ImU32 kCrimson = rgba(214, 74, 74), kGreen = rgba(126, 201, 140), kTrack = rgba(6, 12, 30, 235);
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

    // The steps a build goes through (tools/build_events.py STEPS), shown before a build has said anything.
    static Build planned() {
        Build b;
        b.steps = {{"check", "Checking your files"}, {"tools", "Getting the build tools"},
                   {"update", "Unpacking update 1.5.0"}, {"translate", "Translating the game's code to C"},
                   {"compile", "Compiling the game"}, {"finish", "Finishing the game folder"}};
        return b;
    }
    int index_of(const std::string& id) const {
        for (size_t i = 0; i < steps.size(); i++)
            if (steps[i].id == id) return int(i);
        return -1;
    }
    const char* current_label() const {
        return current >= 0 && current < int(steps.size()) ? steps[current].label.c_str() : "Starting";
    }
    bool compiling() const { return current >= 0 && current < int(steps.size()) && steps[current].id == "compile"; }
    void apply_step(int i) {
        current = i;
        fraction = -1, eta = -1, detail.clear(), done = total = 0;
        recent.clear();
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
            if (i >= 0 && i != current) apply_step(i);
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
};

// Reads the events file as it grows. A file that got shorter means a new build started: read it again from the top.
struct EventsFile {
    std::string path;
    Sint64 offset = 0;
    std::string partial;

    void reset() { offset = 0, partial.clear(); }
    void poll(Build& build) {
        SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
        if (!io) return;
        Sint64 size = SDL_GetIOSize(io);
        if (size < offset) { reset(); build = Build{}; }
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

// ---- Processes, files and folders --------------------------------------------------------------------------------

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
std::string narrow(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
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

void open_path(const std::string& path) {  // a folder in the file manager, or a file with its usual program
    if (path.empty()) return;
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", wide(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    std::string url = "file://";
    for (unsigned char c : path) {
        if (isalnum(c) || strchr("/-_.~", c)) url += char(c);
        else { char hex[4]; snprintf(hex, sizeof hex, "%%%02X", c); url += hex; }
    }
    SDL_OpenURL(url.c_str());
#endif
}

// ---- Time ----------------------------------------------------------------------------------------------------------

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

// ---- Drawing -------------------------------------------------------------------------------------------------------

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
    dl->AddCircle(ImVec2(c.x, c.y - h * 0.12f), size * 0.2f, col(kGoldDeep), 40, size * 0.03f);  // ring
    float bw = size * 0.05f;                                                                      // sword
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

void ornament(ImDrawList* dl, float x0, float x1, float y, float scale) {  // a gold rule with a diamond in the middle
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

void text_at(ImDrawList* dl, ImVec2 p, float size, ImU32 color, const char* text) { dl->AddText(nullptr, size, p, color, text); }

ImVec2 text_size(float size, const char* text) {  // size in pixels, as text_at takes it
    return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
}

// A large button that a controller can reach; primary ones are gold. Returns true when pressed.
bool gold_button(const char* label, ImVec2 size, bool primary, bool enabled = true) {
    ImGui::BeginDisabled(!enabled);
    ImGui::PushStyleColor(ImGuiCol_Button, primary ? vec(kGoldDeep) : vec(rgba(24, 44, 92)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, primary ? vec(kGold) : vec(rgba(36, 62, 122)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, primary ? vec(kGoldLight) : vec(rgba(48, 80, 150)));
    ImGui::PushStyleColor(ImGuiCol_Text, primary ? vec(rgba(14, 20, 38)) : vec(kIvory));
    ImGui::PushStyleColor(ImGuiCol_Border, vec(kGold));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
    ImGui::EndDisabled();
    return pressed && enabled;
}

// ---- The screen: backdrop, header, a steps panel on the left and the main panel on the right ------------------------

struct Frame {
    ImDrawList* dl = nullptr;
    ImVec2 origin, size, steps_a, steps_b, main_a, main_b;
    float scale = 1, fb = 20, fs = 20, pad = 28, footer_y = 0;
    float cx = 0, cw = 0;  // the main panel's content column (screen x and width)
    double t = 0;
};

Frame begin_frame(float scale, const char* subtitle, const std::string& top_right, bool main_failed) {
    Frame f;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    f.origin = vp->WorkPos, f.size = vp->WorkSize, f.scale = scale, f.t = ImGui::GetTime();
    ImVec2 o = f.origin, sz = f.size;
    bg->AddRectFilledMultiColor(o, ImVec2(o.x + sz.x, o.y + sz.y), kSkyTop, kSkyTop, kSkyBottom, kSkyBottom);
    draw_crest(bg, ImVec2(o.x + sz.x * 0.86f, o.y + sz.y * 0.66f), sz.y * 0.62f, 0.07f);  // watermark

    ImGui::SetNextWindowPos(o);
    ImGui::SetNextWindowSize(sz);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("frame", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();
    f.dl = ImGui::GetWindowDrawList();
    // fb: the base font size for ImGui's own text (it applies the scale itself); fs: pixels, for the draw list.
    f.pad = 28 * scale, f.fb = ImGui::GetStyle().FontSizeBase, f.fs = f.fb * scale;

    float head = 112 * scale;
    draw_crest(f.dl, ImVec2(o.x + f.pad + 34 * scale, o.y + head * 0.5f + 2 * scale), 64 * scale, 1.0f);
    float tx = o.x + f.pad + 84 * scale;
    text_at(f.dl, ImVec2(tx, o.y + 22 * scale), f.fs * 1.75f, kGold, "FIRE EMBLEM WARRIORS");
    text_at(f.dl, ImVec2(tx + 2 * scale, o.y + 22 * scale + f.fs * 1.95f), f.fs * 0.95f, kMuted, subtitle);
    if (!top_right.empty()) {
        ImVec2 es = text_size(f.fs, top_right.c_str());
        text_at(f.dl, ImVec2(o.x + sz.x - f.pad - es.x, o.y + 30 * scale), f.fs, kMuted, top_right.c_str());
    }
    ornament(f.dl, o.x + f.pad, o.x + sz.x - f.pad, o.y + head, scale);

    float top = o.y + head + 22 * scale, bottom = o.y + sz.y - 64 * scale;
    float left_w = std::max(260 * scale, sz.x * 0.30f);
    f.steps_a = ImVec2(o.x + f.pad, top), f.steps_b = ImVec2(o.x + f.pad + left_w, bottom);
    f.main_a = ImVec2(f.steps_b.x + 22 * scale, top), f.main_b = ImVec2(o.x + sz.x - f.pad, bottom);
    f.footer_y = bottom + 22 * scale;
    panel(f.dl, f.steps_a, f.steps_b, scale);
    panel(f.dl, f.main_a, f.main_b, scale, main_failed ? kCrimson : kPanelEdge);
    f.cx = f.main_a.x + 30 * scale, f.cw = f.main_b.x - f.main_a.x - 60 * scale;
    return f;
}

// The steps, as diamonds on a gold thread: done (filled), current (pulsing), failed (crimson), to come (outlined).
void draw_steps(const Frame& f, const Build& b) {
    const float scale = f.scale, fs = f.fs;
    ImVec2 la = f.steps_a, lb = f.steps_b;
    text_at(f.dl, ImVec2(la.x + 22 * scale, la.y + 16 * scale), fs * 0.85f, kGold, "STEPS");
    float row = std::min(54 * scale, (lb.y - la.y - 60 * scale) / std::max<size_t>(b.steps.size(), 1));
    float y0 = la.y + 58 * scale;
    for (size_t i = 0; i < b.steps.size(); i++) {
        ImVec2 m(la.x + 32 * scale, y0 + row * float(i) + fs * 0.5f);
        bool done = int(i) < b.current || b.outcome == Outcome::Done;
        bool now = int(i) == b.current && b.outcome != Outcome::Done;
        bool failed = now && (b.outcome == Outcome::Failed || b.outcome == Outcome::Gone);
        if (i + 1 < b.steps.size())
            f.dl->AddLine(ImVec2(m.x, m.y + 9 * scale), ImVec2(m.x, m.y + row - 9 * scale), done ? kGoldDeep : kFaint, 2 * scale);
        if (failed) {
            diamond(f.dl, m, 9 * scale, kCrimson, true);
        } else if (done) {
            diamond(f.dl, m, 9 * scale, kGold, true);
        } else if (now) {
            float pulse = 0.5f + 0.5f * float(sin(f.t * 4.0));
            diamond(f.dl, m, (9 + 3 * pulse) * scale, kGoldLight, false, 2 * scale);
            diamond(f.dl, m, 5 * scale, kGold, true);
        } else {
            diamond(f.dl, m, 8 * scale, kFaint, false, 1.5f * scale);
        }
        ImU32 color = failed ? kCrimson : now ? kGoldLight : done ? kIvory : kMuted;
        text_at(f.dl, ImVec2(m.x + 24 * scale, m.y - fs * 0.5f), fs, color, b.steps[i].label.c_str());
    }
}

// The main panel's content goes between these two, in a child window so that lines start at its left edge.
void begin_main(Frame& f) {
    ImGui::SetCursorScreenPos(ImVec2(f.cx, f.main_a.y + 26 * f.scale));
    ImGui::BeginChild("main", ImVec2(f.cw, f.main_b.y - f.main_a.y - 40 * f.scale), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    f.dl = ImGui::GetWindowDrawList();
    ImGui::PushTextWrapPos(f.cw);
}
void end_main(Frame& f) {
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    f.dl = ImGui::GetWindowDrawList();
}
void end_frame(Frame& f, const char* footer) {
    text_at(f.dl, ImVec2(f.origin.x + f.pad, f.footer_y), f.fs * 0.85f, kFaint, footer);
    ImGui::End();
}

// A running build: the step, the bar with its percentage, the figures and the files that just compiled.
void draw_progress(Frame& f, const Build& b, const char* idle_text) {
    const float scale = f.scale, fs = f.fs, cx = f.cx, cw = f.cw;
    ImGui::PushFont(nullptr, f.fb * 1.45f);
    ImGui::TextColored(vec(kGoldLight), "%s", b.current_label());
    ImGui::PopFont();
    float by = ImGui::GetCursorScreenPos().y + 18 * scale;
    progress_bar(f.dl, ImVec2(cx, by), ImVec2(cx + cw, by + 34 * scale), b.fraction, f.t, scale);
    if (b.fraction >= 0) {
        char pct[16];
        snprintf(pct, sizeof pct, "%d%%", int(b.fraction * 100));
        ImVec2 ps = text_size(fs, pct);
        ImVec2 at(cx + (cw - ps.x) * 0.5f, by + (34 * scale - ps.y) * 0.5f);
        text_at(f.dl, ImVec2(at.x + 1, at.y + 1), fs, rgba(0, 0, 0, 160), pct);
        text_at(f.dl, at, fs, kIvory, pct);
    }
    float fy = by + 52 * scale, col = cw / 3;
    auto figure = [&](int i, const char* name, const std::string& value) {
        text_at(f.dl, ImVec2(cx + col * i, fy), fs * 0.8f, kMuted, name);
        text_at(f.dl, ImVec2(cx + col * i, fy + fs * 1.0f), fs * 1.15f, kIvory, value.c_str());
    };
    if (b.compiling() && b.total > 0) {
        figure(0, "BUILD STEPS", std::to_string(b.done) + " of " + std::to_string(b.total));
        figure(1, "TIME LEFT", time_left(b.eta));
    } else {
        figure(0, "STATUS", b.detail.empty() ? std::string(b.started ? "Working..." : idle_text) : b.detail);
    }
    figure(2, "THIS STEP", b.current >= 0 ? "step " + std::to_string(b.current + 1) + " of " + std::to_string(b.steps.size())
                                          : std::string("-"));
    float ly = fy + fs * 3.2f;
    if (b.compiling() && !b.recent.empty()) {
        text_at(f.dl, ImVec2(cx, ly), fs * 0.8f, kMuted, "JUST COMPILED");
        float yy = ly + fs * 1.2f;
        for (size_t i = 0; i < b.recent.size() && yy < f.main_b.y - fs * 1.5f; i++, yy += fs * 1.25f) {
            diamond(f.dl, ImVec2(cx + 5 * scale, yy + fs * 0.5f), 3 * scale, i == 0 ? kGold : kGoldDeep, true);
            text_at(f.dl, ImVec2(cx + 18 * scale, yy), fs * 0.95f, i == 0 ? kIvory : kMuted, b.recent[i].c_str());
        }
    } else if (b.compiling()) {
        text_at(f.dl, ImVec2(cx, ly), fs * 0.9f, kMuted,
                "The first build compiles the whole game: about 20-45 minutes on a desktop PC, longer on a Steam Deck.");
    }
    ImGui::SetCursorScreenPos(ImVec2(cx, ly + fs * 9.5f));  // buttons that follow (the setup's Cancel) go here
    ImGui::Dummy(ImVec2(0, 0));  // ImGui wants an item after moving the cursor past the content
}

}  // namespace
