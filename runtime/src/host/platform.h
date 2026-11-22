// Narrow host services used by the guest memory manager and CPU contexts.
// No guest data, renderer API, or OS handle types cross this boundary.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace host {
void* aligned_allocate(std::size_t size, std::size_t alignment);
void aligned_release(void* allocation);
// Reservations never replace existing mappings. A non-null address must match exactly.
// write_watch requests the Windows optimization; Linux uses the renderer's hash fallback.
void* memory_reserve(void* address, std::size_t size, bool write_watch = false);
bool memory_commit(void* address, std::size_t size);
bool memory_decommit(void* address, std::size_t size);
bool memory_release(void* address, std::size_t size);
unsigned long memory_error();
// Pages written in [address, address + size) since the last call, reset as they are reported (Windows
// GetWriteWatch with WRITE_WATCH_FLAG_RESET; Linux userfaultfd async write-protect + PAGEMAP_SCAN, kernel 6.7+).
// `count` holds the capacity of `pages` and receives the number reported. False when the range is not inside
// one write-watched reservation or the host cannot track writes: callers then re-hash instead.
bool write_watch_reset(void* address, std::size_t size, void** pages, std::size_t* count);
// Calls of write_watch_reset so far and the nanoseconds they took (Linux; diagnostics).
void write_watch_totals(std::uint64_t* calls, std::uint64_t* ns);
std::size_t memory_readable(void* address, std::size_t size);

// Auto-reset events retain one pending wake (including a wake before a wait).
void* event_create();
void event_release(void* event);
void event_signal(void* event);
bool event_wait(void* event, std::int64_t timeout_ns = -1);
// Create dormant: the callback cannot run until thread_start. Handles are opaque;
// Windows keeps real HANDLE values so existing Windows-only diagnostics still work.
using ThreadEntry = void (*)(void*);
void* thread_create(ThreadEntry entry, void* argument, std::size_t stack_size);
bool thread_start(void* thread);
void thread_join_release(void* thread);
[[noreturn]] void thread_exit();
// Runtime workers live until process termination; do not run global destructors concurrently.
[[noreturn]] void process_exit(int code);
std::uint32_t thread_id();
// The OS id (thread_id() inside it) of a started thread_create thread; 0 before it runs.
std::uint32_t thread_os_id(void* thread);
void thread_yield();
std::uint64_t monotonic_ns();

// ---- Stack capture of another thread (stall reports, HWDER_THREAD_DUMP; host/stack.cpp)
// The thread's host call stack, innermost first, as text: "name+0xoff < name+0xoff < ...". Recompiled guest
// functions appear as f_<address>; frames without an exported name as <module>+0x<offset in the file>, for
// addr2line. Windows: SuspendThread + RtlVirtualUnwind. Linux: a signal stops the thread, which runs backtrace()
// in the handler and waits there while its stack is also scanned for return addresses into the executable.
// Empty when the thread could not be stopped. `thread` is a thread_create handle.
std::string thread_stack_text(void* thread);
// Lets the calling (runtime) thread be listed by diag_thread_stacks under `name`.
void diag_thread_register(const char* name);
// {name, stack} of every registered runtime thread.
std::vector<std::pair<std::string, std::string>> diag_thread_stacks();
}  // namespace host
