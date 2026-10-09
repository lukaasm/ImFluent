#pragma once

// Win32 backdrop hooks (wallpaper, window and monitor rects). Call after the platform backend init.
// allow_native_backdrop: use the Windows 11 system backdrop instead; needs a swapchain that presents alpha.

void ImFluent_ImplWin32_InstallBackdropHooks( bool allow_native_backdrop = false );
void ImFluent_ImplWin32_UninstallBackdropHooks();
