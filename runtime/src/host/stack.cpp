// Host call stack of another thread, as text, for stall reports and HWDER_THREAD_DUMP (see host/platform.h).
// Nothing here runs unless a report is made: registering a thread only stores its handle.
#include "host/platform.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#ifndef _WIN32
#include <cxxabi.h>
#endif

#include "hwder/dispatch.h"

namespace host {
namespace {
// "f_<address>" gains the game's symbol name when tools/recomp/symbols.txt has one; C++ names are demangled.
std::string pretty(const char* name) {
    std::string s = name;
#ifndef _WIN32
    if (s.rfind("_Z", 0) == 0) {
        int status = 0;
        char* d = abi::__cxa_demangle(name, nullptr, nullptr, &status);
        if (status == 0 && d) s = d;
        std::free(d);
        // Parameter lists make a stack unreadable: keep the qualified name.
        const size_t paren = s.find('(');
        if (paren != std::string::npos) s.resize(paren);
    }
#endif
    if (s.rfind("f_", 0) == 0) {
        char* end = nullptr;
        const unsigned long long addr = std::strtoull(s.c_str() + 2, &end, 16);
        if (end && *end == 0)
            if (const char* n = hw_symbol_name(addr)) s += std::string("(") + n + ")";
    }
    return s;
}
}  // namespace
}  // namespace host

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>

namespace host {
namespace {
std::mutex g_reg_m;
std::vector<std::pair<std::string, HANDLE>> g_reg;

std::string describe(DWORD64 pc) {
    static std::mutex m;  // DbgHelp is single-threaded
    std::lock_guard<std::mutex> l(m);
    char buf[sizeof(SYMBOL_INFO) + 256] = {};
    auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    DWORD64 disp = 0;
    char out[320];
    if (SymFromAddr(GetCurrentProcess(), pc, &disp, sym))
        std::snprintf(out, sizeof(out), "%s+0x%llx", pretty(sym->Name).c_str(), static_cast<unsigned long long>(disp));
    else
        std::snprintf(out, sizeof(out), "?%#llx", static_cast<unsigned long long>(pc));
    return out;
}

std::string capture(HANDLE h) {
    DWORD64 pcs[32];
    int n = 0;
    if (SuspendThread(h) == static_cast<DWORD>(-1)) return {};
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    if (GetThreadContext(h, &ctx)) {
        // RtlVirtualUnwind does not allocate, so it is safe while the target is suspended (it may hold the heap lock).
        for (; n < 32 && ctx.Rip; n++) {
            pcs[n] = ctx.Rip;
            DWORD64 base = 0;
            PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(ctx.Rip, &base, nullptr);
            if (!rf) {  // leaf function: the return address is at [rsp]
                ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
                ctx.Rsp += 8;
                continue;
            }
            void* hd = nullptr;
            DWORD64 ef = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, rf, &ctx, &hd, &ef, nullptr);
        }
    }
    ResumeThread(h);
    std::string s;
    for (int i = 0; i < n; i++) s += (i ? " < " : "") + describe(pcs[i]);
    return s;
}
}  // namespace

std::string thread_stack_text(void* thread) { return thread ? capture(static_cast<HANDLE>(thread)) : std::string(); }

void diag_thread_register(const char* name) {
    HANDLE h = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &h,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0))
        return;
    std::lock_guard<std::mutex> l(g_reg_m);
    g_reg.push_back({name, h});
}

std::vector<std::pair<std::string, std::string>> diag_thread_stacks() {
    std::vector<std::pair<std::string, HANDLE>> reg;
    {
        std::lock_guard<std::mutex> l(g_reg_m);
        reg = g_reg;
    }
    std::vector<std::pair<std::string, std::string>> out;
    for (auto& [name, h] : reg) out.push_back({name, capture(h)});
    return out;
}
}  // namespace host

#else
#include <dlfcn.h>
#include <execinfo.h>
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include <atomic>

namespace host {
std::uintptr_t thread_stack_top(void* thread);  // host/thread.cpp
std::uintptr_t current_stack_top();

namespace {
std::mutex g_reg_m;
std::vector<std::pair<std::string, std::pair<pid_t, std::uintptr_t>>> g_reg;  // name, {tid, stack top}

// One capture at a time (g_cap_m). States: 0 idle, 1 signal sent, 2 target stopped in the handler (registers and
// backtrace saved), 3 released by the capturer. The handler leaves on 3 or after 2 s, whichever comes first.
std::mutex g_cap_m;
std::atomic<int> g_state{0};
std::atomic<pid_t> g_target{0};
std::uintptr_t g_rip = 0, g_rsp = 0;
void* g_bt[32];
int g_bt_n = 0;
std::uintptr_t g_text_lo = 0, g_text_hi = 0;  // the executable's code, for the stack scan
int g_signal = 0;

long long now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ll + ts.tv_nsec;
}

void on_capture(int, siginfo_t*, void* opaque) {
    if (g_state.load() != 1 || static_cast<pid_t>(syscall(SYS_gettid)) != g_target.load()) return;
    const auto* uc = static_cast<const ucontext_t*>(opaque);
    g_rip = static_cast<std::uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
    g_rsp = static_cast<std::uintptr_t>(uc->uc_mcontext.gregs[REG_RSP]);
    g_bt_n = backtrace(g_bt, 32);  // loaded once in setup(), as the crash handler does
    int expected = 1;
    if (!g_state.compare_exchange_strong(expected, 2)) return;  // the capturer gave up
    const long long t0 = now_ns();
    while (g_state.load() == 2 && now_ns() - t0 < 2000000000ll) {
        timespec d{0, 200000};
        nanosleep(&d, nullptr);
    }
    g_state.store(0);
}

void setup() {
    static std::once_flag once;
    std::call_once(once, [] {
        void* warm[1];
        backtrace(warm, 1);
        dl_iterate_phdr(
            [](dl_phdr_info* info, size_t, void*) -> int {
                for (int i = 0; i < info->dlpi_phnum; i++) {
                    const auto& ph = info->dlpi_phdr[i];
                    if (ph.p_type != PT_LOAD || !(ph.p_flags & PF_X)) continue;
                    g_text_lo = info->dlpi_addr + ph.p_vaddr;
                    g_text_hi = g_text_lo + ph.p_memsz;
                    break;
                }
                return 1;  // the first object is the executable
            },
            nullptr);
        g_signal = SIGRTMIN + 6;
        struct sigaction sa{};
        sa.sa_sigaction = on_capture;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&sa.sa_mask);
        sigaction(g_signal, &sa, nullptr);
    });
}

std::string describe(std::uintptr_t pc) {
    Dl_info info{};
    char out[320];
    if (dladdr(reinterpret_cast<void*>(pc), &info)) {
        if (info.dli_sname)
            std::snprintf(out, sizeof(out), "%s+0x%llx", pretty(info.dli_sname).c_str(),
                          static_cast<unsigned long long>(pc - reinterpret_cast<std::uintptr_t>(info.dli_saddr)));
        else {
            const char* file = info.dli_fname ? std::strrchr(info.dli_fname, '/') : nullptr;
            file = file ? file + 1 : (info.dli_fname ? info.dli_fname : "?");
            std::snprintf(out, sizeof(out), "%s+0x%llx", file,
                          static_cast<unsigned long long>(pc - reinterpret_cast<std::uintptr_t>(info.dli_fbase)));
        }
    } else {
        std::snprintf(out, sizeof(out), "?%#llx", static_cast<unsigned long long>(pc));
    }
    return out;
}

// By thread id (tgkill), not pthread_t: a guest thread that has just exited only makes the signal fail.
std::string capture(pid_t tid, std::uintptr_t stack_hi) {
    if (!tid) return "(not started)";
    setup();
    if (tid == static_cast<pid_t>(syscall(SYS_gettid))) {  // the caller itself: no signal needed
        void* bt[32];
        const int n = backtrace(bt, 32);
        std::string s = "bt:";
        for (int i = 1; i < n; i++) s += (i > 1 ? " < " : " ") + describe(reinterpret_cast<std::uintptr_t>(bt[i]));
        return s;
    }
    std::lock_guard<std::mutex> l(g_cap_m);
    if (g_state.load() != 0) return "(a previous capture is still pending)";
    g_target.store(tid);  // the handler checks it runs on the target (a late signal from a capture that gave up)
    g_state.store(1);
    if (syscall(SYS_tgkill, getpid(), tid, g_signal) != 0) {
        g_state.store(0);
        return "(signal failed)";
    }
    const long long t0 = now_ns();
    while (g_state.load() == 1 && now_ns() - t0 < 500000000ll) {
        timespec d{0, 100000};
        nanosleep(&d, nullptr);
    }
    int expected = 1;
    if (g_state.compare_exchange_strong(expected, 0)) return "(thread did not stop)";
    // Stopped in the handler: its stack does not change while it is scanned.
    std::vector<std::uintptr_t> scan;
    if (stack_hi > g_rsp && g_text_hi) {
        const std::uintptr_t end = std::min<std::uintptr_t>(stack_hi, g_rsp + (512u << 10));
        for (std::uintptr_t p = g_rsp & ~std::uintptr_t(7); p + 8 <= end && scan.size() < 24; p += 8) {
            const std::uintptr_t v = *reinterpret_cast<const std::uintptr_t*>(p);
            if (v >= g_text_lo && v < g_text_hi && (scan.empty() || scan.back() != v)) scan.push_back(v);
        }
    }
    void* bt[32];
    const int bt_n = std::min(g_bt_n, 32);
    std::memcpy(bt, g_bt, sizeof(void*) * bt_n);
    const std::uintptr_t rip = g_rip;
    g_state.store(3);
    const long long t1 = now_ns();
    while (g_state.load() == 3 && now_ns() - t1 < 200000000ll) {
        timespec d{0, 100000};
        nanosleep(&d, nullptr);
    }
    g_state.store(0);  // the handler leaves on anything but 2 (or has already left after its own timeout)
    std::string s = "pc " + describe(rip) + "; bt:";
    // backtrace() starts inside the handler: skip to the interrupted frame.
    int first = 0;
    for (int i = 0; i < bt_n; i++)
        if (reinterpret_cast<std::uintptr_t>(bt[i]) == rip) first = i + 1;
    for (int i = first; i < bt_n; i++) s += (i > first ? " < " : " ") + describe(reinterpret_cast<std::uintptr_t>(bt[i]));
    if (bt_n - first < 4) {  // the unwinder stopped early: the scan still shows the callers (some may be stale)
        s += "; stack scan:";
        for (size_t i = 0; i < scan.size(); i++) s += (i ? " < " : " ") + describe(scan[i]);
    }
    return s;
}
}  // namespace

std::string thread_stack_text(void* thread) {
    return thread ? capture(static_cast<pid_t>(thread_os_id(thread)), thread_stack_top(thread)) : std::string();
}

void diag_thread_register(const char* name) {
    const std::uintptr_t top = current_stack_top();
    std::lock_guard<std::mutex> l(g_reg_m);
    g_reg.push_back({name, {static_cast<pid_t>(thread_id()), top}});
}

std::vector<std::pair<std::string, std::string>> diag_thread_stacks() {
    std::vector<std::pair<std::string, std::pair<pid_t, std::uintptr_t>>> reg;
    {
        std::lock_guard<std::mutex> l(g_reg_m);
        reg = g_reg;
    }
    std::vector<std::pair<std::string, std::string>> out;
    for (auto& [name, t] : reg) out.push_back({name, capture(t.first, t.second)});
    return out;
}
}  // namespace host
#endif
