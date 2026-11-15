// HWDER_SAMPLE_US: a CPU-time sampling profiler for host threads (Linux; the GPU command thread and, in battle,
// MainThread). Every that many microseconds of a thread's own CPU time a SIGPROF records its instruction address
// and the current phase tag (g_sample_phase). Every 10 s each thread's counts are written to its own file beside
// the log (<log>.samples.txt, <log>.samples-main.txt): per function, per phase and function, and per address
// (count, offset in the executable, function), so `addr2line -f -i -e hwder <offset>` gives the source line.
// Windows: wall-clock samples of the same threads, see the second half of this file.
#include "host/sampler.h"

#include "host/platform.h"
#include "hwder/runtime.h"

namespace host {
volatile unsigned char g_sample_phase = 0;
}

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include "debug_flags.h"
#include "host/platform.h"
#include "host/timing.h"

namespace host {
namespace {
struct TlEvent {
    long long t;
    const char* what;
    unsigned thread;
    unsigned long long a, b;
};
constexpr unsigned kTlMax = 1u << 18;
TlEvent* g_tl = nullptr;
std::atomic<unsigned> g_tl_n{0};
long long g_tl_start = 0, g_tl_end = 0, g_tl_freq = 1;

bool timeline_init() {
    const char* e = debug::str(debug::Timeline);
    if (!e || !*e) return false;
    Counter f, now;
    counter_frequency(&f);
    counter(&now);
    g_tl_freq = f.QuadPart;
    g_tl_start = now.QuadPart + (long long)(std::atof(e) * f.QuadPart);
    const char* comma = std::strchr(e, ',');  // "<start>,<seconds>": a longer window than the default 0.5 s
    const double secs = comma ? std::atof(comma + 1) : 0.5;
    g_tl_end = g_tl_start + (long long)((secs > 0 ? secs : 0.5) * f.QuadPart);
    g_tl = static_cast<TlEvent*>(std::calloc(kTlMax, sizeof(TlEvent)));
    const char* log = debug::str(debug::Log);
    std::string path = std::string(log && *log ? log : "hwder.log") + ".timeline.txt";
    std::thread([path] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            Counter c;
            counter(&c);
            if (c.QuadPart > g_tl_end + g_tl_freq / 10) break;
        }
        FILE* f = std::fopen(path.c_str(), "w");
        if (!f) return;
        const unsigned n = std::min(g_tl_n.load(), kTlMax);
        for (unsigned i = 0; i < n; i++) {
            const TlEvent& ev = g_tl[i];
            if (!ev.what) continue;
            std::fprintf(f, "%10.1f %7u %-16s %llu %llu\n", (ev.t - g_tl_start) * 1e6 / g_tl_freq, ev.thread, ev.what, ev.a, ev.b);
        }
        std::fclose(f);
    }).detach();
    return true;
}
}  // namespace

bool timeline_on() {
    static const bool on = timeline_init();
    if (!on) return false;
    Counter c;
    counter(&c);
    return c.QuadPart >= g_tl_start && c.QuadPart < g_tl_end;
}

void timeline_event(const char* what, unsigned long long a, unsigned long long b) {
    Counter c;
    counter(&c);
    const unsigned i = g_tl_n.fetch_add(1, std::memory_order_relaxed);
    if (i >= kTlMax) return;
    static thread_local unsigned tid = thread_id();
    g_tl[i] = {c.QuadPart, what, tid, a, b};
}
}  // namespace host

#ifndef _WIN32
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cxxabi.h>
#include <dlfcn.h>
#include <map>
#include <mutex>
#include <signal.h>
#include <string>
#include <sys/syscall.h>
#include <thread>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace host {
namespace {
constexpr std::uint32_t kMaxSamples = 1u << 22;
struct Sampler {
    std::string path;
    std::uintptr_t* pcs = nullptr;
    unsigned char* phases = nullptr;
    std::atomic<std::uint32_t> count{0};
};
thread_local Sampler* t_sampler = nullptr;
std::mutex g_m;
std::vector<Sampler*> g_samplers;
const char* g_phase_names[256] = {};

void on_sample(int, siginfo_t*, void* opaque) {
    Sampler* s = t_sampler;
    if (!s) return;
    const auto* uc = static_cast<const ucontext_t*>(opaque);
    const std::uint32_t i = s->count.fetch_add(1, std::memory_order_relaxed);
    if (i < kMaxSamples) {
        s->pcs[i] = static_cast<std::uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
        s->phases[i] = g_sample_phase;
    }
}

std::string symbolize(std::uintptr_t pc, std::uintptr_t* off) {
    Dl_info info{};
    std::string name = "?";
    *off = pc;
    if (dladdr(reinterpret_cast<void*>(pc), &info)) {
        *off = pc - reinterpret_cast<std::uintptr_t>(info.dli_fbase);
        if (info.dli_sname) {
            int status = 0;
            char* d = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
            name = status == 0 && d ? d : info.dli_sname;
            std::free(d);
        } else if (info.dli_fname) {
            name = std::string("[") + info.dli_fname + "]";
        }
    }
    return name;
}

template <class K>
std::vector<std::pair<std::uint32_t, K>> ranked(const std::unordered_map<K, std::uint32_t>& m) {
    std::vector<std::pair<std::uint32_t, K>> rows;
    for (auto& [k, c] : m) rows.push_back({c, k});
    std::sort(rows.rbegin(), rows.rend());
    return rows;
}

void dump(Sampler& s) {
    const std::uint32_t n = std::min(s.count.load(), kMaxSamples);
    std::unordered_map<std::uintptr_t, std::uint32_t> hist;
    std::map<unsigned char, std::unordered_map<std::uintptr_t, std::uint32_t>> by_phase_pc;
    std::map<unsigned char, std::uint32_t> phase_total;
    for (std::uint32_t i = 0; i < n; i++) {
        hist[s.pcs[i]]++;
        by_phase_pc[s.phases[i]][s.pcs[i]]++;
        phase_total[s.phases[i]]++;
    }
    std::unordered_map<std::uintptr_t, std::string> names;
    std::unordered_map<std::uintptr_t, std::uintptr_t> offs;
    auto name_of = [&](std::uintptr_t pc) -> const std::string& {
        auto it = names.find(pc);
        if (it != names.end()) return it->second;
        std::uintptr_t off;
        std::string nm = symbolize(pc, &off);
        offs[pc] = off;
        return names[pc] = nm;
    };
    // Per function as well: the leaf address alone splits a hot loop over many lines.
    std::unordered_map<std::string, std::uint32_t> funcs;
    const auto rows = ranked(hist);
    std::vector<std::string> lines;
    for (auto& [c, pc] : rows) {
        const std::string& name = name_of(pc);
        funcs[name] += c;
        if (lines.size() < 400) {
            char b[64];
            std::snprintf(b, sizeof(b), "%8u %#12llx ", c, static_cast<unsigned long long>(offs[pc]));
            lines.push_back(b + name.substr(0, 160));
        }
    }
    const std::string tmp = s.path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) return;
    std::uint64_t watch_calls = 0, watch_ns = 0;
    write_watch_totals(&watch_calls, &watch_ns);
    std::fprintf(f, "%u samples; write watch: %llu calls, %.1f ms total\n\nby function:\n", n,
                 static_cast<unsigned long long>(watch_calls), watch_ns / 1e6);
    const auto by_func = ranked(funcs);
    for (std::size_t i = 0; i < by_func.size() && i < 80; i++)
        std::fprintf(f, "%8u %5.1f%% %s\n", by_func[i].first, 100.0 * by_func[i].first / std::max(n, 1u),
                     by_func[i].second.substr(0, 200).c_str());
    if (phase_total.size() > 1) {
        std::fprintf(f, "\nby phase:\n");
        for (auto& [p, c] : phase_total)
            std::fprintf(f, "%8u %5.1f%% %s\n", c, 100.0 * c / std::max(n, 1u),
                         g_phase_names[p] ? g_phase_names[p] : (p ? "?" : "untagged"));
        for (auto& [p, pcs] : by_phase_pc) {
            std::unordered_map<std::string, std::uint32_t> pf;
            for (auto& [pc, c] : pcs) pf[name_of(pc)] += c;
            const auto r = ranked(pf);
            std::fprintf(f, "\nphase %s (%u samples), by function:\n",
                         g_phase_names[p] ? g_phase_names[p] : (p ? "?" : "untagged"), phase_total[p]);
            for (std::size_t i = 0; i < r.size() && i < 60; i++)
                std::fprintf(f, "%8u %5.1f%% %s\n", r[i].first, 100.0 * r[i].first / std::max(phase_total[p], 1u),
                             r[i].second.substr(0, 200).c_str());
        }
    }
    std::fprintf(f, "\nby address (count, offset in the executable, function):\n");
    for (const auto& l : lines) std::fprintf(f, "%s\n", l.c_str());
    std::fclose(f);
    std::rename(tmp.c_str(), s.path.c_str());
}
}  // namespace

void name_sample_phase(unsigned char phase, const char* name) { g_phase_names[phase] = name; }

void sample_current_thread(int period_us, const std::string& path) {
    if (period_us <= 0 || t_sampler) return;
    auto* s = new Sampler;
    s->path = path;
    s->pcs = static_cast<std::uintptr_t*>(std::calloc(kMaxSamples, sizeof(std::uintptr_t)));
    s->phases = static_cast<unsigned char*>(std::calloc(kMaxSamples, 1));
    if (!s->pcs || !s->phases) return;
    bool first;
    {
        std::lock_guard<std::mutex> l(g_m);
        first = g_samplers.empty();
        g_samplers.push_back(s);
    }
    t_sampler = s;
    if (first) {
        struct sigaction sa{};
        sa.sa_sigaction = on_sample;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGPROF, &sa, nullptr);
    }
    sigevent sev{};
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = SIGPROF;
    sev._sigev_un._tid = static_cast<pid_t>(syscall(SYS_gettid));
    timer_t timer;
    if (timer_create(CLOCK_THREAD_CPUTIME_ID, &sev, &timer) != 0) {
        hw_log("sampler: timer_create failed");
        return;
    }
    itimerspec spec{};
    spec.it_interval.tv_nsec = spec.it_value.tv_nsec = static_cast<long>(period_us) * 1000;
    timer_settime(timer, 0, &spec, nullptr);
    hw_log("sampler: sampling this thread every %d us of its CPU time into %s", period_us, path.c_str());
    if (first)
        std::thread([] {
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                std::vector<Sampler*> all;
                {
                    std::lock_guard<std::mutex> l(g_m);
                    all = g_samplers;
                }
                for (Sampler* x : all) dump(*x);
            }
        }).detach();
}
}  // namespace host
#else
// Windows: wall-clock sampling. A sampler thread suspends the target every period, reads its context and unwinds a
// few frames (RtlVirtualUnwind), then resumes it. Each 10 s report covers only the last 10 s. Nothing is allocated while the target is suspended (it may hold
// the heap lock). Waits show up too (the thread's blocked location), which is what a busy figure that includes
// waits needs. Report: by leaf function, by inclusive function (any of the first frames) and by phase.
#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace host {
namespace {
constexpr unsigned kDepth = 12;
constexpr std::uint32_t kMaxSamples = 1u << 20;
struct WinSample {
    std::uintptr_t pc[kDepth];
    unsigned char depth, phase;
};
const char* g_phase_names[256] = {};
std::mutex g_sym_m;

std::string symbol_name(std::uintptr_t pc) {
    std::lock_guard<std::mutex> l(g_sym_m);
    char buf[sizeof(SYMBOL_INFO) + 256] = {};
    auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    DWORD64 disp = 0;
    if (SymFromAddr(GetCurrentProcess(), pc, &disp, sym)) return sym->Name;
    char mod[64];
    std::snprintf(mod, sizeof(mod), "?%#llx", static_cast<unsigned long long>(pc));
    return mod;
}

void dump_win(const std::string& path, const WinSample* s, std::uint32_t n) {
    std::unordered_map<std::uintptr_t, std::string> names;
    auto name_of = [&](std::uintptr_t pc) -> const std::string& {
        auto it = names.find(pc);
        if (it != names.end()) return it->second;
        return names[pc] = symbol_name(pc);
    };
    std::unordered_map<std::string, std::uint32_t> leaf, incl;
    std::map<unsigned char, std::unordered_map<std::string, std::uint32_t>> by_phase;
    std::map<unsigned char, std::uint32_t> phase_total;
    std::unordered_map<std::string, std::uint32_t> callers;  // "leaf <- caller" for the top leaves
    for (std::uint32_t i = 0; i < n; i++) {
        const WinSample& x = s[i];
        const std::string& l0 = name_of(x.pc[0]);
        leaf[l0]++;
        by_phase[x.phase][l0]++;
        phase_total[x.phase]++;
        std::unordered_set<std::string> seen;
        for (unsigned d = 0; d < x.depth; d++) {
            const std::string& nm = name_of(x.pc[d]);
            if (seen.insert(nm).second) incl[nm]++;
        }
        std::string chain = l0;
        for (unsigned d = 1; d < x.depth && d < 4; d++) chain += " <- " + name_of(x.pc[d]);
        callers[chain]++;
    }
    auto ranked = [](const std::unordered_map<std::string, std::uint32_t>& m) {
        std::vector<std::pair<std::uint32_t, std::string>> r;
        for (auto& [k, c] : m) r.push_back({c, k});
        std::sort(r.rbegin(), r.rend());
        return r;
    };
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "%u samples (wall clock)\n\nby leaf function:\n", n);
    auto r = ranked(leaf);
    for (std::size_t i = 0; i < r.size() && i < 60; i++)
        std::fprintf(f, "%8u %5.1f%% %s\n", r[i].first, 100.0 * r[i].first / std::max(n, 1u), r[i].second.substr(0, 200).c_str());
    std::fprintf(f, "\ninclusive (function anywhere in the first %u frames):\n", kDepth);
    r = ranked(incl);
    for (std::size_t i = 0; i < r.size() && i < 80; i++)
        std::fprintf(f, "%8u %5.1f%% %s\n", r[i].first, 100.0 * r[i].first / std::max(n, 1u), r[i].second.substr(0, 200).c_str());
    std::fprintf(f, "\nleaf <- callers:\n");
    r = ranked(callers);
    for (std::size_t i = 0; i < r.size() && i < 80; i++)
        std::fprintf(f, "%8u %5.1f%% %s\n", r[i].first, 100.0 * r[i].first / std::max(n, 1u), r[i].second.substr(0, 300).c_str());
    if (phase_total.size() > 1) {
        std::fprintf(f, "\nby phase:\n");
        for (auto& [p, c] : phase_total)
            std::fprintf(f, "%8u %5.1f%% %s\n", c, 100.0 * c / std::max(n, 1u), g_phase_names[p] ? g_phase_names[p] : (p ? "?" : "untagged"));
    }
    std::fclose(f);
    std::remove(path.c_str());
    std::rename(tmp.c_str(), path.c_str());
}
}  // namespace

void name_sample_phase(unsigned char phase, const char* name) { g_phase_names[phase] = name; }

void sample_current_thread(int period_us, const std::string& path) {
    if (period_us <= 0) return;
    HANDLE h = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &h,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0)) {
        hw_log("sampler: DuplicateHandle failed");
        return;
    }
    auto* buf = static_cast<WinSample*>(std::calloc(kMaxSamples, sizeof(WinSample)));
    if (!buf) return;
    hw_log("sampler: sampling this thread every %d us of wall time into %s", period_us, path.c_str());
    std::thread([h, buf, period_us, path] {
        std::uint32_t n = 0;
        auto last_dump = std::chrono::steady_clock::now();
        for (;;) {
            if (period_us >= 1000) Sleep(period_us / 1000);
            else std::this_thread::sleep_for(std::chrono::microseconds(period_us));
            if (SuspendThread(h) == (DWORD)-1) break;
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            if (GetThreadContext(h, &ctx) && n < kMaxSamples) {
                WinSample& s = buf[n];
                s.phase = g_sample_phase;
                s.depth = 0;
                for (unsigned d = 0; d < kDepth && ctx.Rip; d++) {
                    s.pc[s.depth++] = ctx.Rip;
                    DWORD64 image = 0;
                    PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &image, nullptr);
                    if (!fn) {  // leaf function: the return address is at [rsp]
                        ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
                        ctx.Rsp += 8;
                        continue;
                    }
                    void* handler_data = nullptr;
                    DWORD64 frame = 0;
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, ctx.Rip, fn, &ctx, &handler_data, &frame, nullptr);
                }
                n++;
            }
            ResumeThread(h);
            if (std::chrono::steady_clock::now() - last_dump > std::chrono::seconds(10)) {
                dump_win(path, buf, n);  // the last 10 s only: the file always shows the latest window
                n = 0;
                last_dump = std::chrono::steady_clock::now();
            }
        }
    }).detach();
}
}  // namespace host
#endif
