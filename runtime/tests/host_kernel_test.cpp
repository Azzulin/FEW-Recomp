// Synthetic guest-thread integration: no generated code, game files, saves or keys.
#include "host/platform.h"
#include "kernel/kernel.h"
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
std::atomic<int> entered{0}, finished{0};
std::atomic<bool> woke{false};
void* ready;
void* done;
void check(bool condition, const char* text) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
}
extern "C" void hw_log(const char* format, ...) { va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args); std::fputc('\n', stderr); }
extern "C" void hw_fatal(const char* text) { std::fprintf(stderr, "FATAL: %s\n", text); std::abort(); }
extern "C" void hw_call(Ctx* context, u64 entry) {
    auto* self = kern::current_thread();
    check(self && self->ctx == context, "guest context/thread binding");
    check(context->x[0] == 0x1234 && context->sp == 0x4000100000ull, "guest argument/stack");
    check(context->tpidrro_el0 == self->tls, "guest TLS register");
    if (entry == 1) {
        entered++;
        kern::g_kernel_lock.lock();
        kern::g_kernel_lock.lock();
        host::event_signal(ready);
        woke = kern::park_current_thread(2000000000);
        // Releasing both levels after the wait exercises recursion-depth restoration.
        kern::g_kernel_lock.unlock();
        kern::g_kernel_lock.unlock();
        check(finished.load() == 1, "other thread acquired kernel lock while parked");
        host::event_signal(done);
    } else if (entry == 2) {
        // A short-lived thread: returns at once (ExitThread).
    } else check(false, "synthetic entry");
}
int main() {
    kern::mem::init();
    ready = host::event_create(); done = host::event_create();
    check(ready && done, "test events");
    auto thread = kern::create_thread(1, 0x1234, 0x400010000full, 44, 0);
    thread->handle = kern::g_handles.add(thread);
    check(!entered && !thread->started, "creation is dormant until StartThread");
    check(reinterpret_cast<std::uintptr_t>(thread->ctx) % 64 == 0, "CPU context alignment");
    check(!host::event_wait(ready, 1000000), "dormant callback never runs");
    kern::start_thread(thread.get());
    check(host::event_wait(ready, 2000000000), "guest callback started");
    {
        kern::KGuard lock(kern::g_kernel_lock);
        finished++;
        kern::wake_thread(thread.get());
    }
    check(host::event_wait(done, 2000000000), "guest resumed after wake");
    host::thread_join_release(thread->host_handle);
    thread->host_handle = nullptr;
    check(woke && thread->exited && thread->is_signaled(), "ExitThread publishes thread signal");
    host::event_release(thread->wake_event); thread->wake_event = nullptr;
    host::event_release(ready); host::event_release(done);
    host::aligned_release(thread->ctx); thread->ctx = nullptr;

    // Threads that ended and whose handle was closed give their TLS slot back (issue #2: the 16 MiB TLS region
    // ran out after ~32768 short-lived threads). Far more threads than the region holds, a few alive at a time.
    const auto threads_before = kern::all_threads().size();
    u64 first_tls = 0;
    for (int i = 0; i < 40000; i++) {
        auto t = kern::create_thread(2, 0x1234, 0x400010000full, 44, 0);
        t->handle = kern::g_handles.add(t);
        if (!first_tls) first_tls = t->tls;
        check(t->tls >= kern::mem::kTlsBase && t->tls < kern::mem::kTlsBase + 0x1000, "TLS slot reused");
        kern::start_thread(t.get());
        while (!t->exited) host::thread_yield();
        kern::g_handles.close(t->handle);  // the game's CloseHandle after joining the thread
    }
    check(kern::all_threads().size() <= threads_before + 1, "ended threads released");
    check(first_tls != 0, "short-lived threads ran");
    std::puts("host kernel thread/lock/TLS checks passed");
}
