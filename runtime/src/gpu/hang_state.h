// Post-mortem of a stuck frame: where each runtime GPU thread is blocked, cheap enough to keep on always.
// The threads mark their long waits with `Where`; gpu::dump_hang_state() (called by vi when a frame fence is
// late) logs the marks with the queues and serials around them.
#pragma once
#include <atomic>

namespace gpu::hang {

enum Thread { kGpu, kRecord, kWaiter, kThreads };
inline const char* const kThreadNames[kThreads] = {"gpu", "vk record", "vk waiter"};
inline std::atomic<const char*> g_where[kThreads];
inline std::atomic<unsigned long long> g_arg[kThreads];
inline thread_local int t_thread = -1;  // set once by each thread above; other threads leave no marks

inline void set_thread(Thread t, const char* where) {
    t_thread = t;
    g_where[t].store(where, std::memory_order_relaxed);
}

struct Where {
    int t;
    const char* prev = nullptr;
    unsigned long long prev_arg = 0;
    explicit Where(const char* w, unsigned long long arg = 0) : t(t_thread) {
        if (t < 0) return;
        prev = g_where[t].exchange(w, std::memory_order_relaxed);
        prev_arg = g_arg[t].exchange(arg, std::memory_order_relaxed);
    }
    ~Where() {
        if (t < 0) return;
        g_where[t].store(prev, std::memory_order_relaxed);
        g_arg[t].store(prev_arg, std::memory_order_relaxed);
    }
    Where(const Where&) = delete;
    Where& operator=(const Where&) = delete;
};

}  // namespace gpu::hang
