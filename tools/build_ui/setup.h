// FEW Recomp Setup (Windows): the one window a player needs. Pick your own game files (base, update 1.5.0, DLCs,
// prod.keys), build the game with gerar-few.ps1, play. setup.cpp; the look and the progress view come from ui.h.
#pragma once
#include <SDL3/SDL.h>
#include <string>

// The kit folder (the one holding gerar-few.ps1: next to the exe, or up to three folders above it) must exist.
bool setup_init(SDL_Window* window, std::string& error);
// Draws one frame. close_requested: the window's close button; the setup asks first while a build runs.
// Returns false when the program should end.
bool setup_frame(SDL_Window* window, float scale, bool close_requested);
// Stops a build still running (the window is closing).
void setup_shutdown();
// A build is running (tests: FEW_SETUP_AUTOBUILD=1 starts one at once, and a screenshot waits for its end).
bool setup_busy();
