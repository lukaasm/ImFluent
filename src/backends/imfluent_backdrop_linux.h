#pragma once

// Backdrop GetWallpaper for Linux desktops: GNOME, Ubuntu, Budgie, Cinnamon and MATE (gsettings), KDE Plasma
// (plasma-org.kde.plasma.desktop-appletsrc) and XFCE (xfconf-query). Elsewhere the backdrop shows its fallback colour.
// `decode`: the app's image decoder (gdk-pixbuf also reads the JPEG XL wallpapers of recent GNOME); NULL uses stb_image
// when built with IMFLUENT_BACKDROP_STB_IMAGE and stb_image.h on the include path.
// Window rects: also install ImFluent_ImplSDL3_InstallBackdropScreenRects(), or set GetScreenRects yourself.

// Decodes an image file to RGBA8 in a malloc() buffer that the caller releases with free().
typedef bool ( *ImFluentDecodeImageFn )( const char * path, unsigned char ** pixels, int * width, int * height );

void ImFluent_ImplLinux_InstallBackdropHooks( ImFluentDecodeImageFn decode = 0 );
void ImFluent_ImplLinux_UninstallBackdropHooks();
