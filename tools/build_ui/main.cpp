// FEW Recomp's window, in two roles:
//
//   FEW Recomp Setup.exe                         Windows setup: pick your files, build, play (setup.cpp)
//   few-build-ui --events <file> [--pid <pid>]   the progress of a build that runs in a terminal (gerar-few.ps1,
//                                                build-linux.sh); it only reads the events file, so closing it never
//                                                stops the build (tools/build_events.py documents the file)
//
// SDL3 + Dear ImGui on SDL's renderer, usable with a mouse, a keyboard or a controller. The look (ui.h) is our own:
// no logo, art or text of the game is used. FEW_BUILD_UI_SCREENSHOT=<file.bmp> saves the window after a second and
// quits (tests, documentation).
#include "ui.h"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#ifdef _WIN32
#include "setup.h"
#endif
#include "font_data.h"  // Roboto Medium (Apache-2.0), from third_party/imgui/misc/fonts at configure time

namespace {

struct Viewer {
    Build build;
    EventsFile events;
    long pid = 0;
    double next_poll = 0, next_pid_check = 0;
    bool quit = false, focus = true;
    std::string message;
};

void poll(Viewer& v, SDL_Window* window) {
    double now = SDL_GetTicks() / 1000.0;
    if (now < v.next_poll) return;
    v.next_poll = now + 0.25;
    Outcome before = v.build.outcome;
    v.events.poll(v.build);
    if (v.build.outcome == Outcome::Running && v.build.started && now >= v.next_pid_check) {
        v.next_pid_check = now + 2.0;
        if (!process_alive(v.pid)) {
            v.events.poll(v.build);  // its last words, written just before it ended
            if (v.build.outcome == Outcome::Running) v.build.outcome = Outcome::Gone;
        }
    }
    if (v.build.outcome != before && v.build.outcome != Outcome::Running) {
        v.focus = true;
        SDL_FlashWindow(window, SDL_FLASH_UNTIL_FOCUSED);
    }
}

void draw_viewer(Viewer& v, float scale) {
    Build& b = v.build;
    const bool failed = b.outcome == Outcome::Failed || b.outcome == Outcome::Gone;
    std::string elapsed;
    if (b.started)
        elapsed = "Elapsed  " + duration(std::max(0.0, (b.outcome == Outcome::Running ? now_seconds() : b.last_t) - b.start_t));
    Frame f = begin_frame(scale, "Building the PC version from your own copy of the game", elapsed, failed);
    if (b.started) draw_steps(f, b);
    else text_at(f.dl, ImVec2(f.steps_a.x + 22 * scale, f.steps_a.y + 58 * scale), f.fs, kMuted, "Waiting for the build to start...");
    begin_main(f);
    if (b.outcome == Outcome::Done) {
        ImGui::PushFont(nullptr, f.fb * 1.6f);
        ImGui::TextColored(vec(kGoldLight), "Your game is ready");
        ImGui::PopFont();
        ImGui::TextColored(vec(kMuted), "Built in %s. It can now be started from here or from %s.",
                           duration(std::max(0.0, b.last_t - b.start_t)).c_str(),
#ifdef _WIN32
                           "few.exe in the game folder"
#else
                           "Steam (the build added it to your library)"
#endif
        );
        float by = ImGui::GetCursorScreenPos().y + 26 * scale;
        progress_bar(f.dl, ImVec2(f.cx, by), ImVec2(f.cx + f.cw, by + 34 * scale), 1.0, f.t, scale);
        ImGui::SetCursorScreenPos(ImVec2(f.cx, by + 70 * scale));
        // Three buttons that always fit the panel: Play the widest, then the folder, then Close.
        float gap = 16 * scale, bh = 56 * scale, room = f.cw - 2 * gap;
        float w_play = std::min(room * 0.36f, 260 * scale), w_open = std::min(room * 0.40f, 300 * scale);
        float w_close = std::min(room - w_play - w_open, 160 * scale);
        if (v.focus) { ImGui::SetKeyboardFocusHere(); v.focus = false; }
        if (gold_button("Play", ImVec2(w_play, bh), true)) {
            if (start_game(b.play, b.folder)) v.quit = true;
            else v.message = "The game could not be started from here: start it from " + b.play;
        }
        ImGui::SameLine(0, gap);
        if (gold_button("Open the game folder", ImVec2(w_open, bh), false)) open_path(b.folder);
        ImGui::SameLine(0, gap);
        if (gold_button("Close", ImVec2(w_close, bh), false)) v.quit = true;
        if (!v.message.empty()) ImGui::TextColored(vec(kCrimson), "%s", v.message.c_str());
    } else if (failed) {
        ImGui::PushFont(nullptr, f.fb * 1.6f);
        ImGui::TextColored(vec(kCrimson), b.outcome == Outcome::Gone ? "The build is no longer running" : "The build stopped");
        ImGui::PopFont();
        ImGui::Spacing();
        if (b.outcome == Outcome::Gone)
            ImGui::TextColored(vec(kIvory), "Its terminal window was closed, or it was stopped, during \"%s\".", b.current_label());
        else
            ImGui::TextColored(vec(kIvory), "%s", b.error.empty() ? "See the terminal window for the reason." : b.error.c_str());
        ImGui::Spacing();
        ImGui::TextColored(vec(kMuted), "Running it again continues where it stopped.");
        if (!b.log_path.empty()) ImGui::TextColored(vec(kMuted), "Everything it printed is in %s", b.log_path.c_str());
        else ImGui::TextColored(vec(kMuted), "Everything it printed is in its terminal window.");
        ImGui::Dummy(ImVec2(0, 20 * scale));
        if (v.focus) { ImGui::SetKeyboardFocusHere(); v.focus = false; }
        if (gold_button("Close", ImVec2(160 * scale, 52 * scale), true)) v.quit = true;
    } else {
        draw_progress(f, b, "Waiting...");
    }
    end_main(f);
    end_frame(f, b.outcome == Outcome::Running
                     ? "Closing this window does not stop the build. Every detail is in its terminal window."
                     : "You can close this window.");
}

}  // namespace

int main(int argc, char** argv) {
    Viewer viewer;
    for (int i = 1; i + 1 < argc; i++) {
        if (!strcmp(argv[i], "--events")) viewer.events.path = argv[++i];
        else if (!strcmp(argv[i], "--pid")) viewer.pid = strtol(argv[++i], nullptr, 10);
    }
    const bool setup = viewer.events.path.empty();
#ifndef _WIN32
    if (setup) {
        fprintf(stderr, "usage: few-build-ui --events <file> [--pid <build process>]\n");
        return 2;
    }
    unsetenv("LD_PRELOAD");  // Steam's overlay library, when started from a Steam session
#endif
    SDL_SetHint(SDL_HINT_APP_NAME, setup ? "FEW Recomp Setup" : "Fire Emblem Warriors build");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        fprintf(stderr, "few-build-ui: %s (the build goes on in its terminal)\n", SDL_GetError());
        return 3;
    }
    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    bool game_mode = getenv("GAMESCOPE_WAYLAND_DISPLAY") || (desktop && !SDL_strcasecmp(desktop, "gamescope"));
    // No high pixel density: on KDE with fractional scaling it put the drawing and the pointer in different places
    // (HWDER's setup window). The compositor scales the window; on X11 we grow by the display scale ourselves.
    SDL_Window* window = SDL_CreateWindow(setup ? "FEW Recomp Setup" : "Fire Emblem Warriors - PC build", 1120, 700,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | (game_mode ? SDL_WINDOW_FULLSCREEN : 0));
    float display_scale = window ? std::clamp(SDL_GetWindowDisplayScale(window), 1.0f, 3.0f) : 1.0f;
    SDL_Rect usable{};
    if (window && !game_mode && SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window), &usable)) {
        SDL_SetWindowSize(window, std::min(int(1120 * display_scale), int(usable.w * 0.95f)),
                          std::min(int(700 * display_scale), int(usable.h * 0.92f)));
        SDL_SetWindowMinimumSize(window, 720, 460);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        fprintf(stderr, "few-build-ui: %s (the build goes on in its terminal)\n", SDL_GetError());
        return 3;
    }
#ifdef _WIN32
    std::string setup_error;
    if (setup && !setup_init(window, setup_error)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FEW Recomp Setup", setup_error.c_str(), window);
        return 1;
    }
#endif
    SDL_SetRenderVSync(renderer, 1);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    // A layout slip must never put a developer message in front of a player: log it (stderr) and carry on.
    io.ConfigErrorRecoveryEnableTooltip = false;
    io.ConfigErrorRecoveryEnableAssert = false;
    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)kFontData, int(sizeof kFontData), 20.0f, &font_config);
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 20.0f;
    style.FrameRounding = 4;
    style.FramePadding = ImVec2(16, 10);
    style.ItemSpacing = ImVec2(12, 10);
    style.WindowPadding = ImVec2(0, 0);
    style.PopupRounding = 6;
    style.Colors[ImGuiCol_NavCursor] = vec(kGoldLight);
    style.Colors[ImGuiCol_PopupBg] = vec(rgba(14, 28, 62, 250));
    style.Colors[ImGuiCol_ModalWindowDimBg] = vec(rgba(4, 8, 20, 160));
    style.Colors[ImGuiCol_TitleBgActive] = vec(rgba(24, 44, 92));
    style.ScaleAllSizes(display_scale);
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    const char* screenshot = getenv("FEW_BUILD_UI_SCREENSHOT");
    int frames = 0;
    bool running = true;
    while (running) {
        bool close_requested = false;
        SDL_Event event;
        bool waited = SDL_WaitEventTimeout(&event, 33);
        while (waited || SDL_PollEvent(&event)) {
            waited = false;
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) close_requested = true;
        }
        if (!setup) poll(viewer, window);
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        float fit = std::min(w / (1120.0f * display_scale), h / (700.0f * display_scale));
        float scale = std::clamp(fit, 0.7f, 2.0f) * display_scale;
        style.FontScaleMain = scale;
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
#ifdef _WIN32
        if (setup) running = setup_frame(window, scale, close_requested);
        else
#endif
        {
            draw_viewer(viewer, scale);
            running = !viewer.quit && !close_requested;
        }
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 9, 18, 42, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        bool hold = false;  // a screenshot of the setup waits for a build it started to end
#ifdef _WIN32
        hold = setup && setup_busy();
#endif
        if (screenshot && !hold && ++frames == 60) {
            if (SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr)) {
                SDL_SaveBMP(shot, screenshot);
                SDL_DestroySurface(shot);
            }
            running = false;
        }
        SDL_RenderPresent(renderer);
    }
#ifdef _WIN32
    if (setup) setup_shutdown();
#endif
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
