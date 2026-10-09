#include "imfluent_backdrop_sdl3.h"

#include "../imfluent.h"

#include <imgui.h>

#include <SDL3/SDL.h>

#include <stdint.h>

namespace
{
    bool GetScreenRects( ImGuiViewport * vp, ImVec4 * client, ImVec4 * monitor, void * )
    {
        SDL_Window * window = vp ? SDL_GetWindowFromID( ( SDL_WindowID )( intptr_t )vp->PlatformHandle ) : NULL;
        if ( !window )
            return false;
        const char * driver = SDL_GetCurrentVideoDriver();
        if ( driver && SDL_strcmp( driver, "wayland" ) == 0 )
            return false;
        int x = 0, y = 0, w = 0, h = 0;
        SDL_Rect bounds;
        if ( !SDL_GetWindowPosition( window, &x, &y ) || !SDL_GetWindowSize( window, &w, &h ) || !SDL_GetDisplayBounds( SDL_GetDisplayForWindow( window ), &bounds ) )
            return false;
        *client  = ImVec4( ( float )x, ( float )y, ( float )( x + w ), ( float )( y + h ) );
        *monitor = ImVec4( ( float )bounds.x, ( float )bounds.y, ( float )( bounds.x + bounds.w ), ( float )( bounds.y + bounds.h ) );
        return true;
    }
} // namespace

void ImFluent_ImplSDL3_InstallBackdropScreenRects()
{
    ImFluent::GetBackdropHooks().GetScreenRects = &GetScreenRects;
}