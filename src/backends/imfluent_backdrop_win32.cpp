#include "imfluent_backdrop_win32.h"

#include "../imfluent.h"

#include <imgui.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#include <shobjidl.h>
#include <wincodec.h>

#include <string>
#include <vector>

#if defined( _MSC_VER )
#pragma comment( lib, "ole32.lib" )
#pragma comment( lib, "windowscodecs.lib" )
#pragma comment( lib, "dwmapi.lib" )
#endif

namespace
{
    struct Win32BackdropState
    {
        bool ComInitialized = false;

        std::vector< unsigned char > Pixels;
        int Width  = 0;
        int Height = 0;
        int ImageW = 0; // Before the downscale
        int ImageH = 0;
        std::wstring Path;
        FILETIME WriteTime                  = {};
        DESKTOP_WALLPAPER_POSITION Position = DWPOS_FILL;
        COLORREF Background                 = 0;
        RECT Monitor                        = {};
        unsigned int Serial                 = 0;
    };
    Win32BackdropState g_Win32Backdrop;

    const int kMaxDecodeSize = 512;

    template< typename T >
    void SafeRelease( T *& p )
    {
        if ( p )
            p->Release();
        p = nullptr;
    }

    HWND ViewportHwnd( ImGuiViewport * vp )
    {
        return vp ? ( HWND )vp->PlatformHandleRaw : nullptr;
    }

    bool DecodeImage( const std::wstring & path, Win32BackdropState & st )
    {
        IWICImagingFactory * factory    = nullptr;
        IWICBitmapDecoder * decoder     = nullptr;
        IWICBitmapFrameDecode * frame   = nullptr;
        IWICBitmapScaler * scaler       = nullptr;
        IWICFormatConverter * converter = nullptr;
        bool ok                         = false;
        if ( SUCCEEDED( CoCreateInstance( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &factory ) ) ) && SUCCEEDED( factory->CreateDecoderFromFilename( path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder ) ) && SUCCEEDED( decoder->GetFrame( 0, &frame ) ) )
        {
            UINT iw = 0, ih = 0;
            frame->GetSize( &iw, &ih );
            if ( iw > 0 && ih > 0 )
            {
                const double s = ( iw > ih ? iw : ih ) > ( UINT )kMaxDecodeSize ? ( double )kMaxDecodeSize / ( iw > ih ? iw : ih ) : 1.0;
                const UINT w   = ( UINT )( iw * s + 0.5 ) > 0 ? ( UINT )( iw * s + 0.5 ) : 1;
                const UINT h   = ( UINT )( ih * s + 0.5 ) > 0 ? ( UINT )( ih * s + 0.5 ) : 1;
                if ( SUCCEEDED( factory->CreateBitmapScaler( &scaler ) ) && SUCCEEDED( scaler->Initialize( frame, w, h, WICBitmapInterpolationModeFant ) ) && SUCCEEDED( factory->CreateFormatConverter( &converter ) ) && SUCCEEDED( converter->Initialize( scaler, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom ) ) )
                {
                    st.Pixels.resize( ( size_t )w * h * 4 );
                    if ( SUCCEEDED( converter->CopyPixels( nullptr, w * 4, ( UINT )st.Pixels.size(), st.Pixels.data() ) ) )
                    {
                        st.Width  = ( int )w;
                        st.Height = ( int )h;
                        st.ImageW = ( int )iw;
                        st.ImageH = ( int )ih;
                        ok        = true;
                    }
                }
            }
        }
        SafeRelease( converter );
        SafeRelease( scaler );
        SafeRelease( frame );
        SafeRelease( decoder );
        SafeRelease( factory );
        return ok;
    }

    // The current wallpaper, also for slideshows and Spotlight.
    std::wstring TranscodedWallpaperPath()
    {
        wchar_t appdata[ MAX_PATH ] = {};
        const DWORD n               = GetEnvironmentVariableW( L"APPDATA", appdata, MAX_PATH );
        if ( n == 0 || n >= MAX_PATH )
            return std::wstring();
        return std::wstring( appdata ) + L"\\Microsoft\\Windows\\Themes\\TranscodedWallpaper";
    }

    void QueryWallpaper( const RECT & monitor, std::wstring & path, DESKTOP_WALLPAPER_POSITION & position, COLORREF & background )
    {
        IDesktopWallpaper * dw = nullptr;
        if ( SUCCEEDED( CoCreateInstance( __uuidof( DesktopWallpaper ), nullptr, CLSCTX_ALL, IID_PPV_ARGS( &dw ) ) ) )
        {
            UINT count = 0;
            dw->GetMonitorDevicePathCount( &count );
            for ( UINT i = 0; i < count && path.empty(); ++i )
            {
                LPWSTR id = nullptr;
                RECT r    = {};
                if ( SUCCEEDED( dw->GetMonitorDevicePathAt( i, &id ) ) && id )
                {
                    if ( SUCCEEDED( dw->GetMonitorRECT( id, &r ) ) && EqualRect( &r, &monitor ) )
                    {
                        LPWSTR p = nullptr;
                        if ( SUCCEEDED( dw->GetWallpaper( id, &p ) ) && p )
                        {
                            path = p;
                            CoTaskMemFree( p );
                        }
                    }
                    CoTaskMemFree( id );
                }
            }
            dw->GetPosition( &position );
            dw->GetBackgroundColor( &background );
            dw->Release();
        }
        if ( path.empty() || GetFileAttributesW( path.c_str() ) == INVALID_FILE_ATTRIBUTES )
            path = TranscodedWallpaperPath();
    }

    void PlaceWallpaper( const Win32BackdropState & st, ImFluentWallpaper * out )
    {
        const double iw = st.ImageW, ih = st.ImageH;
        double mx = st.Monitor.left, my = st.Monitor.top;
        double mw = st.Monitor.right - st.Monitor.left, mh = st.Monitor.bottom - st.Monitor.top;
        double area_x = mx, area_y = my, area_w = mw, area_h = mh;
        if ( st.Position == DWPOS_SPAN )
        {
            area_x = GetSystemMetrics( SM_XVIRTUALSCREEN );
            area_y = GetSystemMetrics( SM_YVIRTUALSCREEN );
            area_w = GetSystemMetrics( SM_CXVIRTUALSCREEN );
            area_h = GetSystemMetrics( SM_CYVIRTUALSCREEN );
        }
        if ( st.Position == DWPOS_STRETCH )
        {
            out->UV0 = ImVec2( 0.f, 0.f );
            out->UV1 = ImVec2( 1.f, 1.f );
            return;
        }
        if ( st.Position == DWPOS_TILE )
        {
            out->UV0    = ImVec2( 0.f, 0.f );
            out->UV1    = ImVec2( ( float )( mw / iw ), ( float )( mh / ih ) );
            out->Repeat = true;
            return;
        }
        // Scale: fill and span cover the area, fit contains the image, center is 1:1.
        double s = 1.0;
        if ( st.Position == DWPOS_FILL || st.Position == DWPOS_SPAN )
            s = ( area_w / iw > area_h / ih ) ? area_w / iw : area_h / ih;
        else if ( st.Position == DWPOS_FIT )
            s = ( area_w / iw < area_h / ih ) ? area_w / iw : area_h / ih;
        const double ox = area_x + ( area_w - iw * s ) * 0.5;
        const double oy = area_y + ( area_h - ih * s ) * 0.5;
        out->UV0        = ImVec2( ( float )( ( mx - ox ) / ( iw * s ) ), ( float )( ( my - oy ) / ( ih * s ) ) );
        out->UV1        = ImVec2( ( float )( ( mx + mw - ox ) / ( iw * s ) ), ( float )( ( my + mh - oy ) / ( ih * s ) ) );
    }

    bool GetWallpaper( ImGuiViewport * vp, ImFluentWallpaper * out, void * )
    {
        HWND hwnd = ViewportHwnd( vp );
        if ( !hwnd )
            return false;
        MONITORINFO mi = { sizeof( mi ) };
        if ( !GetMonitorInfoW( MonitorFromWindow( hwnd, MONITOR_DEFAULTTONEAREST ), &mi ) )
            return false;

        Win32BackdropState & st = g_Win32Backdrop;
        std::wstring path;
        DESKTOP_WALLPAPER_POSITION position = DWPOS_FILL;
        COLORREF background                 = 0;
        QueryWallpaper( mi.rcMonitor, path, position, background );
        WIN32_FILE_ATTRIBUTE_DATA fa = {};
        const bool has_file          = !path.empty() && GetFileAttributesExW( path.c_str(), GetFileExInfoStandard, &fa );

        const bool same_file = has_file && path == st.Path && CompareFileTime( &fa.ftLastWriteTime, &st.WriteTime ) == 0 && !st.Pixels.empty();
        const bool same      = same_file && position == st.Position && background == st.Background && EqualRect( &mi.rcMonitor, &st.Monitor );
        if ( !same )
        {
            if ( !same_file )
            {
                st.Pixels.clear();
                st.Path      = path;
                st.WriteTime = fa.ftLastWriteTime;
                if ( !has_file || !DecodeImage( path, st ) )
                    st.Pixels.clear();
            }
            st.Position   = position;
            st.Background = background;
            st.Monitor    = mi.rcMonitor;
            st.Serial++;
        }

        out->Serial          = st.Serial;
        out->BackgroundColor = IM_COL32( GetRValue( st.Background ), GetGValue( st.Background ), GetBValue( st.Background ), 255 );
        if ( st.Pixels.empty() )
        {
            static unsigned char solid[ 4 ];
            solid[ 0 ]  = GetRValue( st.Background );
            solid[ 1 ]  = GetGValue( st.Background );
            solid[ 2 ]  = GetBValue( st.Background );
            solid[ 3 ]  = 255;
            out->Pixels = solid;
            out->Width = out->Height = 1;
            return true;
        }
        out->Pixels = st.Pixels.data();
        out->Width  = st.Width;
        out->Height = st.Height;
        PlaceWallpaper( st, out );
        return true;
    }

    bool GetScreenRects( ImGuiViewport * vp, ImVec4 * client, ImVec4 * monitor, void * )
    {
        HWND hwnd = ViewportHwnd( vp );
        RECT rc   = {};
        if ( !hwnd || !GetClientRect( hwnd, &rc ) )
            return false;
        POINT tl = { rc.left, rc.top }, br = { rc.right, rc.bottom };
        ClientToScreen( hwnd, &tl );
        ClientToScreen( hwnd, &br );
        MONITORINFO mi = { sizeof( mi ) };
        if ( !GetMonitorInfoW( MonitorFromWindow( hwnd, MONITOR_DEFAULTTONEAREST ), &mi ) )
            return false;
        *client  = ImVec4( ( float )tl.x, ( float )tl.y, ( float )br.x, ( float )br.y );
        *monitor = ImVec4( ( float )mi.rcMonitor.left, ( float )mi.rcMonitor.top, ( float )mi.rcMonitor.right, ( float )mi.rcMonitor.bottom );
        return true;
    }

    // Windows 11 22H2+: DWMWA_SYSTEMBACKDROP_TYPE (38), 2 = Mica, 4 = Mica Alt; 20 = DWMWA_USE_IMMERSIVE_DARK_MODE.
    bool SetNativeBackdrop( ImGuiViewport * vp, ImFluentBackdrop backdrop, bool dark, void * )
    {
        HWND hwnd = ViewportHwnd( vp );
        if ( !hwnd )
            return false;
        const BOOL dark_mode = dark ? TRUE : FALSE;
        DwmSetWindowAttribute( hwnd, 20, &dark_mode, sizeof( dark_mode ) );
        const int type        = backdrop == ImFluentBackdrop_Mica ? 2 : backdrop == ImFluentBackdrop_MicaAlt ? 4 : 1;
        const HRESULT hr      = DwmSetWindowAttribute( hwnd, 38, &type, sizeof( type ) );
        const int m           = backdrop != ImFluentBackdrop_None ? -1 : 0;
        const MARGINS margins = { m, m, m, m };
        DwmExtendFrameIntoClientArea( hwnd, &margins );
        return SUCCEEDED( hr ) && backdrop != ImFluentBackdrop_None;
    }
} // namespace

void ImFluent_ImplWin32_InstallBackdropHooks( bool allow_native_backdrop )
{
    const HRESULT hr               = CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );
    g_Win32Backdrop.ComInitialized = SUCCEEDED( hr ); // RPC_E_CHANGED_MODE works too
    ImFluentBackdropHooks & hooks  = ImFluent::GetBackdropHooks();
    hooks.GetWallpaper             = &GetWallpaper;
    hooks.GetScreenRects           = &GetScreenRects;
    hooks.SetNativeBackdrop        = allow_native_backdrop ? &SetNativeBackdrop : nullptr;
}

void ImFluent_ImplWin32_UninstallBackdropHooks()
{
    ImFluent::GetBackdropHooks() = ImFluentBackdropHooks();
    g_Win32Backdrop.Pixels.clear();
    g_Win32Backdrop.Pixels.shrink_to_fit();
    if ( g_Win32Backdrop.ComInitialized )
        CoUninitialize();
    g_Win32Backdrop.ComInitialized = false;
}