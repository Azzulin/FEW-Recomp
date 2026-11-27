// Identity of the game this runtime is built for. Every place that needs the title ID, the save folder
// or the game's name reads it from here (the runtime started as HWDER, built for Hyrule Warriors: DE).
//
// Target: Fire Emblem Warriors, US release: base game v0 (RomFS) + update 1.5.0 (v327680, its ExeFS is the code
// that is recompiled, SDK 3.7.0) + add-on content. Facts measured on the dump are in docs/few.md.
#pragma once

// Release: US by default. The Japanese release (also the one sold in Asia; GitHub issue #1) has the same update
// 1.5.0 (v327680) under its own IDs; tools/recomp/gen_main.py sees it in data/update/update.json and CMake then
// defines HW_GAME_REGION_JP for the runtime only (tools/game_profile.py REGIONS). Not yet tested on a Japanese dump.
#if defined(HW_GAME_REGION_JP)
#define HW_GAME_TITLE_ID 0x0100EAE003A5A000ull
#define HW_GAME_TITLE_ID_STR "0100EAE003A5A000"
#define HW_GAME_REGION "JP"
#else
// Application (program) ID of the program NCA. The update is this | 0x800; add-on content is this + 0x1000 + n.
#define HW_GAME_TITLE_ID 0x0100F15003E64000ull
#define HW_GAME_TITLE_ID_STR "0100F15003E64000"
#define HW_GAME_REGION "US"
#endif
#define HW_GAME_NAME "Fire Emblem Warriors"
// The update the generated code is built from (CNMT version; display version 1.5.0).
#define HW_GAME_UPDATE_VERSION 327680u
#define HW_GAME_DISPLAY_VERSION "1.5.0"

// GetDesiredLanguage (and set's GetLanguageCode) answer. US: the NACP lists en-US, en-GB, fr, de, es-419, es, it,
// fr-CA; the base RomFS carries English voice and movies (nx/voice/eng, nx/movie/eng); Japanese voice is the add-on
// 0100F15003E65005. JP: the RomFS has only jpn folders (docs/few.md). HWDER_LANGUAGE overrides it (applet.cpp).
#if defined(HW_GAME_REGION_JP)
#define HW_GAME_LANGUAGE "ja"
#else
#define HW_GAME_LANGUAGE "en-US"
#endif

// Save files the in-game save picker imports. Not yet known for this game: these are HWDE's names and the
// picker only matters once a save exists. Confirm against what the game writes under save/<title>/<user>/.
#define HW_GAME_SAVE_MAIN "zmha.bin"
#define HW_GAME_SAVE_SIDE "zmhasd.bin"
