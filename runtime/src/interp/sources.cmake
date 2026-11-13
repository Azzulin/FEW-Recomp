# Owned by the interp subsystem (high frame rate). Paths relative to the repo root.
# HW_GAME_HOOKS (CMakeLists.txt): the real module needs the game's named functions in tools/recomp/symbols.txt.
if(HW_GAME_HOOKS)
    list(APPEND RUNTIME_SOURCES
        runtime/src/interp/camera.cpp
        runtime/src/interp/interp_draw.cpp
    )
else()
    list(APPEND RUNTIME_SOURCES runtime/src/interp/interp_off.cpp)
endif()
