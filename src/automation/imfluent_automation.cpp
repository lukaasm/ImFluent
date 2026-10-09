#include "imfluent_automation.h"
#include "imfluent_mcp_http.h"

#include "imfluent.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "imgui_te_exporters.h"
#include "imgui_te_ui.h"

#include <nlohmann/json.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

#include <chrono>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace
{
    // [SECTION] State

    // One MCP tools/call. Created on the HTTP thread, executed on the main thread.
    struct Job
    {
        std::string Tool;
        json Args;
        std::promise< json > Promise;
        int FramesLeft = 0; // Immediate jobs that finish after N more rendered frames
        bool Started   = false;
        bool Done      = false;

        void Finish( const json & result )
        {
            if ( Done )
                return;
            Done = true;
            Promise.set_value( result );
        }
    };
    typedef std::shared_ptr< Job > JobPtr;

    struct AutomationState
    {
        ImFluentAutomationConfig Config;
        ImFluentAutomationHost Host;
        ImGuiTestEngine * Engine = nullptr;
        ImGuiTest * ExecTest     = nullptr;
        ImFluentMcpHttpServer Server;

        std::mutex QueueMutex;
        std::deque< JobPtr > Queue;
        JobPtr Current;    // Job at the front, being executed
        json EngineResult; // Written by the test coroutine for the current job
        bool EngineQueued = false;
        bool ShuttingDown = false;

        // --run-tests
        bool CliTestsQueued = false;
        bool CliTestsDone   = false;
        int CliExitCode     = 0;
        int FrameCounter    = 0;
    };

    AutomationState g_Auto;

    const char * kServerName    = "imfluent-gallery";
    const char * kServerVersion = "0.1.0";

    // [SECTION] Small helpers

    std::string FormatId( ImGuiID id )
    {
        char buf[ 16 ];
        snprintf( buf, sizeof( buf ), "0x%08X", id );
        return buf;
    }

    json RectToJson( const ImRect & r )
    {
        return json::array( { ( int )IM_ROUND( r.Min.x ), ( int )IM_ROUND( r.Min.y ), ( int )IM_ROUND( r.GetWidth() ), ( int )IM_ROUND( r.GetHeight() ) } );
    }

    const char * FindLabel( ImGuiID id )
    {
        if ( id == 0 )
            return "";
        const char * label = ImGuiTestEngine_FindItemDebugLabel( GImGui, id );
        return label ? label : "";
    }

    bool IsHexId( const std::string & s )
    {
        if ( s.size() < 3 || s[ 0 ] != '0' || ( s[ 1 ] != 'x' && s[ 1 ] != 'X' ) )
            return false;
        for ( size_t i = 2; i < s.size(); ++i )
            if ( !isxdigit( ( unsigned char )s[ i ] ) )
                return false;
        return true;
    }

    // Turns a user reference into a Test Engine path:
    //  "0x1234ABCD"            -> item id
    //  "//Window/Item"         -> absolute path, unchanged
    //  "Group/Label"           -> "//<root>/Group/Label"
    //  "Label"                 -> "//<root>/**/Label" (search by label)
    // `window` replaces <root>; "$FOCUSED" names the focused window (useful for popups and flyouts).
    std::string ResolveRefPath( const std::string & ref, const std::string & window )
    {
        if ( ref.size() >= 2 && ref[ 0 ] == '/' && ref[ 1 ] == '/' )
            return ref;
        std::string root = window.empty() ? std::string( g_Auto.Config.RootWindow ) : window;
        if ( root.size() >= 2 && root[ 0 ] == '/' && root[ 1 ] == '/' )
            root = root.substr( 2 );
        std::string path = "//" + root + "/";
        if ( ref.find( '/' ) == std::string::npos )
            path += "**/";
        return path + ref;
    }

    ImGuiTestRef MakeRef( const std::string & ref, const std::string & window, std::string & storage )
    {
        if ( IsHexId( ref ) )
            return ImGuiTestRef( ( ImGuiID )strtoul( ref.c_str() + 2, NULL, 16 ) );
        storage = ResolveRefPath( ref, window );
        return ImGuiTestRef( storage.c_str() );
    }

    json StatusFlagsToJson( ImGuiItemStatusFlags status, ImGuiItemFlags item_flags )
    {
        json flags = json::array();
        if ( item_flags & ImGuiItemFlags_Disabled )
            flags.push_back( "disabled" );
        if ( status & ImGuiItemStatusFlags_Openable )
            flags.push_back( ( status & ImGuiItemStatusFlags_Opened ) ? "opened" : "closed" );
        if ( status & ImGuiItemStatusFlags_Checkable )
            flags.push_back( ( status & ImGuiItemStatusFlags_Checked ) ? "checked" : "unchecked" );
        if ( status & ImGuiItemStatusFlags_Inputable )
            flags.push_back( "inputable" );
        return flags;
    }

    json ItemToJson( const ImGuiTestItemInfo & item )
    {
        ImGuiContext & g = *GImGui;
        json flags       = StatusFlagsToJson( item.StatusFlags, item.ItemFlags );
        if ( g.NavId == item.ID )
            flags.push_back( "focused" );
        if ( g.ActiveId == item.ID )
            flags.push_back( "active" );
        if ( g.HoveredId == item.ID )
            flags.push_back( "hovered" );
        json j       = json::object();
        j[ "id" ]    = FormatId( item.ID );
        j[ "label" ] = item.DebugLabel;
        j[ "rect" ]  = RectToJson( item.RectFull );
        if ( item.Window )
            j[ "window" ] = item.Window->Name;
        if ( !flags.empty() )
            j[ "flags" ] = flags;
        if ( item.NavLayer != 0 )
            j[ "nav_layer" ] = "menu";
        return j;
    }

    json NavState()
    {
        ImGuiContext & g = *GImGui;
        json j           = json::object();
        json focus       = json::object();
        focus[ "id" ]    = FormatId( g.NavId );
        focus[ "label" ] = FindLabel( g.NavId );
        if ( g.NavWindow )
        {
            focus[ "window" ] = g.NavWindow->Name;
            if ( g.NavId != 0 && g.NavWindow->NavRectRel[ g.NavLayer ].GetWidth() > 0.f )
                focus[ "rect" ] = RectToJson( ImGui::WindowRectRelToAbs( g.NavWindow, g.NavWindow->NavRectRel[ g.NavLayer ] ) );
        }
        focus[ "cursor_visible" ] = g.NavCursorVisible;
        j[ "focus" ]              = focus;
        if ( g.ActiveId != 0 )
            j[ "active" ] = { { "id", FormatId( g.ActiveId ) }, { "label", FindLabel( g.ActiveId ) } };
        if ( g.HoveredId != 0 )
            j[ "hovered" ] = { { "id", FormatId( g.HoveredId ) }, { "label", FindLabel( g.HoveredId ) } };
        json popups = json::array();
        for ( const ImGuiPopupData & p : g.OpenPopupStack )
        {
            json pj    = json::object();
            pj[ "id" ] = FormatId( p.PopupId );
            if ( p.Window )
                pj[ "window" ] = p.Window->Name;
            pj[ "opener" ] = { { "id", FormatId( p.OpenParentId ) }, { "label", FindLabel( p.OpenParentId ) } };
            popups.push_back( pj );
        }
        if ( !popups.empty() )
            j[ "popups" ] = popups;
        static const char * sources[] = { "none", "mouse", "keyboard", "gamepad" };
        j[ "input_source" ]           = sources[ ImClamp( ( int )g.NavInputSource, 0, 3 ) ];
        if ( g.IO.WantTextInput )
            j[ "text_input" ] = true;
        return j;
    }

    json ListWindows( bool include_hidden )
    {
        ImGuiContext & g = *GImGui;
        json out         = json::array();
        for ( int i = 0; i < g.Windows.Size; ++i )
        {
            ImGuiWindow * w = g.Windows[ i ];
            if ( !include_hidden && ( !w->WasActive || w->Hidden ) )
                continue;
            json j      = json::object();
            j[ "name" ] = w->Name;
            j[ "id" ]   = FormatId( w->ID );
            j[ "rect" ] = RectToJson( w->Rect() );
            json kinds  = json::array();
            if ( w->Flags & ImGuiWindowFlags_ChildWindow )
                kinds.push_back( "child" );
            if ( w->Flags & ImGuiWindowFlags_Popup )
                kinds.push_back( "popup" );
            if ( w->Flags & ImGuiWindowFlags_Modal )
                kinds.push_back( "modal" );
            if ( w->Flags & ImGuiWindowFlags_Tooltip )
                kinds.push_back( "tooltip" );
            if ( w == g.NavWindow )
                kinds.push_back( "focused" );
            if ( !kinds.empty() )
                j[ "kind" ] = kinds;
            if ( w->ParentWindow )
                j[ "parent" ] = w->ParentWindow->Name;
            out.push_back( j );
        }
        return out;
    }

    // [SECTION] Key chords

    ImGuiKey FindKeyByName( const std::string & name )
    {
        struct Alias
        {
            const char * Name;
            ImGuiKey Key;
        };
        static const Alias aliases[] = {
            { "left", ImGuiKey_LeftArrow },
            { "right", ImGuiKey_RightArrow },
            { "up", ImGuiKey_UpArrow },
            { "down", ImGuiKey_DownArrow },
            { "esc", ImGuiKey_Escape },
            { "return", ImGuiKey_Enter },
            { "del", ImGuiKey_Delete },
            { "ins", ImGuiKey_Insert },
            { "pgup", ImGuiKey_PageUp },
            { "pgdn", ImGuiKey_PageDown },
            { "ctrl", ImGuiKey_LeftCtrl },
            { "control", ImGuiKey_LeftCtrl },
            { "shift", ImGuiKey_LeftShift },
            { "alt", ImGuiKey_LeftAlt },
            { "win", ImGuiKey_LeftSuper },
            { "super", ImGuiKey_LeftSuper },
            { "spacebar", ImGuiKey_Space },
            { "apps", ImGuiKey_Menu },
            { "contextmenu", ImGuiKey_Menu },
        };
        for ( const Alias & a : aliases )
            if ( ImStricmp( a.Name, name.c_str() ) == 0 )
                return a.Key;
        for ( int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k )
            if ( ImStricmp( ImGui::GetKeyName( ( ImGuiKey )k ), name.c_str() ) == 0 )
                return ( ImGuiKey )k;
        return ImGuiKey_None;
    }

    // "Ctrl+Shift+Tab" -> chord. Returns false on unknown names.
    bool ParseKeyChord( const std::string & text, ImGuiKeyChord & out )
    {
        out          = 0;
        ImGuiKey key = ImGuiKey_None;
        size_t start = 0;
        while ( start <= text.size() )
        {
            size_t plus = text.find( '+', start );
            if ( plus == start && plus + 1 == text.size() )
                plus = std::string::npos; // trailing "+" is not a separator
            std::string tok = text.substr( start, plus == std::string::npos ? std::string::npos : plus - start );
            const bool last = ( plus == std::string::npos );
            if ( !last && ImStricmp( tok.c_str(), "ctrl" ) == 0 )
                out |= ImGuiMod_Ctrl;
            else if ( !last && ImStricmp( tok.c_str(), "control" ) == 0 )
                out |= ImGuiMod_Ctrl;
            else if ( !last && ImStricmp( tok.c_str(), "shift" ) == 0 )
                out |= ImGuiMod_Shift;
            else if ( !last && ImStricmp( tok.c_str(), "alt" ) == 0 )
                out |= ImGuiMod_Alt;
            else if ( !last && ( ImStricmp( tok.c_str(), "super" ) == 0 || ImStricmp( tok.c_str(), "win" ) == 0 || ImStricmp( tok.c_str(), "cmd" ) == 0 ) )
                out |= ImGuiMod_Super;
            else
            {
                key = FindKeyByName( tok );
                if ( key == ImGuiKey_None )
                    return false;
            }
            if ( last )
                break;
            start = plus + 1;
        }
        if ( key == ImGuiKey_None )
            return false;
        out |= key;
        return true;
    }

    std::vector< std::string > SplitKeys( const std::string & text )
    {
        std::vector< std::string > out;
        std::string cur;
        for ( char c : text )
        {
            if ( c == ' ' || c == ',' )
            {
                if ( !cur.empty() )
                    out.push_back( cur );
                cur.clear();
            }
            else
                cur += c;
        }
        if ( !cur.empty() )
            out.push_back( cur );
        return out;
    }

    // [SECTION] Screenshots

    void PngWriteCallback( void * context, void * data, int size )
    {
        std::vector< unsigned char > * out = ( std::vector< unsigned char > * )context;
        out->insert( out->end(), ( unsigned char * )data, ( unsigned char * )data + size );
    }

    std::string Base64Encode( const unsigned char * data, size_t len )
    {
        static const char * table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve( ( len + 2 ) / 3 * 4 );
        for ( size_t i = 0; i < len; i += 3 )
        {
            const unsigned int b0 = data[ i ];
            const unsigned int b1 = ( i + 1 < len ) ? data[ i + 1 ] : 0;
            const unsigned int b2 = ( i + 2 < len ) ? data[ i + 2 ] : 0;
            const unsigned int v  = ( b0 << 16 ) | ( b1 << 8 ) | b2;
            out += table[ ( v >> 18 ) & 63 ];
            out += table[ ( v >> 12 ) & 63 ];
            out += ( i + 1 < len ) ? table[ ( v >> 6 ) & 63 ] : '=';
            out += ( i + 2 < len ) ? table[ v & 63 ] : '=';
        }
        return out;
    }

    // Box-filter downscale of an RGBA8 image.
    void DownscaleRgba( const std::vector< unsigned int > & src, int sw, int sh, float scale, std::vector< unsigned int > & dst, int & dw, int & dh )
    {
        dw = ImMax( 1, ( int )( sw * scale ) );
        dh = ImMax( 1, ( int )( sh * scale ) );
        dst.resize( ( size_t )dw * dh );
        for ( int y = 0; y < dh; ++y )
            for ( int x = 0; x < dw; ++x )
            {
                const int x0 = x * sw / dw, x1 = ImMax( x0 + 1, ( x + 1 ) * sw / dw );
                const int y0 = y * sh / dh, y1 = ImMax( y0 + 1, ( y + 1 ) * sh / dh );
                unsigned int acc[ 4 ] = { 0, 0, 0, 0 };
                int n                 = 0;
                for ( int yy = y0; yy < y1; ++yy )
                    for ( int xx = x0; xx < x1; ++xx )
                    {
                        const unsigned int p = src[ ( size_t )yy * sw + xx ];
                        acc[ 0 ] += p & 0xFF;
                        acc[ 1 ] += ( p >> 8 ) & 0xFF;
                        acc[ 2 ] += ( p >> 16 ) & 0xFF;
                        acc[ 3 ] += ( p >> 24 ) & 0xFF;
                        ++n;
                    }
                dst[ ( size_t )y * dw + x ] = ( acc[ 0 ] / n ) | ( ( acc[ 1 ] / n ) << 8 ) | ( ( acc[ 2 ] / n ) << 16 ) | ( ( acc[ 3 ] / n ) << 24 );
            }
    }

    json ToolError( const std::string & message )
    {
        return { { "content", json::array( { { { "type", "text" }, { "text", message } } } ) }, { "isError", true } };
    }

    json ToolText( const json & payload )
    {
        return { { "content", json::array( { { { "type", "text" }, { "text", payload.dump( 1 ) } } } ) }, { "isError", false } };
    }

    json TakeScreenshot( const json & args )
    {
        ImGuiContext & g = *GImGui;
        if ( !g_Auto.Host.CaptureFramebuffer )
            return ToolError( "screenshot: the host has no framebuffer capture" );

        ImRect r( ImVec2( 0, 0 ), g.IO.DisplaySize );
        if ( args.contains( "window" ) && args[ "window" ].is_string() )
        {
            const std::string name = args[ "window" ];
            ImGuiWindow * w        = ( name == "$FOCUSED" ) ? g.NavWindow : ImGui::FindWindowByName( name.c_str() );
            if ( !w )
                return ToolError( "screenshot: no window named '" + name + "'" );
            r = w->Rect();
        }
        if ( args.contains( "rect" ) && args[ "rect" ].is_array() && args[ "rect" ].size() == 4 )
        {
            const json & a = args[ "rect" ];
            r              = ImRect( a[ 0 ].get< float >(), a[ 1 ].get< float >(), a[ 0 ].get< float >() + a[ 2 ].get< float >(), a[ 1 ].get< float >() + a[ 3 ].get< float >() );
        }
        const float margin = args.value( "margin", 0.f );
        r.Expand( margin );
        r.ClipWith( ImRect( ImVec2( 0, 0 ), g.IO.DisplaySize ) );
        if ( r.GetWidth() < 1.f || r.GetHeight() < 1.f )
            return ToolError( "screenshot: empty capture rectangle" );

        const ImVec2 fb_scale = g.IO.DisplayFramebufferScale;
        const int x           = ( int )( r.Min.x * fb_scale.x );
        const int y           = ( int )( r.Min.y * fb_scale.y );
        const int w           = ( int )( r.GetWidth() * fb_scale.x );
        const int h           = ( int )( r.GetHeight() * fb_scale.y );
        std::vector< unsigned int > pixels( ( size_t )w * h );
        if ( !g_Auto.Host.CaptureFramebuffer( x, y, w, h, pixels.data(), g_Auto.Host.UserData ) )
            return ToolError( "screenshot: framebuffer capture failed" );

        int out_w = w, out_h = h;
        const float scale = ImClamp( args.value( "scale", 1.f ), 0.05f, 1.f );
        std::vector< unsigned int > scaled;
        const std::vector< unsigned int > * img = &pixels;
        if ( scale < 1.f )
        {
            DownscaleRgba( pixels, w, h, scale, scaled, out_w, out_h );
            img = &scaled;
        }
        std::vector< unsigned char > png;
        if ( !stbi_write_png_to_func( PngWriteCallback, &png, out_w, out_h, 4, img->data(), out_w * 4 ) )
            return ToolError( "screenshot: PNG encoding failed" );

        json info      = json::object();
        info[ "rect" ] = RectToJson( r );
        info[ "size" ] = json::array( { out_w, out_h } );
        if ( args.contains( "save_path" ) && args[ "save_path" ].is_string() )
        {
            const std::string path = args[ "save_path" ];
            FILE * f               = fopen( path.c_str(), "wb" );
            if ( !f )
                return ToolError( "screenshot: cannot write '" + path + "'" );
            fwrite( png.data(), 1, png.size(), f );
            fclose( f );
            info[ "saved" ] = path;
        }
        json content = json::array();
        if ( args.value( "inline", true ) )
            content.push_back( { { "type", "image" }, { "data", Base64Encode( png.data(), png.size() ) }, { "mimeType", "image/png" } } );
        content.push_back( { { "type", "text" }, { "text", info.dump() } } );
        return { { "content", content }, { "isError", false } };
    }

    // [SECTION] Gallery pages

    std::string ResolvePage( const std::string & name )
    {
        for ( int i = 0; ImFluent::GetDemoWindowPageId( i ); ++i )
            if ( ImStricmp( ImFluent::GetDemoWindowPageId( i ), name.c_str() ) == 0 )
                return ImFluent::GetDemoWindowPageId( i );
        const char * prefixes[] = { "Item_", "Section_" };
        for ( const char * prefix : prefixes )
        {
            const std::string candidate = std::string( prefix ) + name;
            for ( int i = 0; ImFluent::GetDemoWindowPageId( i ); ++i )
                if ( ImStricmp( ImFluent::GetDemoWindowPageId( i ), candidate.c_str() ) == 0 )
                    return ImFluent::GetDemoWindowPageId( i );
        }
        return std::string();
    }

    // [SECTION] Engine-side execution (runs inside the test coroutine)

    void SetRunSpeed( const json & args )
    {
        ImGuiTestEngineIO & io  = ImGuiTestEngine_GetIO( g_Auto.Engine );
        io.ConfigRunSpeed       = g_Auto.Config.NormalSpeed ? ImGuiTestRunSpeed_Normal : ImGuiTestRunSpeed_Fast;
        const std::string speed = args.value( "speed", std::string() );
        if ( speed == "fast" )
            io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
        else if ( speed == "normal" )
            io.ConfigRunSpeed = ImGuiTestRunSpeed_Normal;
        else if ( speed == "cinematic" )
            io.ConfigRunSpeed = ImGuiTestRunSpeed_Cinematic;
    }

    ImGuiMouseButton ParseButton( const json & step )
    {
        const std::string b = step.value( "button", std::string( "left" ) );
        if ( b == "right" )
            return ImGuiMouseButton_Right;
        if ( b == "middle" )
            return ImGuiMouseButton_Middle;
        return ImGuiMouseButton_Left;
    }

    // Executes one step. Returns an error message, or an empty string on success.
    std::string RunStep( ImGuiTestContext * ctx, const json & step, const std::string & window, json & out )
    {
        const std::string action = step.value( "action", std::string() );
        const std::string ref    = step.value( "ref", std::string() );
        std::string ref_storage;
        const bool needs_ref = action == "click" || action == "hover" || action == "open" || action == "close" || action == "check" || action == "uncheck" || action == "focus" || action == "scroll_to" || action == "hold";
        if ( needs_ref && ref.empty() )
            return "step '" + action + "' needs a 'ref'";
        const ImGuiTestRef tref = ref.empty() ? ImGuiTestRef() : MakeRef( ref, step.value( "window", window ), ref_storage );

        if ( action == "click" )
        {
            if ( step.value( "double", false ) )
                ctx->ItemDoubleClick( tref );
            else
                ctx->ItemClick( tref, ParseButton( step ) );
        }
        else if ( action == "hover" )
            ctx->MouseMove( tref );
        else if ( action == "hold" )
            ctx->ItemHold( tref, step.value( "seconds", 1.f ) );
        else if ( action == "open" )
            ctx->ItemOpen( tref );
        else if ( action == "close" )
            ctx->ItemClose( tref );
        else if ( action == "check" )
            ctx->ItemCheck( tref );
        else if ( action == "uncheck" )
            ctx->ItemUncheck( tref );
        else if ( action == "focus" )
            ctx->NavMoveTo( tref );
        else if ( action == "scroll_to" )
            ctx->ScrollToItemY( tref );
        else if ( action == "key" )
        {
            const std::string keys                  = step.value( "keys", std::string() );
            const int count                         = ImMax( 1, step.value( "count", 1 ) );
            const std::vector< std::string > chords = SplitKeys( keys );
            if ( chords.empty() )
                return "step 'key' needs 'keys', e.g. \"Tab\" or \"Shift+Tab Enter\"";
            for ( const std::string & c : chords )
            {
                ImGuiKeyChord chord;
                if ( !ParseKeyChord( c, chord ) )
                    return "unknown key '" + c + "'";
            }
            for ( int n = 0; n < count; ++n )
                for ( const std::string & c : chords )
                {
                    ImGuiKeyChord chord;
                    ParseKeyChord( c, chord );
                    ctx->KeyPress( chord );
                }
        }
        else if ( action == "key_down" || action == "key_up" )
        {
            ImGuiKeyChord chord;
            if ( !ParseKeyChord( step.value( "keys", std::string() ), chord ) )
                return "unknown key '" + step.value( "keys", std::string() ) + "'";
            if ( action == "key_down" )
                ctx->KeyDown( chord );
            else
                ctx->KeyUp( chord );
        }
        else if ( action == "type" )
        {
            const std::string text = step.value( "text", std::string() );
            if ( !ref.empty() )
                ctx->ItemClick( tref );
            if ( step.value( "replace", false ) )
                ctx->KeyCharsReplace( text.c_str() );
            else
                ctx->KeyChars( text.c_str() );
            if ( step.value( "enter", false ) )
                ctx->KeyPress( ImGuiKey_Enter );
        }
        else if ( action == "mouse_move" )
            ctx->MouseMoveToPos( ImVec2( step.value( "x", 0.f ), step.value( "y", 0.f ) ) );
        else if ( action == "mouse_click" )
        {
            if ( step.contains( "x" ) && step.contains( "y" ) )
                ctx->MouseMoveToPos( ImVec2( step.value( "x", 0.f ), step.value( "y", 0.f ) ) );
            ctx->MouseClickMulti( ParseButton( step ), ImMax( 1, step.value( "count", 1 ) ) );
        }
        else if ( action == "mouse_down" )
            ctx->MouseDown( ParseButton( step ) );
        else if ( action == "mouse_up" )
            ctx->MouseUp( ParseButton( step ) );
        else if ( action == "wheel" )
            ctx->MouseWheel( ImVec2( step.value( "dx", 0.f ), step.value( "dy", 0.f ) ) );
        else if ( action == "wait" )
        {
            if ( step.contains( "seconds" ) )
                ctx->SleepNoSkip( step.value( "seconds", 0.f ), 1.f / 60.f );
            else
                ctx->Yield( ImMax( 1, step.value( "frames", 1 ) ) );
        }
        else if ( action == "close_popups" )
            ctx->PopupCloseAll();
        else if ( action == "input_mode" )
        {
            const std::string mode = step.value( "mode", std::string( "mouse" ) );
            ctx->SetInputMode( mode == "keyboard" ? ImGuiInputSource_Keyboard : mode == "gamepad" ? ImGuiInputSource_Gamepad : ImGuiInputSource_Mouse );
        }
        else
            return "unknown action '" + action + "'";

        if ( ctx->IsError() )
            return "step '" + action + "' failed";
        if ( step.value( "state", false ) )
            out[ "state" ] = NavState();
        return std::string();
    }

    std::string CollectLog( ImGuiTest * test, ImGuiTestVerboseLevel max_level )
    {
        ImGuiTextBuffer buf;
        test->Output.Log.ExtractLinesForVerboseLevels( ImGuiTestVerboseLevel_Error, max_level, &buf );
        return buf.c_str();
    }

    void ExecTestFunc( ImGuiTestContext * ctx )
    {
        JobPtr job = g_Auto.Current;
        if ( !job )
            return;
        const json & args        = job->Args;
        const std::string window = args.value( "window", std::string() );
        json result              = json::object();

        if ( job->Tool == "list_items" )
        {
            std::string storage;
            const std::string ref = args.value( "ref", std::string() );
            ImGuiTestRef parent;
            if ( ref.empty() )
            {
                storage = "//" + ( window.empty() ? std::string( g_Auto.Config.RootWindow ) : window );
                parent  = ImGuiTestRef( storage.c_str() );
            }
            else if ( IsHexId( ref ) || ( ref.size() >= 2 && ref[ 0 ] == '/' && ref[ 1 ] == '/' ) )
                parent = MakeRef( ref, window, storage );
            else
            {
                storage = "//" + ( window.empty() ? std::string( g_Auto.Config.RootWindow ) : window ) + "/" + ref;
                parent  = ImGuiTestRef( storage.c_str() );
            }
            ImGuiTestItemList list;
            ctx->GatherItems( &list, parent, args.value( "depth", -1 ) );
            const bool visible_only  = args.value( "visible_only", true );
            const bool labeled_only  = args.value( "labeled_only", true );
            const std::string filter = args.value( "filter", std::string() );
            const int max_items      = args.value( "max", 400 );
            json items               = json::array();
            int skipped              = 0;
            for ( int n = 0; n < list.GetSize(); ++n )
            {
                const ImGuiTestItemInfo * item = list.GetByIndex( n );
                if ( visible_only && ( item->RectClipped.GetWidth() <= 0.f || item->RectClipped.GetHeight() <= 0.f ) )
                    continue;
                if ( labeled_only && item->DebugLabel[ 0 ] == 0 )
                    continue;
                if ( !filter.empty() && !ImStristr( item->DebugLabel, NULL, filter.c_str(), NULL ) )
                    continue;
                if ( ( int )items.size() >= max_items )
                {
                    ++skipped;
                    continue;
                }
                items.push_back( ItemToJson( *item ) );
            }
            result[ "items" ] = items;
            if ( skipped )
                result[ "truncated" ] = skipped;
        }
        else if ( job->Tool == "item_info" )
        {
            std::string storage;
            const ImGuiTestItemInfo item = ctx->ItemInfo( MakeRef( args.value( "ref", std::string() ), window, storage ), ImGuiTestOpFlags_NoError );
            if ( item.ID == 0 )
                result[ "error" ] = "item not found";
            else
                result[ "item" ] = ItemToJson( item );
        }
        else
        {
            // Every action tool is a list of steps.
            json steps = json::array();
            if ( job->Tool == "run_steps" )
                steps = args.value( "steps", json::array() );
            else
            {
                json step = args;
                if ( job->Tool == "click" )
                    step[ "action" ] = "click";
                else if ( job->Tool == "hover" )
                    step[ "action" ] = "hover";
                else if ( job->Tool == "key" )
                    step[ "action" ] = "key";
                else if ( job->Tool == "type_text" )
                    step[ "action" ] = "type";
                else if ( job->Tool == "mouse" )
                    step[ "action" ] = args.value( "action", std::string( "mouse_move" ) );
                steps.push_back( step );
            }
            if ( args.value( "keyboard", false ) )
                ctx->SetInputMode( ImGuiInputSource_Keyboard );
            json step_results = json::array();
            for ( size_t i = 0; i < steps.size(); ++i )
            {
                json out              = json::object();
                const std::string err = RunStep( ctx, steps[ i ], window, out );
                out[ "step" ]         = ( int )i;
                if ( !err.empty() )
                {
                    out[ "error" ] = err;
                    step_results.push_back( out );
                    result[ "failed_step" ] = ( int )i;
                    break;
                }
                if ( out.size() > 1 )
                    step_results.push_back( out );
            }
            ctx->Yield( 2 ); // Let the UI settle so the reported state matches the next screenshot
            if ( !step_results.empty() )
                result[ "steps" ] = step_results;
            result[ "state" ] = NavState();
        }
        g_Auto.EngineResult = result;
    }

    // [SECTION] Tool table

    bool IsEngineTool( const std::string & tool )
    {
        return tool == "list_items" || tool == "item_info" || tool == "click" || tool == "hover" || tool == "key" || tool == "type_text" || tool == "mouse" || tool == "run_steps" || tool == "run_tests";
    }

    json ToolDefinitions()
    {
        const json ref_desc    = "Item reference. A plain label (\"Standard button\") is searched by label under the gallery window. "
                                 "Use \"Parent/Label\" for an ID path, \"//WindowName/Label\" for an absolute path, or an id from list_items (\"0x1234ABCD\").";
        const json window_desc = "Window that relative references resolve against. Default: the gallery root window. "
                                 "Use \"$FOCUSED\" for the focused window, for example an open flyout or popup.";
        const json speed_desc  = "fast (default: teleport the mouse), normal (watchable), or cinematic.";
        json tools             = json::array();
        auto add               = [ & ]( const char * name, const char * desc, json props, json required )
        {
            json schema = { { "type", "object" }, { "properties", props } };
            if ( !required.empty() )
                schema[ "required" ] = required;
            tools.push_back( { { "name", name }, { "description", desc }, { "inputSchema", schema } } );
        };

        add( "app_info", "Application state: display size, DPI scale, theme, current gallery page, Test Engine status.", json::object(), json::array() );
        add( "list_pages", "List the gallery page ids accepted by goto_page.", json::object(), json::array() );
        add( "goto_page", "Show a gallery page. Accepts a page id from list_pages or a control name such as \"Button\" or \"ComboBox\".", { { "page", { { "type", "string" } } } }, json::array( { "page" } ) );
        add( "set_theme", "Switch the Fluent theme.", { { "theme", { { "type", "string" }, { "enum", { "light", "dark", "highcontrast" } } } } }, json::array( { "theme" } ) );
        add( "resize_window", "Resize the application window (window coordinates).", { { "width", { { "type", "integer" } } }, { "height", { { "type", "integer" } } } }, json::array( { "width", "height" } ) );
        add( "screenshot", "Capture the last rendered frame as PNG. Crop to a window or a rectangle; downscale with 'scale'.",
            { { "window", { { "type", "string" }, { "description", "Window name from list_windows, or \"$FOCUSED\"." } } }, { "rect", { { "type", "array" }, { "items", { { "type", "number" } } }, { "description", "[x, y, width, height] in UI coordinates." } } }, { "margin", { { "type", "number" } } }, { "scale", { { "type", "number" }, { "description", "0.05..1 downscale factor." } } },
                { "save_path", { { "type", "string" }, { "description", "Also write the PNG to this file." } } }, { "inline", { { "type", "boolean" }, { "description", "Return the image in the result (default true)." } } } },
            json::array() );
        add( "list_windows", "List visible Dear ImGui windows (popups, flyouts, child windows) with their rectangles.", { { "include_hidden", { { "type", "boolean" } } } }, json::array() );
        add( "nav_state", "Keyboard focus (nav id, label, rect, whether the focus ring shows), active and hovered items, open popups.", json::object(), json::array() );
        add( "list_items", "List the items (widgets) under a window or item, with id, label, rectangle and state flags.",
            { { "ref", { { "type", "string" }, { "description", "Parent item or path. Default: the whole window." } } }, { "window", { { "type", "string" }, { "description", window_desc } } }, { "depth", { { "type", "integer" } } }, { "filter", { { "type", "string" }, { "description", "Keep labels that contain this text (case-insensitive)." } } }, { "visible_only", { { "type", "boolean" } } },
                { "labeled_only", { { "type", "boolean" } } }, { "max", { { "type", "integer" } } } },
            json::array() );
        add( "item_info", "Information about one item.", { { "ref", { { "type", "string" }, { "description", ref_desc } } }, { "window", { { "type", "string" }, { "description", window_desc } } } }, json::array( { "ref" } ) );
        add( "click", "Move the mouse to an item and click it.", { { "ref", { { "type", "string" }, { "description", ref_desc } } }, { "window", { { "type", "string" }, { "description", window_desc } } }, { "button", { { "type", "string" }, { "enum", { "left", "right", "middle" } } } }, { "double", { { "type", "boolean" } } }, { "speed", { { "type", "string" }, { "description", speed_desc } } } },
            json::array( { "ref" } ) );
        add( "hover", "Move the mouse over an item.", { { "ref", { { "type", "string" }, { "description", ref_desc } } }, { "window", { { "type", "string" }, { "description", window_desc } } } }, json::array( { "ref" } ) );
        add( "key", "Press keys. 'keys' holds chords separated by spaces, e.g. \"Tab\", \"Shift+Tab\", \"Alt+Down\", \"Down Down Enter\". Returns the keyboard focus afterwards.", { { "keys", { { "type", "string" } } }, { "count", { { "type", "integer" }, { "description", "Repeat the whole sequence." } } }, { "speed", { { "type", "string" }, { "description", speed_desc } } } },
            json::array( { "keys" } ) );
        add( "type_text", "Type characters into the focused text field, or click 'ref' first.",
            { { "text", { { "type", "string" } } }, { "ref", { { "type", "string" }, { "description", ref_desc } } }, { "window", { { "type", "string" }, { "description", window_desc } } }, { "replace", { { "type", "boolean" }, { "description", "Select all and replace the existing text." } } }, { "enter", { { "type", "boolean" }, { "description", "Press Enter afterwards." } } } },
            json::array( { "text" } ) );
        add( "mouse", "Low-level mouse at UI coordinates: mouse_move, mouse_click, mouse_down, mouse_up, wheel.",
            { { "action", { { "type", "string" }, { "enum", { "mouse_move", "mouse_click", "mouse_down", "mouse_up", "wheel" } } } }, { "x", { { "type", "number" } } }, { "y", { { "type", "number" } } }, { "button", { { "type", "string" }, { "enum", { "left", "right", "middle" } } } }, { "count", { { "type", "integer" } } }, { "dx", { { "type", "number" } } }, { "dy", { { "type", "number" } } } },
            json::array( { "action" } ) );
        add( "run_steps",
            "Run a sequence of steps in one call. Each step is an object with 'action' and its arguments: "
            "click{ref,button,double}, hover{ref}, hold{ref,seconds}, open{ref}, close{ref}, check{ref}, uncheck{ref}, focus{ref}, scroll_to{ref}, "
            "key{keys,count}, key_down{keys}, key_up{keys}, type{text,ref,replace,enter}, mouse_move{x,y}, mouse_click{x,y,button,count}, mouse_down, mouse_up, wheel{dx,dy}, "
            "wait{frames|seconds}, close_popups, input_mode{mode:mouse|keyboard}. Add \"state\":true to a step to record the keyboard focus after it.",
            { { "steps", { { "type", "array" }, { "items", { { "type", "object" } } } } }, { "window", { { "type", "string" }, { "description", window_desc } } }, { "keyboard", { { "type", "boolean" }, { "description", "Drive item actions with the keyboard (nav) instead of the mouse." } } }, { "speed", { { "type", "string" }, { "description", speed_desc } } } }, json::array( { "steps" } ) );
        add( "run_tests", "Run the registered ImFluent regression tests (Dear ImGui Test Engine) and report each result.", { { "filter", { { "type", "string" }, { "description", "Test Engine filter, e.g. \"keyboard\", \"^nav_\", \"-slow\". Default: all." } } }, { "speed", { { "type", "string" }, { "description", speed_desc } } } }, json::array() );
        return tools;
    }

    // [SECTION] Main-thread job processing

    json RunImmediateTool( Job & job, bool & keep_waiting )
    {
        keep_waiting     = false;
        ImGuiContext & g = *GImGui;
        const json & a   = job.Args;
        if ( job.Tool == "app_info" )
        {
            const char * themes[]    = { "light", "dark", "highcontrast" };
            json j                   = json::object();
            j[ "app" ]               = "ImFluentGallery";
            j[ "imgui" ]             = IMGUI_VERSION;
            j[ "display_size" ]      = json::array( { ( int )g.IO.DisplaySize.x, ( int )g.IO.DisplaySize.y } );
            j[ "framebuffer_scale" ] = g.IO.DisplayFramebufferScale.x;
            j[ "dpi_scale" ]         = g.Style.FontScaleDpi;
            j[ "theme" ]             = themes[ ImClamp( ( int )ImFluent::GetThemePreset(), 0, 2 ) ];
            j[ "page" ]              = ImFluent::GetDemoWindowPage();
            j[ "frame" ]             = g.FrameCount;
            j[ "fps" ]               = g.IO.Framerate;
            j[ "root_window" ]       = g_Auto.Config.RootWindow;
            return ToolText( j );
        }
        if ( job.Tool == "list_pages" )
        {
            json pages = json::array();
            for ( int i = 0; ImFluent::GetDemoWindowPageId( i ); ++i )
                pages.push_back( ImFluent::GetDemoWindowPageId( i ) );
            return ToolText( { { "current", ImFluent::GetDemoWindowPage() }, { "pages", pages } } );
        }
        if ( job.Tool == "goto_page" )
        {
            if ( !job.Started )
            {
                const std::string page = ResolvePage( a.value( "page", std::string() ) );
                if ( page.empty() )
                    return ToolError( "goto_page: unknown page '" + a.value( "page", std::string() ) + "' (see list_pages)" );
                ImFluent::SetDemoWindowPage( page.c_str() );
                job.FramesLeft = 3;
            }
            if ( job.FramesLeft-- > 0 )
            {
                keep_waiting = true;
                return json();
            }
            return ToolText( { { "page", ImFluent::GetDemoWindowPage() } } );
        }
        if ( job.Tool == "set_theme" )
        {
            if ( !job.Started )
            {
                const std::string t = a.value( "theme", std::string() );
                if ( t == "light" )
                    ImFluent::SetThemePreset( ImFluentThemePreset_Light );
                else if ( t == "dark" )
                    ImFluent::SetThemePreset( ImFluentThemePreset_Dark );
                else if ( t == "highcontrast" )
                    ImFluent::SetThemePreset( ImFluentThemePreset_HighContrast );
                else
                    return ToolError( "set_theme: theme must be light, dark or highcontrast" );
                job.FramesLeft = 2;
            }
            if ( job.FramesLeft-- > 0 )
            {
                keep_waiting = true;
                return json();
            }
            return ToolText( { { "theme", a.value( "theme", std::string() ) } } );
        }
        if ( job.Tool == "resize_window" )
        {
            if ( !g_Auto.Host.SetWindowSize )
                return ToolError( "resize_window: not supported by the host" );
            if ( !job.Started )
            {
                g_Auto.Host.SetWindowSize( a.value( "width", 1280 ), a.value( "height", 800 ), g_Auto.Host.UserData );
                job.FramesLeft = 4;
            }
            if ( job.FramesLeft-- > 0 )
            {
                keep_waiting = true;
                return json();
            }
            return ToolText( { { "display_size", json::array( { ( int )g.IO.DisplaySize.x, ( int )g.IO.DisplaySize.y } ) } } );
        }
        if ( job.Tool == "screenshot" )
            return TakeScreenshot( a );
        if ( job.Tool == "list_windows" )
            return ToolText( ListWindows( a.value( "include_hidden", false ) ) );
        if ( job.Tool == "nav_state" )
            return ToolText( NavState() );
        return ToolError( "unknown tool '" + job.Tool + "'" );
    }

    json BuildTestsReport()
    {
        ImVector< ImGuiTest * > tests;
        ImGuiTestEngine_GetTestList( g_Auto.Engine, &tests );
        json results = json::array();
        int passed = 0, failed = 0;
        for ( ImGuiTest * t : tests )
        {
            if ( t == g_Auto.ExecTest || t->Output.Status == ImGuiTestStatus_Unknown || t->Output.Status == ImGuiTestStatus_Queued )
                continue;
            json r        = json::object();
            r[ "test" ]   = std::string( t->Category ) + "/" + t->Name;
            const bool ok = t->Output.Status == ImGuiTestStatus_Success;
            r[ "status" ] = ok ? "pass" : "fail";
            r[ "ms" ]     = ( double )( t->Output.EndTime - t->Output.StartTime ) / 1000.0;
            if ( !ok )
                r[ "log" ] = CollectLog( t, ImGuiTestVerboseLevel_Warning );
            ( ok ? passed : failed )++;
            results.push_back( r );
        }
        return { { "passed", passed }, { "failed", failed }, { "results", results } };
    }

    void ResetTestStatuses()
    {
        ImVector< ImGuiTest * > tests;
        ImGuiTestEngine_GetTestList( g_Auto.Engine, &tests );
        for ( ImGuiTest * t : tests )
            t->Output.Status = ImGuiTestStatus_Unknown;
    }

    void StartEngineJob( Job & job )
    {
        SetRunSpeed( job.Args );
        g_Auto.EngineResult = json();
        if ( job.Tool == "run_tests" )
        {
            ResetTestStatuses();
            const std::string filter = job.Args.value( "filter", std::string() );
            ImGuiTestEngine_QueueTests( g_Auto.Engine, ImGuiTestGroup_Tests, filter.empty() ? nullptr : filter.c_str(), ImGuiTestRunFlags_RunFromGui );
        }
        else
            ImGuiTestEngine_QueueTest( g_Auto.Engine, g_Auto.ExecTest, ImGuiTestRunFlags_RunFromGui );
        g_Auto.EngineQueued = true;
    }

    json FinishEngineJob( Job & job )
    {
        g_Auto.EngineQueued = false;
        if ( job.Tool == "run_tests" )
            return ToolText( BuildTestsReport() );
        ImGuiTest * t = g_Auto.ExecTest;
        json result   = g_Auto.EngineResult.is_null() ? json::object() : g_Auto.EngineResult;
        if ( t->Output.Status != ImGuiTestStatus_Success )
        {
            result[ "error" ] = CollectLog( t, ImGuiTestVerboseLevel_Warning );
            json r            = ToolText( result );
            r[ "isError" ]    = true;
            return r;
        }
        return ToolText( result );
    }

    void ProcessJobs()
    {
        for ( int guard = 0; guard < 16; ++guard )
        {
            if ( !g_Auto.Current )
            {
                std::lock_guard< std::mutex > lock( g_Auto.QueueMutex );
                if ( g_Auto.Queue.empty() )
                    return;
                g_Auto.Current = g_Auto.Queue.front();
                g_Auto.Queue.pop_front();
            }
            Job & job = *g_Auto.Current;
            if ( IsEngineTool( job.Tool ) )
            {
                if ( !job.Started )
                {
                    if ( !ImGuiTestEngine_IsTestQueueEmpty( g_Auto.Engine ) )
                        return; // The Test Engine is busy (e.g. a --run-tests session)
                    StartEngineJob( job );
                    job.Started = true;
                    return;
                }
                if ( !ImGuiTestEngine_IsTestQueueEmpty( g_Auto.Engine ) )
                    return;
                job.Finish( FinishEngineJob( job ) );
            }
            else
            {
                bool keep_waiting = false;
                json r            = RunImmediateTool( job, keep_waiting );
                job.Started       = true;
                if ( keep_waiting )
                    return;
                job.Finish( r );
            }
            g_Auto.Current.reset();
        }
    }

    // [SECTION] JSON-RPC (HTTP thread)

    json RpcResult( const json & id, const json & result )
    {
        return { { "jsonrpc", "2.0" }, { "id", id }, { "result", result } };
    }

    json RpcError( const json & id, int code, const std::string & message )
    {
        return { { "jsonrpc", "2.0" }, { "id", id }, { "error", { { "code", code }, { "message", message } } } };
    }

    json CallTool( const std::string & name, const json & args )
    {
        static const json tools = ToolDefinitions();
        bool known              = false;
        for ( const json & t : tools )
            if ( t[ "name" ] == name )
                known = true;
        if ( !known )
            return json();

        JobPtr job              = std::make_shared< Job >();
        job->Tool               = name;
        job->Args               = args.is_object() ? args : json::object();
        std::future< json > fut = job->Promise.get_future();
        {
            std::lock_guard< std::mutex > lock( g_Auto.QueueMutex );
            if ( g_Auto.ShuttingDown )
                return ToolError( "the application is shutting down" );
            g_Auto.Queue.push_back( job );
        }
        const int timeout_s = ( name == "run_tests" ) ? 1800 : 120;
        if ( fut.wait_for( std::chrono::seconds( timeout_s ) ) != std::future_status::ready )
            return ToolError( name + ": timed out (is the gallery window minimized?)" );
        return fut.get();
    }

    json HandleRpc( const json & msg )
    {
        if ( !msg.is_object() )
            return RpcError( nullptr, -32600, "Invalid Request" );
        if ( !msg.contains( "method" ) || !msg[ "method" ].is_string() )
            return json(); // A response from the client: nothing to answer
        if ( !msg.contains( "id" ) )
            return json(); // Notification
        const json id            = msg[ "id" ];
        const std::string method = msg[ "method" ];
        const json params        = msg.value( "params", json::object() );

        if ( method == "initialize" )
        {
            static const char * supported[] = { "2025-06-18", "2025-03-26", "2024-11-05" };
            std::string version             = supported[ 0 ];
            const std::string requested     = params.value( "protocolVersion", std::string() );
            for ( const char * v : supported )
                if ( requested == v )
                    version = v;
            json result                 = json::object();
            result[ "protocolVersion" ] = version;
            result[ "capabilities" ]    = { { "tools", { { "listChanged", false } } } };
            result[ "serverInfo" ]      = { { "name", kServerName }, { "version", kServerVersion } };
            result[ "instructions" ]    = "Drives ImFluentGallery (Dear ImGui + ImFluent) through the Dear ImGui Test Engine. "
                                          "Typical loop: goto_page -> screenshot -> list_items -> click/key/type_text -> nav_state/screenshot. "
                                          "Use key with \"Tab\"/\"Shift+Tab\"/arrows/\"Enter\"/\"Escape\" to check keyboard-only navigation; each action returns the focused item.";
            return RpcResult( id, result );
        }
        if ( method == "ping" )
            return RpcResult( id, json::object() );
        if ( method == "tools/list" )
            return RpcResult( id, { { "tools", ToolDefinitions() } } );
        if ( method == "tools/call" )
        {
            const std::string name = params.value( "name", std::string() );
            json result            = CallTool( name, params.value( "arguments", json::object() ) );
            if ( result.is_null() )
                return RpcError( id, -32602, "Unknown tool: " + name );
            return RpcResult( id, result );
        }
        if ( method == "resources/list" )
            return RpcResult( id, { { "resources", json::array() } } );
        if ( method == "prompts/list" )
            return RpcResult( id, { { "prompts", json::array() } } );
        return RpcError( id, -32601, "Method not found: " + method );
    }

    std::string HandleHttpBody( const std::string & body )
    {
        const json msg = json::parse( body, nullptr, false );
        if ( msg.is_discarded() )
            return RpcError( nullptr, -32700, "Parse error" ).dump();
        if ( msg.is_array() )
        {
            json out = json::array();
            for ( const json & m : msg )
            {
                json r = HandleRpc( m );
                if ( !r.is_null() )
                    out.push_back( r );
            }
            return out.empty() ? std::string() : out.dump();
        }
        const json r = HandleRpc( msg );
        return r.is_null() ? std::string() : r.dump();
    }

    bool CaptureForEngine( ImGuiID, int x, int y, int w, int h, unsigned int * pixels, void * )
    {
        return g_Auto.Host.CaptureFramebuffer && g_Auto.Host.CaptureFramebuffer( x, y, w, h, pixels, g_Auto.Host.UserData );
    }
} // namespace

// [SECTION] Public API

bool ImFluentAutomation::Init( const ImFluentAutomationConfig & config, const ImFluentAutomationHost & host )
{
    g_Auto.Config = config;
    g_Auto.Host   = host;

    g_Auto.Engine                   = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO & io          = ImGuiTestEngine_GetIO( g_Auto.Engine );
    io.ConfigVerboseLevel           = ImGuiTestVerboseLevel_Info;
    io.ConfigVerboseLevelOnError    = ImGuiTestVerboseLevel_Debug;
    io.ConfigRunSpeed               = config.NormalSpeed ? ImGuiTestRunSpeed_Normal : ImGuiTestRunSpeed_Fast;
    io.ConfigRestoreFocusAfterTests = false; // Keep the UI state an MCP action left behind
    io.ConfigLogToTTY               = config.RunTests;
    io.ConfigSavedSettings          = false;
    io.ScreenCaptureFunc            = CaptureForEngine;
    if ( config.RunTests && config.ExportJUnit )
    {
        io.ExportResultsFilename = config.ExportJUnit;
        io.ExportResultsFormat   = ImGuiTestEngineExportFormat_JUnitXml;
    }

    g_Auto.ExecTest        = IM_REGISTER_TEST( g_Auto.Engine, "mcp", "exec" );
    g_Auto.ExecTest->Group = ImGuiTestGroup_Unknown; // Never picked up by QueueTests(ImGuiTestGroup_Tests)
    g_Auto.ExecTest->Flags |= ImGuiTestFlags_NoGuiWarmUp;
    g_Auto.ExecTest->TestFunc = ExecTestFunc;
    ImFluentTests_RegisterAll( g_Auto.Engine );

    ImGuiTestEngine_Start( g_Auto.Engine, ImGui::GetCurrentContext() );
    ImGuiTestEngine_InstallDefaultCrashHandler();

    if ( config.McpPort > 0 )
    {
        if ( g_Auto.Server.Start( config.McpPort, "/mcp", HandleHttpBody ) )
            fprintf( stdout, "[imfluent] MCP server listening on http://127.0.0.1:%d/mcp\n", config.McpPort );
        else
            fprintf( stderr, "[imfluent] MCP server could not bind 127.0.0.1:%d\n", config.McpPort );
        fflush( stdout );
    }
    return true;
}

void ImFluentAutomation::ShowWindows()
{
    if ( g_Auto.Config.ShowEngineUi && g_Auto.Engine )
        ImGuiTestEngine_ShowTestEngineWindows( g_Auto.Engine, &g_Auto.Config.ShowEngineUi );
}

void ImFluentAutomation::PostSwap()
{
    if ( !g_Auto.Engine )
        return;
    ImGuiTestEngine_PostSwap( g_Auto.Engine );
    ++g_Auto.FrameCounter;

    if ( g_Auto.Config.RunTests && !g_Auto.CliTestsQueued && g_Auto.FrameCounter > 2 )
    {
        ImGuiTestEngine_QueueTests( g_Auto.Engine, ImGuiTestGroup_Tests, g_Auto.Config.TestFilter, ImGuiTestRunFlags_RunFromCommandLine );
        g_Auto.CliTestsQueued = true;
    }
    else if ( g_Auto.CliTestsQueued && !g_Auto.CliTestsDone && ImGuiTestEngine_IsTestQueueEmpty( g_Auto.Engine ) )
    {
        const json report = BuildTestsReport();
        for ( const json & r : report[ "results" ] )
            fprintf( stdout, "[%s] %s\n", r[ "status" ].get< std::string >().c_str(), r[ "test" ].get< std::string >().c_str() );
        fprintf( stdout, "[imfluent] tests: %d passed, %d failed\n", report[ "passed" ].get< int >(), report[ "failed" ].get< int >() );
        fflush( stdout );
        g_Auto.CliExitCode  = report[ "failed" ].get< int >();
        g_Auto.CliTestsDone = true;
    }

    ProcessJobs();
}

bool ImFluentAutomation::WantsMaxSpeed()
{
    return g_Auto.Engine && ImGuiTestEngine_GetIO( g_Auto.Engine ).IsRequestingMaxAppSpeed;
}

bool ImFluentAutomation::WantsExit( int * exit_code )
{
    if ( !g_Auto.CliTestsDone )
        return false;
    if ( exit_code )
        *exit_code = g_Auto.CliExitCode;
    return true;
}

void ImFluentAutomation::Stop()
{
    {
        std::lock_guard< std::mutex > lock( g_Auto.QueueMutex );
        g_Auto.ShuttingDown = true;
        for ( JobPtr & job : g_Auto.Queue )
            job->Finish( ToolError( "the application is shutting down" ) );
        g_Auto.Queue.clear();
    }
    if ( g_Auto.Current )
    {
        g_Auto.Current->Finish( ToolError( "the application is shutting down" ) );
        g_Auto.Current.reset();
    }
    g_Auto.Server.Stop();
    if ( g_Auto.Engine )
        ImGuiTestEngine_Stop( g_Auto.Engine );
}

void ImFluentAutomation::Shutdown()
{
    if ( g_Auto.Engine )
        ImGuiTestEngine_DestroyContext( g_Auto.Engine );
    g_Auto.Engine = nullptr;
}

ImGuiTestEngine * ImFluentAutomation::GetEngine()
{
    return g_Auto.Engine;
}