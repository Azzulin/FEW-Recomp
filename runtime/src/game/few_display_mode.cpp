// Fire Emblem Warriors: 60 fps battles through the game's own Performance mode ("60 fps na batalha").
//
// How the game picks its frame rate (main 1.5.0, read from the code; names in tools/recomp/symbols.txt and the
// details in docs/few.md "Modos de exibição"):
//   Display_SetMode (sys, mode) stores mode 0..5 at sys+0x9cb4 (and a configuration id at sys+0x44). The mode is
//     chosen from the console's operation mode and the Performance option: handheld 2, docked 0 (quality) or 1
//     (performance); 3..5 are the same three plus 3 (another layout the game switches to).
//   Display_GetSize: modes 0, 1, 3, 4 output 1920x1080; 2 and 5 output 1280x720.
//   Display_GetPresentInterval: modes 1 and 4 always present every vsync (60 Hz). The others present every second
//     vsync (30 Hz) as soon as the scene has content (a battle), 60 Hz otherwise (menus).
//   Every frame the game stores the interval and 60 / interval (its frame rate) in two globals that the logic reads,
//     so a 60 Hz mode does not run the game fast. Display_IsQualityMode (modes 0, 2, 3, 5) also tunes the dynamic
//     resolution controller (DynRes_Update averages 30 frames instead of 60) and Display_ModelDrawDistance gives
//     characters 2000 units of draw distance instead of 1500.
// The runtime reports a docked console (service/applet.cpp), so a new save runs in mode 0: 30 fps battles. With the
// option on (the default), this hook turns quality modes 0 and 3 into their performance twins 1 and 4, which is what
// choosing Performance in the game's options does. The game resets mode 1 to 0 by itself while the byte at
// sys+0xa6b2 is set (real-time cutscenes, at 30 Hz); the hook leaves those calls alone.
// Display_SetMode also stores a configuration id at sys+0x44 (mode 0: 0, 1: 3, 2: 5, 3: 7, 4: 8, 5: 9) that picks the
// 3D render size; with fps60_1080p the hook gives Performance mode the Quality id (1920x1080 instead of 1280x720),
// and with fps30_720p (the Steam Deck's default) it gives Quality mode the Performance id: 30 fps at 1280x720, the
// height of the Deck's screen, with everything else as in Quality mode (effects, character draw distance).
#include "game/guest.h"
#include "game/hook.h"
#include "gpu/settings.h"
#include "hwder/runtime.h"

// gen_main leaves a symbol out when it cannot find its code in the user's release (tools/recomp/symbol_sigs.py,
// GitHub issue #1): the hook is then not built and the game keeps its own behaviour.
#ifdef HW_ADDR_Display_SetMode
HW_HOOK(Display_SetMode) {
    const u64 sys = c->x[0];
    const u32 mode = (u32)c->x[1];
    u32 to = mode;
    if (settings::video().battle_60fps && (mode == 0 || mode == 3) && game::read<u8>(sys + 0xa6b2) == 0) to = mode + 1;
    // The game calls this every frame (and twice per frame in real-time cutscenes, where it drops back to mode 0 at
    // 30 Hz): log only the first few distinct calls.
    static u32 last = ~0u, logged = 0;
    if (((mode << 8) | to) != last && logged < 4) {
        last = (mode << 8) | to;
        logged++;
        if (to != mode) hw_log("few: display mode %u -> %u (60 fps battles, the game's Performance mode)", mode, to);
        else hw_log("few: display mode %u", mode);
    }
    c->x[1] = to;
    HW_ORIG(Display_SetMode)(c);
    // Performance mode renders its 3D at configuration 3 (1280x720, down to 1120x630); configuration 0 is Quality's
    // 1920x1080. The configuration id only picks render target sizes (DynRes_Update and its tables), so 60 Hz keeps
    // the Quality picture with it.
    if (to != mode && settings::video().full_res_60) game::write<u32>(sys + 0x44, mode == 0 ? 0u : 7u);
    if (to == mode && (mode == 0 || mode == 3) && settings::video().low_res_30)
        game::write<u32>(sys + 0x44, mode == 0 ? 3u : 8u);
}
#endif

// The dynamic resolution controller steps the configuration id within its mode's group (0-2, 3-4, 5-6; 7-9 fixed) from
// the GPU time it reads in its timestamp reports. Here those report when the command thread reaches them, not when the
// GPU did the work, so the "GPU time" includes command-thread latency and the picture drops to the lowest step while
// the GPU idles. Keep each group's first (largest) configuration, or with res_630p the Performance group's lower
// step (id 4: 1120x630 instead of 1280x720; docs/few.md "Resolução dinâmica").
#ifdef HW_ADDR_DynRes_Update
HW_HOOK(DynRes_Update) {
    static const u32 kGroupTop[10] = {0, 0, 0, 3, 3, 5, 5, 7, 8, 9};
    const u32 id = (u32)c->x[0];
    u32 to = id < 10 ? kGroupTop[id] : id;
    if (to == 3 && settings::video().res_630p) to = 4;
    static u32 last = ~0u;
    if (to != last && (to == 3 || to == 4)) {
        last = to;
        hw_log("few: 3D resolution configuration %u (%s)", to, to == 4 ? "1120x630" : "1280x720");
    }
    c->x[0] = to;
}
#endif
