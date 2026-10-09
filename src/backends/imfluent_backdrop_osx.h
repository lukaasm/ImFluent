#pragma once

// macOS backdrop hooks: wallpaper of the window's screen (NSWorkspace, decoded and downscaled by ImageIO), window and
// screen rects (NSWindow from PlatformHandleRaw). allow_native_backdrop: an NSVisualEffectView behind the content
// instead; the renderer must present transparent pixels (a non-opaque CAMetalLayer, SDL_WINDOW_TRANSPARENT).
// Link AppKit, ImageIO and CoreGraphics. Works with or without ARC.

void ImFluent_ImplOSX_InstallBackdropHooks( bool allow_native_backdrop = false );
void ImFluent_ImplOSX_UninstallBackdropHooks();
