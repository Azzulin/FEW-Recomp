// Scripted input for unattended runs (HWDER_INPUT_SCRIPT=<file>): presses buttons and moves sticks on player 1
// at given times, on top of whatever the host controllers do. Game-independent, unlike HWDER_AUTOPLAY's HWDE menus.
//
// One event per line, '#' starts a comment:
//   <start_ms> <dur_ms> <BUTTON[+BUTTON...]>... [lx=N] [ly=N] [rx=N] [ry=N] [every=MS] [until=MS]
// Times count from the first input poll. Buttons: A B X Y L R ZL ZR PLUS MINUS UP DOWN LEFT RIGHT LS RS; '-' for
// none. Sticks are -32767..32767 (+y = up). With every=MS the event repeats with that period (active for dur_ms
// of each period) until until=MS (default: forever).
#pragma once
#include "hid/host_input.h"

namespace hid {

// Applies the script at time t_ms (from the first call) to `in`. Returns false when no script is set.
bool apply_input_script(u64 t_ms, HostInput& in);

}  // namespace hid
