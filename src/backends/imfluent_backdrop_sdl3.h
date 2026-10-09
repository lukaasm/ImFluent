#pragma once

// Backdrop GetScreenRects for any SDL3 window (imgui_impl_sdl3 stores the SDL_WindowID in PlatformHandle).
// Wayland clients cannot read their position: there the wallpaper maps to the window instead of the monitor.

void ImFluent_ImplSDL3_InstallBackdropScreenRects();
