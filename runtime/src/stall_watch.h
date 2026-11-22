// Stall reports: when the game stops queueing frames, log where every thread is (HWDER_STALL_DUMP).
#pragma once

namespace stall {
// vi QueueBuffer: the game handed over a frame.
void note_frame();
// Starts the watcher thread (once the game's main thread runs).
void start();
// Logs every guest thread (what it waits on and its host stack): stall reports and HWDER_THREAD_DUMP.
void dump_guest_threads();
}  // namespace stall
