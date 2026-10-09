#include "imfluent_backdrop_linux.h"

#include "../imfluent.h"

#include <imgui.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined( IMFLUENT_BACKDROP_STB_IMAGE )
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

namespace
{
    const int kMaxDecodeSize = 512;
    const int kPathMax       = 4096;

    struct LinuxBackdropState
    {
        ImFluentDecodeImageFn Decode = NULL;
        ImVector< unsigned char > Pixels;
        int Width                = 0;
        int Height               = 0;
        int ImageW               = 0; // Before the downscale
        int ImageH               = 0;
        char Path[ kPathMax ]    = "";
        long long WriteTime      = 0;
        ImFluentWallpaperFit Fit = ImFluentWallpaperFit_Fill;
        ImU32 Background         = IM_COL32( 0, 0, 0, 255 );
        ImVec4 Monitor;
        unsigned int Serial      = 0;
        unsigned char Solid[ 4 ] = { 0, 0, 0, 255 };
    };
    LinuxBackdropState g_LinuxBackdrop;

    int MaxInt( int a, int b )
    {
        return a > b ? a : b;
    }

    // The first line a command prints, trimmed.
    bool RunCommand( const char * cmd, char * out, int out_size )
    {
        FILE * f = popen( cmd, "r" );
        if ( !f )
            return false;
        out[ 0 ]       = 0;
        const bool got = fgets( out, out_size, f ) != NULL;
        pclose( f );
        int n = got ? ( int )strlen( out ) : 0;
        while ( n > 0 && isspace( ( unsigned char )out[ n - 1 ] ) )
            out[ --n ] = 0;
        return n > 0;
    }

    // gsettings prints strings quoted: 'file:///usr/share/backgrounds/a%20b.jpg'.
    void Unquote( char * s )
    {
        int n = ( int )strlen( s );
        if ( n >= 2 && ( s[ 0 ] == '\'' || s[ 0 ] == '"' ) && s[ n - 1 ] == s[ 0 ] )
        {
            memmove( s, s + 1, n - 2 );
            s[ n - 2 ] = 0;
        }
    }

    void UriToPath( const char * uri, char * out, int out_size )
    {
        if ( strncmp( uri, "file://", 7 ) == 0 )
            uri += 7;
        int o = 0;
        for ( const char * p = uri; *p && o < out_size - 1; ++p )
        {
            if ( p[ 0 ] == '%' && isxdigit( ( unsigned char )p[ 1 ] ) && isxdigit( ( unsigned char )p[ 2 ] ) )
            {
                const char hex[ 3 ] = { p[ 1 ], p[ 2 ], 0 };
                out[ o++ ]          = ( char )strtol( hex, NULL, 16 );
                p += 2;
            }
            else
                out[ o++ ] = *p;
        }
        out[ o ] = 0;
    }

    bool Gsettings( const char * schema, const char * key, char * out, int out_size )
    {
        char cmd[ 256 ];
        snprintf( cmd, sizeof( cmd ), "gsettings get %s %s 2>/dev/null", schema, key );
        if ( !RunCommand( cmd, out, out_size ) )
            return false;
        Unquote( out );
        return out[ 0 ] != 0;
    }

    // picture-options of GNOME, Cinnamon and MATE.
    ImFluentWallpaperFit FitFromOptions( const char * options )
    {
        if ( strcmp( options, "scaled" ) == 0 )
            return ImFluentWallpaperFit_Fit;
        if ( strcmp( options, "stretched" ) == 0 )
            return ImFluentWallpaperFit_Stretch;
        if ( strcmp( options, "centered" ) == 0 )
            return ImFluentWallpaperFit_Center;
        if ( strcmp( options, "wallpaper" ) == 0 )
            return ImFluentWallpaperFit_Tile;
        if ( strcmp( options, "spanned" ) == 0 )
            return ImFluentWallpaperFit_Span;
        return ImFluentWallpaperFit_Fill; // "zoom"
    }

    ImU32 ParseHexColor( const char * s )
    {
        unsigned int r = 0, g = 0, b = 0;
        if ( s[ 0 ] == '#' && sscanf( s + 1, "%02x%02x%02x", &r, &g, &b ) == 3 )
            return IM_COL32( r, g, b, 255 );
        return IM_COL32( 0, 0, 0, 255 );
    }

    void ReadGnomeLike( const char * schema, const char * uri_key, bool prefer_dark, char * path, ImFluentWallpaperFit * fit, ImU32 * color )
    {
        char value[ kPathMax ];
        if ( ( prefer_dark && Gsettings( schema, "picture-uri-dark", value, kPathMax ) ) || Gsettings( schema, uri_key, value, kPathMax ) )
            UriToPath( value, path, kPathMax );
        if ( Gsettings( schema, "picture-options", value, kPathMax ) )
        {
            if ( strcmp( value, "none" ) == 0 )
                path[ 0 ] = 0;
            *fit = FitFromOptions( value );
        }
        if ( Gsettings( schema, "primary-color", value, kPathMax ) )
            *color = ParseHexColor( value );
    }

    // Plasma keeps the wallpaper per containment: [Containments][N][Wallpaper][org.kde.image][General].
    bool ReadPlasma( char * path, ImFluentWallpaperFit * fit, ImU32 * color )
    {
        const char * config = getenv( "XDG_CONFIG_HOME" );
        const char * home   = getenv( "HOME" );
        char file[ kPathMax ];
        if ( config && config[ 0 ] )
            snprintf( file, sizeof( file ), "%s/plasma-org.kde.plasma.desktop-appletsrc", config );
        else if ( home )
            snprintf( file, sizeof( file ), "%s/.config/plasma-org.kde.plasma.desktop-appletsrc", home );
        else
            return false;
        FILE * f = fopen( file, "r" );
        if ( !f )
            return false;
        char line[ kPathMax ];
        bool in_image = false, found = false;
        while ( fgets( line, sizeof( line ), f ) )
        {
            line[ strcspn( line, "\r\n" ) ] = 0;
            if ( line[ 0 ] == '[' )
            {
                if ( found )
                    break;
                in_image = strstr( line, "[Wallpaper][org.kde.image][General]" ) != NULL;
                continue;
            }
            if ( !in_image )
                continue;
            int r = 0, g = 0, b = 0;
            if ( strncmp( line, "Image=", 6 ) == 0 )
            {
                UriToPath( line + 6, path, kPathMax );
                found = true;
            }
            else if ( strncmp( line, "FillMode=", 9 ) == 0 )
            {
                // Qt Image.fillMode: 0 Stretch, 1 PreserveAspectFit, 2 PreserveAspectCrop, 3-5 Tile, 6 Pad.
                const int mode = atoi( line + 9 );
                *fit           = mode == 0 ? ImFluentWallpaperFit_Stretch : mode == 1 ? ImFluentWallpaperFit_Fit : mode == 6 ? ImFluentWallpaperFit_Center : mode >= 3 ? ImFluentWallpaperFit_Tile : ImFluentWallpaperFit_Fill;
            }
            else if ( strncmp( line, "Color=", 6 ) == 0 && sscanf( line + 6, "%d,%d,%d", &r, &g, &b ) == 3 )
                *color = IM_COL32( r, g, b, 255 );
        }
        fclose( f );
        return found;
    }

    bool ReadXfce( char * path, ImFluentWallpaperFit * fit )
    {
        char prop[ 512 ], cmd[ 768 ], value[ 64 ];
        if ( !RunCommand( "xfconf-query -c xfce4-desktop -l 2>/dev/null | grep -m1 'last-image$'", prop, sizeof( prop ) ) )
            return false;
        snprintf( cmd, sizeof( cmd ), "xfconf-query -c xfce4-desktop -p '%s' 2>/dev/null", prop );
        if ( !RunCommand( cmd, path, kPathMax ) )
            return false;
        // image-style next to last-image: 0 none, 1 centered, 2 tiled, 3 stretched, 4 scaled, 5 zoomed, 6 spanning.
        char * leaf = strstr( prop, "last-image" );
        snprintf( leaf, sizeof( prop ) - ( leaf - prop ), "image-style" );
        snprintf( cmd, sizeof( cmd ), "xfconf-query -c xfce4-desktop -p '%s' 2>/dev/null", prop );
        if ( RunCommand( cmd, value, sizeof( value ) ) )
        {
            const int style = atoi( value );
            *fit            = style == 1 ? ImFluentWallpaperFit_Center : style == 2 ? ImFluentWallpaperFit_Tile : style == 3 ? ImFluentWallpaperFit_Stretch : style == 4 ? ImFluentWallpaperFit_Fit : style == 6 ? ImFluentWallpaperFit_Span : ImFluentWallpaperFit_Fill;
        }
        return true;
    }

    // GNOME time-of-day and slideshow wallpapers are XML files: use the first <file>.
    void ResolveXml( char * path )
    {
        const int n = ( int )strlen( path );
        if ( n < 4 || strcmp( path + n - 4, ".xml" ) != 0 )
            return;
        FILE * f  = fopen( path, "rb" );
        path[ 0 ] = 0;
        if ( !f )
            return;
        static char buf[ 65536 ];
        const size_t len = fread( buf, 1, sizeof( buf ) - 1, f );
        fclose( f );
        buf[ len ]     = 0;
        const char * a = strstr( buf, "<file>" );
        if ( !a )
            return;
        a += 6;
        while ( isspace( ( unsigned char )*a ) )
            ++a;
        if ( *a == '<' ) // <file><size width=".." height="..">/path</size>
            a = strchr( a, '>' ) ? strchr( a, '>' ) + 1 : a;
        const char * b = strchr( a, '<' );
        if ( b )
            snprintf( path, kPathMax, "%.*s", ( int )( b - a ), a );
    }

    // KDE wallpaper packages are directories: use the largest contents/images/<W>x<H>.<ext>.
    void ResolvePackage( char * path )
    {
        struct stat sb;
        if ( stat( path, &sb ) != 0 || !S_ISDIR( sb.st_mode ) )
            return;
        char dir[ kPathMax ];
        snprintf( dir, sizeof( dir ), "%s/contents/images", path );
        path[ 0 ] = 0;
        DIR * d   = opendir( dir );
        if ( !d )
            return;
        long long best = 0;
        while ( struct dirent * e = readdir( d ) )
        {
            int w = 0, h = 0;
            if ( sscanf( e->d_name, "%dx%d", &w, &h ) == 2 && ( long long )w * h > best )
            {
                best = ( long long )w * h;
                snprintf( path, kPathMax, "%s/%s", dir, e->d_name );
            }
        }
        closedir( d );
    }

    bool Decode( const char * path, LinuxBackdropState & st )
    {
        unsigned char * px = NULL;
        int w = 0, h = 0;
        bool ok = false;
        if ( st.Decode )
            ok = st.Decode( path, &px, &w, &h );
#if defined( IMFLUENT_BACKDROP_STB_IMAGE )
        else
        {
            int channels = 0;
            px           = stbi_load( path, &w, &h, &channels, 4 );
            ok           = px != NULL;
        }
#endif
        if ( !ok || !px || w <= 0 || h <= 0 )
        {
            free( px );
            return false;
        }
        // Box filter down to kMaxDecodeSize.
        const int f  = MaxInt( 1, ( MaxInt( w, h ) + kMaxDecodeSize - 1 ) / kMaxDecodeSize );
        const int dw = MaxInt( 1, w / f ), dh = MaxInt( 1, h / f );
        st.Pixels.resize( dw * dh * 4 );
        for ( int y = 0; y < dh; ++y )
            for ( int x = 0; x < dw; ++x )
            {
                unsigned int sum[ 4 ] = { 0, 0, 0, 0 }, count = 0;
                for ( int sy = y * f; sy < ( y + 1 ) * f && sy < h; ++sy )
                    for ( int sx = x * f; sx < ( x + 1 ) * f && sx < w; ++sx, ++count )
                        for ( int c = 0; c < 4; ++c )
                            sum[ c ] += px[ ( ( size_t )sy * w + sx ) * 4 + c ];
                for ( int c = 0; c < 4; ++c )
                    st.Pixels[ ( y * dw + x ) * 4 + c ] = ( unsigned char )( sum[ c ] / ( count ? count : 1 ) );
            }
        st.Width  = dw;
        st.Height = dh;
        st.ImageW = w;
        st.ImageH = h;
        free( px );
        return true;
    }

    bool GetWallpaper( ImGuiViewport * vp, ImFluentWallpaper * out, void * )
    {
        LinuxBackdropState & st  = g_LinuxBackdrop;
        char path[ kPathMax ]    = "";
        char desktop[ 256 ]      = "";
        char value[ 64 ]         = "";
        ImFluentWallpaperFit fit = ImFluentWallpaperFit_Fill;
        ImU32 color              = IM_COL32( 0, 0, 0, 255 );
        if ( const char * env = getenv( "XDG_CURRENT_DESKTOP" ) )
            for ( int i = 0; env[ i ] && i < ( int )sizeof( desktop ) - 1; ++i )
                desktop[ i ] = ( char )tolower( ( unsigned char )env[ i ] );

        const bool prefer_dark = Gsettings( "org.gnome.desktop.interface", "color-scheme", value, sizeof( value ) ) && strcmp( value, "prefer-dark" ) == 0;
        if ( strstr( desktop, "kde" ) )
            ReadPlasma( path, &fit, &color );
        else if ( strstr( desktop, "xfce" ) )
            ReadXfce( path, &fit );
        else if ( strstr( desktop, "mate" ) )
            ReadGnomeLike( "org.mate.background", "picture-filename", false, path, &fit, &color );
        else
            ReadGnomeLike( strstr( desktop, "cinnamon" ) ? "org.cinnamon.desktop.background" : "org.gnome.desktop.background", "picture-uri", prefer_dark, path, &fit, &color );
        ResolveXml( path );
        ResolvePackage( path );

        // The monitor for the placement: from GetScreenRects when installed, else the window.
        const ImFluentBackdropHooks & hooks = ImFluent::GetBackdropHooks();
        ImVec4 client, monitor( 0.f, 0.f, vp ? vp->Size.x : 0.f, vp ? vp->Size.y : 0.f );
        if ( hooks.GetScreenRects )
            hooks.GetScreenRects( vp, &client, &monitor, hooks.UserData );

        struct stat sb;
        const bool has_file    = path[ 0 ] && stat( path, &sb ) == 0 && S_ISREG( sb.st_mode );
        const long long mtime  = has_file ? ( long long )sb.st_mtime : 0;
        const bool same_file   = has_file && strcmp( path, st.Path ) == 0 && mtime == st.WriteTime && !st.Pixels.empty();
        const bool same_screen = monitor.x == st.Monitor.x && monitor.y == st.Monitor.y && monitor.z == st.Monitor.z && monitor.w == st.Monitor.w;
        if ( !same_file || fit != st.Fit || color != st.Background || !same_screen )
        {
            if ( !same_file )
            {
                st.Pixels.clear();
                snprintf( st.Path, sizeof( st.Path ), "%s", path );
                st.WriteTime = mtime;
                if ( has_file )
                    Decode( path, st );
            }
            st.Fit        = fit;
            st.Background = color;
            st.Monitor    = monitor;
            st.Serial++;
        }

        out->Serial          = st.Serial;
        out->BackgroundColor = st.Background;
        if ( st.Pixels.empty() )
        {
            st.Solid[ 0 ] = ( unsigned char )( st.Background >> IM_COL32_R_SHIFT );
            st.Solid[ 1 ] = ( unsigned char )( st.Background >> IM_COL32_G_SHIFT );
            st.Solid[ 2 ] = ( unsigned char )( st.Background >> IM_COL32_B_SHIFT );
            out->Pixels   = st.Solid;
            out->Width = out->Height = 1;
            return true;
        }
        out->Pixels = st.Pixels.Data;
        out->Width  = st.Width;
        out->Height = st.Height;
        ImFluent::PlaceWallpaper( out, st.Fit, ( float )st.ImageW, ( float )st.ImageH, st.Monitor, st.Monitor );
        return true;
    }
} // namespace

void ImFluent_ImplLinux_InstallBackdropHooks( ImFluentDecodeImageFn decode )
{
    g_LinuxBackdrop.Decode                    = decode;
    ImFluent::GetBackdropHooks().GetWallpaper = &GetWallpaper;
}

void ImFluent_ImplLinux_UninstallBackdropHooks()
{
    ImFluent::GetBackdropHooks().GetWallpaper = NULL;
    g_LinuxBackdrop.Pixels.clear();
}