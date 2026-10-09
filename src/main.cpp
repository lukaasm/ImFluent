#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include "imfluent.h"

#include <SDL3/SDL.h>

#include <stdlib.h>
#include <string.h>

#if defined( IMFLUENT_GALLERY_AUTOMATION )
#include "automation/imfluent_automation.h"
#endif

// The gallery renders into an offscreen "scene" texture and blits it to the swapchain.
// Keeping the last frame in a texture we own lets automation read pixels back (screenshots).
struct GalleryRenderer
{
    SDL_GPUDevice * Device         = nullptr;
    SDL_Window * Window            = nullptr;
    SDL_GPUTexture * Scene         = nullptr;
    SDL_GPUTextureFormat Format    = SDL_GPU_TEXTUREFORMAT_INVALID;
    Uint32 SceneW                  = 0;
    Uint32 SceneH                  = 0;
    SDL_GPUPresentMode PresentMode = SDL_GPU_PRESENTMODE_VSYNC;

    void EnsureScene( Uint32 w, Uint32 h )
    {
        if ( Scene && SceneW == w && SceneH == h )
            return;
        if ( Scene )
            SDL_ReleaseGPUTexture( Device, Scene );
        SDL_GPUTextureCreateInfo info = {};
        info.type                     = SDL_GPU_TEXTURETYPE_2D;
        info.format                   = Format;
        info.usage                    = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width                    = w;
        info.height                   = h;
        info.layer_count_or_depth     = 1;
        info.num_levels               = 1;
        info.sample_count             = SDL_GPU_SAMPLECOUNT_1;
        Scene                         = SDL_CreateGPUTexture( Device, &info );
        SceneW                        = w;
        SceneH                        = h;
    }

    // Reads back a rectangle of the scene texture as RGBA8.
    bool Capture( int x, int y, int w, int h, unsigned int * out_rgba )
    {
        if ( !Scene || w <= 0 || h <= 0 || x < 0 || y < 0 || ( Uint32 )( x + w ) > SceneW || ( Uint32 )( y + h ) > SceneH )
            return false;
        SDL_GPUTransferBufferCreateInfo tb_info = {};
        tb_info.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        tb_info.size                            = ( Uint32 )( w * h * 4 );
        SDL_GPUTransferBuffer * tb              = SDL_CreateGPUTransferBuffer( Device, &tb_info );
        if ( !tb )
            return false;

        SDL_GPUCommandBuffer * cmd     = SDL_AcquireGPUCommandBuffer( Device );
        SDL_GPUCopyPass * copy         = SDL_BeginGPUCopyPass( cmd );
        SDL_GPUTextureRegion src       = {};
        src.texture                    = Scene;
        src.x                          = ( Uint32 )x;
        src.y                          = ( Uint32 )y;
        src.w                          = ( Uint32 )w;
        src.h                          = ( Uint32 )h;
        src.d                          = 1;
        SDL_GPUTextureTransferInfo dst = {};
        dst.transfer_buffer            = tb;
        dst.pixels_per_row             = ( Uint32 )w;
        dst.rows_per_layer             = ( Uint32 )h;
        SDL_DownloadFromGPUTexture( copy, &src, &dst );
        SDL_EndGPUCopyPass( copy );
        SDL_GPUFence * fence = SDL_SubmitGPUCommandBufferAndAcquireFence( cmd );
        if ( !fence )
        {
            SDL_ReleaseGPUTransferBuffer( Device, tb );
            return false;
        }
        SDL_WaitForGPUFences( Device, true, &fence, 1 );
        SDL_ReleaseGPUFence( Device, fence );

        const Uint8 * src_px = ( const Uint8 * )SDL_MapGPUTransferBuffer( Device, tb, false );
        const bool bgra      = ( Format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || Format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB );
        for ( int i = 0; i < w * h; ++i )
        {
            const Uint8 * p = src_px + i * 4;
            const Uint8 r   = bgra ? p[ 2 ] : p[ 0 ];
            const Uint8 b   = bgra ? p[ 0 ] : p[ 2 ];
            out_rgba[ i ]   = ( unsigned int )r | ( ( unsigned int )p[ 1 ] << 8 ) | ( ( unsigned int )b << 16 ) | 0xFF000000u;
        }
        SDL_UnmapGPUTransferBuffer( Device, tb );
        SDL_ReleaseGPUTransferBuffer( Device, tb );
        return true;
    }
};

static bool HasArg( int argc, char ** argv, const char * name )
{
    for ( int i = 1; i < argc; ++i )
        if ( strcmp( argv[ i ], name ) == 0 )
            return true;
    return false;
}

static const char * ArgValue( int argc, char ** argv, const char * name, const char * fallback )
{
    for ( int i = 1; i + 1 < argc; ++i )
        if ( strcmp( argv[ i ], name ) == 0 )
            return argv[ i + 1 ];
    return fallback;
}

int main( int argc, char ** argv )
{
    if ( !SDL_Init( SDL_INIT_VIDEO | SDL_INIT_GAMEPAD ) )
        return 1;

    float main_scale = SDL_GetDisplayContentScale( SDL_GetPrimaryDisplay() );
    if ( main_scale <= 0.0f )
        main_scale = 1.0f;

    const int window_w = atoi( ArgValue( argc, argv, "--width", "1280" ) );
    const int window_h = atoi( ArgValue( argc, argv, "--height", "800" ) );

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window * window          = SDL_CreateWindow( "ImFluent Gallery", ( int )( window_w * main_scale ), ( int )( window_h * main_scale ), window_flags );
    if ( window == nullptr )
        return 1;

    SDL_SetWindowPosition( window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED );
    SDL_ShowWindow( window );

    SDL_GPUDevice * gpu_device = SDL_CreateGPUDevice( SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr );
    if ( gpu_device == nullptr )
        return 1;

    if ( !SDL_ClaimWindowForGPUDevice( gpu_device, window ) )
        return 1;

    SDL_SetGPUSwapchainParameters( gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC );

    GalleryRenderer renderer;
    renderer.Device = gpu_device;
    renderer.Window = window;
    renderer.Format = SDL_GetGPUSwapchainTextureFormat( gpu_device, window );

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGuiStyle & style = ImGui::GetStyle();
    style.ScaleAllSizes( main_scale );
    style.FontScaleDpi = main_scale;

    ImGui_ImplSDL3_InitForSDLGPU( window );
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device                     = gpu_device;
    init_info.ColorTargetFormat          = renderer.Format;
    init_info.MSAASamples                = SDL_GPU_SAMPLECOUNT_1;
    init_info.SwapchainComposition       = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    init_info.PresentMode                = SDL_GPU_PRESENTMODE_VSYNC;
    ImGui_ImplSDLGPU3_Init( &init_info );

    ImFluent::LoadFluentSystemFonts();
    ImFluent::SetThemePreset( HasArg( argc, argv, "--light" ) ? ImFluentThemePreset_Light : ImFluentThemePreset_Dark );
    if ( const char * page = ArgValue( argc, argv, "--page", nullptr ) )
        ImFluent::SetDemoWindowPage( page );

#if defined( IMFLUENT_GALLERY_AUTOMATION )
    // --mcp-port N (default 7801, 0 disables), --run-tests [filter], --junit path.xml, --te-ui, --normal-speed
    ImFluentAutomationConfig auto_config;
    auto_config.McpPort      = atoi( ArgValue( argc, argv, "--mcp-port", "7801" ) );
    auto_config.RunTests     = HasArg( argc, argv, "--run-tests" );
    auto_config.TestFilter   = ArgValue( argc, argv, "--run-tests", nullptr );
    auto_config.ExportJUnit  = ArgValue( argc, argv, "--junit", nullptr );
    auto_config.ShowEngineUi = HasArg( argc, argv, "--te-ui" );
    auto_config.NormalSpeed  = HasArg( argc, argv, "--normal-speed" );
    if ( auto_config.TestFilter && auto_config.TestFilter[ 0 ] == '-' && auto_config.TestFilter[ 1 ] == '-' )
        auto_config.TestFilter = nullptr; // "--run-tests" followed by another option
    if ( auto_config.RunTests )
        auto_config.McpPort = atoi( ArgValue( argc, argv, "--mcp-port", "0" ) );

    ImFluentAutomationHost auto_host;
    auto_host.UserData           = &renderer;
    auto_host.CaptureFramebuffer = []( int x, int y, int w, int h, unsigned int * out_rgba, void * user_data ) -> bool
    {
        return ( ( GalleryRenderer * )user_data )->Capture( x, y, w, h, out_rgba );
    };
    auto_host.SetWindowSize = []( int w, int h, void * user_data )
    {
        SDL_SetWindowSize( ( ( GalleryRenderer * )user_data )->Window, w, h );
    };
    ImFluentAutomation::Init( auto_config, auto_host );
#endif

    int exit_code = 0;
    bool done     = false;
    while ( !done )
    {
        SDL_Event event;
        while ( SDL_PollEvent( &event ) )
        {
            ImGui_ImplSDL3_ProcessEvent( &event );
            if ( event.type == SDL_EVENT_QUIT )
                done = true;
            if ( event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID( window ) )
                done = true;
        }

#if defined( IMFLUENT_GALLERY_AUTOMATION )
        if ( ImFluentAutomation::WantsExit( &exit_code ) )
            done = true;

        // Skip vsync while the Test Engine runs in fast mode.
        const SDL_GPUPresentMode want_mode = ( ImFluentAutomation::WantsMaxSpeed() && SDL_WindowSupportsGPUPresentMode( gpu_device, window, SDL_GPU_PRESENTMODE_IMMEDIATE ) ) ? SDL_GPU_PRESENTMODE_IMMEDIATE : SDL_GPU_PRESENTMODE_VSYNC;
        if ( want_mode != renderer.PresentMode )
        {
            SDL_SetGPUSwapchainParameters( gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, want_mode );
            renderer.PresentMode = want_mode;
        }
#endif

        if ( SDL_GetWindowFlags( window ) & SDL_WINDOW_MINIMIZED )
        {
            SDL_Delay( 10 );
            continue;
        }

        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImFluent::ShowDemoWindow( nullptr );
#if defined( IMFLUENT_GALLERY_AUTOMATION )
        ImFluentAutomation::ShowWindows();
#endif

        ImGui::Render();

        ImDrawData * draw_data  = ImGui::GetDrawData();
        const bool is_minimized = ( draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f );

        SDL_GPUCommandBuffer * command_buffer = SDL_AcquireGPUCommandBuffer( gpu_device );
        if ( !command_buffer )
            continue;

        SDL_GPUTexture * swapchain_texture = nullptr;
        Uint32 swapchain_w = 0, swapchain_h = 0;
        SDL_WaitAndAcquireGPUSwapchainTexture( command_buffer, window, &swapchain_texture, &swapchain_w, &swapchain_h );

        if ( swapchain_texture != nullptr && !is_minimized )
        {
            renderer.EnsureScene( swapchain_w, swapchain_h );
            ImGui_ImplSDLGPU3_PrepareDrawData( draw_data, command_buffer );

            SDL_GPUColorTargetInfo target_info = {};
            target_info.texture                = renderer.Scene;
            target_info.clear_color            = SDL_FColor{ 0.f, 0.f, 0.f, 1.f };
            target_info.load_op                = SDL_GPU_LOADOP_CLEAR;
            target_info.store_op               = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass * render_pass    = SDL_BeginGPURenderPass( command_buffer, &target_info, 1, nullptr );
            ImGui_ImplSDLGPU3_RenderDrawData( draw_data, command_buffer, render_pass );
            SDL_EndGPURenderPass( render_pass );

            SDL_GPUBlitInfo blit     = {};
            blit.source.texture      = renderer.Scene;
            blit.source.w            = swapchain_w;
            blit.source.h            = swapchain_h;
            blit.destination.texture = swapchain_texture;
            blit.destination.w       = swapchain_w;
            blit.destination.h       = swapchain_h;
            blit.load_op             = SDL_GPU_LOADOP_DONT_CARE;
            blit.filter              = SDL_GPU_FILTER_NEAREST;
            SDL_BlitGPUTexture( command_buffer, &blit );
        }

        SDL_SubmitGPUCommandBuffer( command_buffer );

#if defined( IMFLUENT_GALLERY_AUTOMATION )
        ImFluentAutomation::PostSwap();
#endif
    }

#if defined( IMFLUENT_GALLERY_AUTOMATION )
    ImFluentAutomation::Stop();
#endif

    SDL_WaitForGPUIdle( gpu_device );
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();

#if defined( IMFLUENT_GALLERY_AUTOMATION )
    ImFluentAutomation::Shutdown();
#endif

    if ( renderer.Scene )
        SDL_ReleaseGPUTexture( gpu_device, renderer.Scene );
    SDL_ReleaseWindowFromGPUDevice( gpu_device, window );
    SDL_DestroyGPUDevice( gpu_device );
    SDL_DestroyWindow( window );
    SDL_Quit();

    return exit_code;
}
