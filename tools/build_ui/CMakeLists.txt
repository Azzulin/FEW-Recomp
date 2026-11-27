# FEW Recomp's window (main.cpp): the Windows setup and the progress view of a build;, read from the events file that the build
# scripts write (tools/build_events.py). SDL3 + Dear ImGui on SDL's own renderer; no game code. Built by
# `tools/build.py --build-ui` with the same compilers as the game, while the game's code is being translated.
#   Windows: SDL from third_party/SDL, static, with video and the renderer (the game's own SDL has neither).
#   Linux:   the prebuilt static SDL3 the Linux build already downloads (HWDER_LINUX_DEPS), which loads X11 or
#            Wayland itself at run time.
cmake_minimum_required(VERSION 3.16)
project(few_build_ui C CXX)
set(CMAKE_CXX_STANDARD 17)  # path::u8string() as std::string; nothing here needs C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
set(TP ${ROOT}/third_party)
set(IMGUI ${TP}/imgui)

set(HWDER_LINUX_DEPS "" CACHE PATH "Linux: prebuilt static SDL3 (the same as the game's)")
if(HWDER_LINUX_DEPS)
    find_package(SDL3 CONFIG REQUIRED PATHS ${HWDER_LINUX_DEPS} NO_DEFAULT_PATH)
else()
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_LIBC ON CACHE BOOL "" FORCE)
    foreach(sub AUDIO GPU CAMERA HAPTIC SENSOR POWER VULKAN RENDER_VULKAN RENDER_GPU OPENGLES)
        set(SDL_${sub} OFF CACHE BOOL "" FORCE)
    endforeach()
    foreach(sub VIDEO RENDER JOYSTICK HIDAPI DIALOG)  # controllers move through the window (ImGui navigation)
        set(SDL_${sub} ON CACHE BOOL "" FORCE)
    endforeach()
    add_subdirectory(${TP}/SDL ${CMAKE_BINARY_DIR}/SDL EXCLUDE_FROM_ALL)
endif()

# The font, Roboto Medium (Apache-2.0, shipped with Dear ImGui), as a C array: written at configure time, rewritten
# only when it changes so the window is not rebuilt every time.
file(READ ${IMGUI}/misc/fonts/Roboto-Medium.ttf FONT_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," FONT_BYTES "${FONT_HEX}")
file(WRITE ${CMAKE_BINARY_DIR}/font_data.h.new "static const unsigned char kFontData[] = {${FONT_BYTES}};\n")
configure_file(${CMAKE_BINARY_DIR}/font_data.h.new ${CMAKE_BINARY_DIR}/font_data.h COPYONLY)

set(SOURCES main.cpp ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_sdl3.cpp ${IMGUI}/backends/imgui_impl_sdlrenderer3.cpp)
if(WIN32)
    enable_language(RC)
    # The setup (setup.cpp): with no --events argument the window is FEW Recomp Setup. The release publishes this
    # program as "FEW Recomp Setup.exe" (.github/workflows/release.yml).
    # Icon, manifest (per-monitor DPI, UTF-8, no administrator) and version information: an exe without them looks
    # like a dropper to antivirus heuristics (HWDER's setup was flagged, docs/antivirus.md).
    list(APPEND SOURCES setup.cpp few_build_ui.rc)
    set_source_files_properties(few_build_ui.rc PROPERTIES OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/few_build_ui.manifest;${CMAKE_CURRENT_SOURCE_DIR}/few.ico")
    add_executable(few_build_ui WIN32 ${SOURCES})
    target_link_libraries(few_build_ui PRIVATE ole32 uuid shell32)
    target_compile_definitions(few_build_ui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
else()
    add_executable(few_build_ui ${SOURCES})
endif()
target_include_directories(few_build_ui PRIVATE ${IMGUI} ${IMGUI}/backends ${CMAKE_BINARY_DIR})
target_compile_definitions(few_build_ui PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(few_build_ui PRIVATE SDL3::SDL3-static)
if(MINGW)
    # -s: no symbols; no link timestamp: the same sources give the same file, so a release's Setup keeps any
    # antivirus reputation the previous identical one earned.
    target_link_options(few_build_ui PRIVATE -static -s -Wl,--no-insert-timestamp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(few_build_ui PRIVATE -s)
endif()
set_target_properties(few_build_ui PROPERTIES OUTPUT_NAME "few-build-ui")
