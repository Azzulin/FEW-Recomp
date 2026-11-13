// The game-drawn 120 fps mode and the widescreen camera hook, switched off.
//
// HWDER's interp_draw.cpp, camera.cpp and game/widescreen.cpp hook Hyrule Warriors functions by address
// (tools/recomp/symbols.txt). Until the matching Fire Emblem Warriors functions are found, the runtime is built with
// this file instead (CMake option HW_GAME_HOOKS=OFF): every query answers "mode off", which is the behaviour the
// presenter, nvdrv and vi already have when the player leaves 120 fps disabled.
#include <atomic>

#include "interp/camera.h"
#include "interp/interp.h"

extern "C" std::atomic<unsigned long long> hw_gi_submit_tag{0};

namespace interp {
bool game_interp() { return false; }
int snapshots(CamSnap*, int) { return 0; }
}  // namespace interp

extern "C" bool hw_game_interp_on() { return false; }
extern "C" u32 hw_game_interp_release_hz() { return 0; }
extern "C" void hw_gi_note_ipc(const char*, u32) {}
extern "C" void hw_widescreen_tick() {}
