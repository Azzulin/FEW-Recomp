// Stall reports. The GPU hang report (gpu::dump_hang_state) only runs when the compositor waits on a frame fence,
// so a game that stops queueing frames altogether (one of its threads waits on something that never comes, or it
// waits for the GPU to go idle while the GPU thread is busy) left the log silent. Seen on the Steam Deck: a black
// screen at a cutscene, nothing logged for the 53 s before the window was closed. This watcher notices the missing
// frames and logs, once at the threshold and once more at 30 s: the GPU and vi state, what each guest thread waits
// on, and the host stack of every guest and runtime thread (recompiled guest functions appear as f_<address>).
#include "stall_watch.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#include "debug_flags.h"
#include "gpu/gpu.h"
#include "host/platform.h"
#include "host/timing.h"
#include "hwder/runtime.h"
#include "kernel/kernel.h"

namespace ipc {
std::string vi_queue_state();  // gpu/vi.cpp
}

namespace stall {
namespace {
std::atomic<unsigned long long> g_frames{0}, g_last_frame_ms{0};

// A stack can be a few hundred characters per frame: one log line per chunk of frames.
void log_stack(const char* prefix, const std::string& stack) {
    const size_t kChunk = 1500;
    if (stack.size() <= kChunk) {
        hw_log("%s%s", prefix, stack.c_str());
        return;
    }
    for (size_t at = 0; at < stack.size();) {
        size_t end = std::min(stack.size(), at + kChunk);
        if (end < stack.size()) {
            const size_t cut = stack.rfind(" < ", end);
            if (cut != std::string::npos && cut > at) end = cut;
        }
        hw_log("%s%s%s", prefix, at ? "... " : "", stack.substr(at, end - at).c_str());
        at = end;
    }
}

void report(double seconds) {
    hw_log("stall: no frame from the game for %.1f s (%llu frames before); GPU, vi and thread state follow", seconds,
           g_frames.load());
    gpu::dump_hang_state();
    hw_log("stall: vi: %s", ipc::vi_queue_state().c_str());
    dump_guest_threads();
    for (const auto& [name, stack] : host::diag_thread_stacks()) {
        const std::string prefix = "stall: runtime thread '" + name + "': ";
        log_stack(prefix.c_str(), stack);
    }
    hw_log("stall: end of report");
}

void watch(double threshold_s) {
    host::name_thread(L"HWDER stall watch");
    int reports = 0;  // per stall: at the threshold and at 30 s
    for (;;) {
        host::sleep_ms(500);
        const unsigned long long frames = g_frames.load(std::memory_order_relaxed);
        if (frames < 30) continue;  // boot: the game has not started presenting yet
        const unsigned long long last = g_last_frame_ms.load(std::memory_order_relaxed);
        const double idle = (double)(host::ticks_ms() - last) / 1000.0;
        if (idle < threshold_s) {
            if (reports) hw_log("stall: frames resumed (%llu reports for this stall)", (unsigned long long)reports);
            reports = 0;
            continue;
        }
        if (reports == 0 || (reports == 1 && idle >= std::max(30.0, threshold_s * 2))) {
            reports++;
            report(idle);
        }
    }
}
}  // namespace

void note_frame() {
    g_last_frame_ms.store(host::ticks_ms(), std::memory_order_relaxed);
    g_frames.fetch_add(1, std::memory_order_relaxed);
}

void start() {
    const double threshold = debug::value(debug::StallDump, 5.0);
    if (threshold <= 0) return;
    std::thread([threshold] { watch(threshold); }).detach();
}

void dump_guest_threads() {
    const unsigned long long now = host::ticks_ms();
    for (const auto& t : kern::all_threads()) {
        if (!t->started || t->exited || !t->host_handle) continue;
        char wait[160] = "not parked in the kernel";  // running, or blocked inside a service (see the stack)
        if (t->waiting) {
            const char* kind = t->wait_kind.load(std::memory_order_relaxed);
            std::snprintf(wait, sizeof(wait), "waiting on %s 0x%llx for %.1f s", kind ? kind : "?",
                          (unsigned long long)t->wait_obj.load(std::memory_order_relaxed),
                          (double)(now - t->wait_since_ms.load(std::memory_order_relaxed)) / 1000.0);
        }
        char prefix[256];
        std::snprintf(prefix, sizeof(prefix), "stall: thread %llu '%s' prio %d, %s: ", (unsigned long long)t->id,
                      t->name.c_str(), t->priority, wait);
        log_stack(prefix, host::thread_stack_text(t->host_handle));
    }
}
}  // namespace stall
