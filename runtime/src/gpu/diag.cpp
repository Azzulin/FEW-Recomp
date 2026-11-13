// Storage and flush of the draw-path profile (see diag.h). Moved from vk/texture_cache.cpp and vk/vk_debug.cpp.
#include <cstring>
#include <cstdio>

#include "gpu/diag.h"
#include "debug_flags.h"

namespace gpu::diag {

double g_prof[kProfCount];
uint32_t g_prof_draws;
uint64_t g_prof_cbuf_bytes, g_prof_cbufs, g_prof_sets, g_prof_sets_reused, g_prof_vtx_bytes, g_prof_vtx_copies, g_prof_idx_bytes, g_prof_idx_scanned;
static double g_prof_win[kProfCount];
static uint64_t g_prof_win_draws;

// snprintf returns the length it wanted, not what fit: without this clamp a long line moves `o` past the buffer,
// `sizeof(line) - o` wraps around and the next append writes over the stack (crash in flush_commands on the
// first heavy 3D load with HWDER_THREAD_STATS / HWDER_PROFILE_MS). Clamped, a too-long line is only cut short.
static int clamp_len(int o, size_t cap) { return o < 0 ? 0 : (size_t)o >= cap ? (int)cap - 1 : o; }

void prof_window_log(unsigned frames, double busy_ms) {
    if (!frames) return;
    char line[1536];
    int o = clamp_len(snprintf(line, sizeof(line), "vk: thread profile per frame: busy %.2f ms, %.0f draws, ms:", busy_ms / frames,
                               (double)g_prof_win_draws / frames), sizeof(line));
    o = clamp_len(o + snprintf(line + o, sizeof(line) - o, " | %.0f cbufs %.0f KiB, %.0f sets (%.0f reused) |", (double)g_prof_cbufs / frames,
                               g_prof_cbuf_bytes / 1024.0 / frames, (double)g_prof_sets / frames, (double)g_prof_sets_reused / frames), sizeof(line));
    o = clamp_len(o + snprintf(line + o, sizeof(line) - o, " %.0f vertex copies %.0f KiB, index %.0f KiB, %.0f indices scanned |",
                               (double)g_prof_vtx_copies / frames, g_prof_vtx_bytes / 1024.0 / frames, g_prof_idx_bytes / 1024.0 / frames,
                               (double)g_prof_idx_scanned / frames), sizeof(line));
    g_prof_cbufs = g_prof_cbuf_bytes = g_prof_sets = g_prof_sets_reused = g_prof_vtx_bytes = g_prof_vtx_copies = g_prof_idx_bytes = g_prof_idx_scanned = 0;
    for (int i = 0; i < kProfCount; i++)
        if (g_prof_win[i] / frames >= 0.005)
            o = clamp_len(o + snprintf(line + o, sizeof(line) - o, " %s %.2f", kProfNames[i], g_prof_win[i] / frames), sizeof(line));
    hw_log("%s", line);
    memset(g_prof_win, 0, sizeof(g_prof_win));
    g_prof_win_draws = 0;
}

bool prof_enabled() { return debug::flag(debug::ProfileMs) || debug::flag(debug::ThreadStats); }

double prof_flush() {
    double total = 0;
    for (double v : g_prof) total += v;
    static const double limit = debug::value(debug::ProfileMs, 200.0);
    if (total > limit) {
        char line[1536];
        int o = clamp_len(snprintf(line, sizeof(line), "vk: profile: %u draws, %.0f ms:", g_prof_draws, total), sizeof(line));
        for (int i = 0; i < kProfCount; i++)
            o = clamp_len(o + snprintf(line + o, sizeof(line) - o, " %s %.0f", kProfNames[i], g_prof[i]), sizeof(line));
        hw_log("%s", line);
    }
    for (int i = 0; i < kProfCount; i++) g_prof_win[i] += g_prof[i];
    g_prof_win_draws += g_prof_draws;
    memset(g_prof, 0, sizeof(g_prof));
    g_prof_draws = 0;
    return total;
}

}  // namespace gpu::diag
