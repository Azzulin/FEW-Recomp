// HWDER_INPUT_SCRIPT (input_script.h).
#include "hid/input_script.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace hid {

namespace {

struct Event {
    u64 start = 0, dur = 0, every = 0, until = ~0ull;
    u64 buttons = 0;
    bool set_lx = false, set_ly = false, set_rx = false, set_ry = false;
    s32 lx = 0, ly = 0, rx = 0, ry = 0;
};

u64 button(const std::string& n) {
    static const struct { const char* name; u64 bit; } kNames[] = {
        {"A", kBtnA},       {"B", kBtnB},         {"X", kBtnX},       {"Y", kBtnY},         {"L", kBtnL},
        {"R", kBtnR},       {"ZL", kBtnZL},       {"ZR", kBtnZR},     {"PLUS", kBtnPlus},   {"MINUS", kBtnMinus},
        {"UP", kBtnUp},     {"DOWN", kBtnDown},   {"LEFT", kBtnLeft}, {"RIGHT", kBtnRight}, {"LS", kBtnStickL},
        {"RS", kBtnStickR}, {"-", 0},
    };
    for (auto& k : kNames)
        if (n == k.name) return k.bit;
    hw_log("input-script: unknown button '%s'", n.c_str());
    return 0;
}

std::vector<Event> load(const char* path) {
    std::vector<Event> out;
    FILE* f = fopen(path, "rb");
    if (!f) {
        hw_log("input-script: cannot open %s", path);
        return out;
    }
    char line[512];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        n++;
        if (char* h = strchr(line, '#')) *h = 0;
        std::istringstream ss(line);
        Event e;
        if (!(ss >> e.start >> e.dur)) continue;
        std::string tok;
        while (ss >> tok) {
            auto num = [&](const char* key, s32& v, bool& set) {
                size_t k = strlen(key);
                if (tok.compare(0, k, key) != 0) return false;
                v = (s32)strtol(tok.c_str() + k, nullptr, 0);
                set = true;
                return true;
            };
            if (num("lx=", e.lx, e.set_lx) || num("ly=", e.ly, e.set_ly) || num("rx=", e.rx, e.set_rx) ||
                num("ry=", e.ry, e.set_ry))
                continue;
            if (tok.compare(0, 6, "every=") == 0) { e.every = strtoull(tok.c_str() + 6, nullptr, 0); continue; }
            if (tok.compare(0, 6, "until=") == 0) { e.until = strtoull(tok.c_str() + 6, nullptr, 0); continue; }
            size_t p = 0;
            while (p <= tok.size()) {
                size_t q = tok.find('+', p);
                if (q == std::string::npos) q = tok.size();
                if (q > p) e.buttons |= button(tok.substr(p, q - p));
                p = q + 1;
            }
        }
        out.push_back(e);
    }
    fclose(f);
    hw_log("input-script: %zu events from %s", out.size(), path);
    return out;
}

}  // namespace

bool apply_input_script(u64 t, HostInput& in) {
    static const char* path = getenv("HWDER_INPUT_SCRIPT");
    if (!path || !*path) return false;
    static const std::vector<Event> events = load(path);
    PadInput& p = in.pads[0];
    in.connected[0] = true;
    for (const Event& e : events) {
        if (t < e.start || t >= e.until) continue;
        const u64 rel = t - e.start;
        const bool on = e.every ? (rel % e.every) < e.dur : rel < e.dur;
        if (!on) continue;
        p.buttons |= e.buttons;
        if (e.set_lx) p.lx = e.lx;
        if (e.set_ly) p.ly = e.ly;
        if (e.set_rx) p.rx = e.rx;
        if (e.set_ry) p.ry = e.ry;
    }
    return true;
}

}  // namespace hid
