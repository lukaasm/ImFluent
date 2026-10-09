#pragma once

// Win32 backdrop hooks: wallpaper (IDesktopWallpaper, WIC), window and monitor rects (HWND from PlatformHandleRaw).
// allow_native_backdrop: the Windows 11 system backdrop instead; needs a swapchain that presents alpha.
// Call after the platform backend init.

void ImFluent_ImplWin32_InstallBackdropHooks( bool allow_native_backdrop = false );
void ImFluent_ImplWin32_UninstallBackdropHooks();
