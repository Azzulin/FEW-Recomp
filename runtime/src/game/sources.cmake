# Runtime overrides of game functions (tools/recomp/symbols.txt `hook` entries). Paths relative to the repo root.
# Fire Emblem Warriors' own hooks (their symbols are in tools/recomp/symbols.txt):
list(APPEND RUNTIME_SOURCES
    runtime/src/game/few_display_mode.cpp
)
# HWDER's Hyrule Warriors hooks build only with HW_GAME_HOOKS=ON (CMakeLists.txt), once the game's functions they
# need are named in tools/recomp/symbols.txt.
if(HW_GAME_HOOKS)
    list(APPEND RUNTIME_SOURCES
        runtime/src/game/stage_props.cpp
        runtime/src/game/widescreen.cpp
    )
endif()
