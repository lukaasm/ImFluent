#include "imfluent_backdrop_osx.h"

#include "../imfluent.h"

#include <imgui.h>

#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>

#include <string>

namespace
{
    const int kMaxDecodeSize = 512;

    struct OSXBackdropState
    {
        ImVector< unsigned char > Pixels;
        int Width                = 0;
        int Height               = 0;
        int ImageW               = 0; // Before the downscale
        int ImageH               = 0;
        std::string Path;
        double WriteTime         = 0.0;
        ImFluentWallpaperFit Fit = ImFluentWallpaperFit_Fill;
        ImU32 Background         = IM_COL32( 0, 0, 0, 255 );
        ImVec4 Monitor;
        unsigned int Serial       = 0;
        unsigned char Solid[ 4 ] = { 0, 0, 0, 255 };
    };
    OSXBackdropState g_OSXBackdrop;

    NSString * const kEffectViewId = @"ImFluentBackdrop";

    NSWindow * ViewportWindow( ImGuiViewport * vp )
    {
        return vp ? ( __bridge NSWindow * )vp->PlatformHandleRaw : nil;
    }

    NSScreen * WindowScreen( NSWindow * window )
    {
        return ( window && window.screen ) ? window.screen : NSScreen.mainScreen;
    }

    // Cocoa screen coordinates grow upward from the bottom of the first screen: flip them to top-down.
    ImVec4 TopDownRect( NSRect r )
    {
        NSArray< NSScreen * > * screens = NSScreen.screens;
        const CGFloat h                  = screens.count ? screens[ 0 ].frame.size.height : 0.0;
        return ImVec4( ( float )r.origin.x, ( float )( h - ( r.origin.y + r.size.height ) ), ( float )( r.origin.x + r.size.width ), ( float )( h - r.origin.y ) );
    }

    bool DecodeImage( NSURL * url, OSXBackdropState & st )
    {
        CGImageSourceRef src = CGImageSourceCreateWithURL( ( __bridge CFURLRef )url, NULL );
        if ( !src )
            return false;
        NSDictionary * opts = @{
            ( __bridge id )kCGImageSourceCreateThumbnailFromImageAlways : @YES,
            ( __bridge id )kCGImageSourceCreateThumbnailWithTransform : @YES,
            ( __bridge id )kCGImageSourceThumbnailMaxPixelSize : @( kMaxDecodeSize ),
        };
        int iw = 0, ih = 0;
        if ( CFDictionaryRef props = CGImageSourceCopyPropertiesAtIndex( src, 0, NULL ) )
        {
            if ( CFNumberRef w = ( CFNumberRef )CFDictionaryGetValue( props, kCGImagePropertyPixelWidth ) )
                CFNumberGetValue( w, kCFNumberIntType, &iw );
            if ( CFNumberRef h = ( CFNumberRef )CFDictionaryGetValue( props, kCGImagePropertyPixelHeight ) )
                CFNumberGetValue( h, kCFNumberIntType, &ih );
            CFRelease( props );
        }
        CGImageRef img = CGImageSourceCreateThumbnailAtIndex( src, 0, ( __bridge CFDictionaryRef )opts );
        CFRelease( src );
        if ( !img )
            return false;

        bool ok        = false;
        const size_t w = CGImageGetWidth( img ), h = CGImageGetHeight( img );
        st.Pixels.resize( ( int )( w * h * 4 ) );
        CGColorSpaceRef cs = CGColorSpaceCreateWithName( kCGColorSpaceSRGB );
        CGContextRef ctx   = CGBitmapContextCreate( st.Pixels.Data, w, h, 8, w * 4, cs, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big );
        if ( ctx && w > 0 && h > 0 )
        {
            CGContextDrawImage( ctx, CGRectMake( 0, 0, w, h ), img );
            // The thumbnail is upright (EXIF orientation applied): scale its size back to the full image.
            const double big   = ( double )( iw > ih ? iw : ih );
            const double small = ( double )( w > h ? w : h );
            const double k     = ( big > small ) ? big / small : 1.0;
            st.Width           = ( int )w;
            st.Height          = ( int )h;
            st.ImageW          = ( int )( w * k + 0.5 );
            st.ImageH          = ( int )( h * k + 0.5 );
            ok                 = true;
        }
        if ( ctx )
            CGContextRelease( ctx );
        CGColorSpaceRelease( cs );
        CGImageRelease( img );
        if ( !ok )
            st.Pixels.clear();
        return ok;
    }

    bool GetWallpaper( ImGuiViewport * vp, ImFluentWallpaper * out, void * )
    {
        @autoreleasepool
        {
            OSXBackdropState & st = g_OSXBackdrop;
            NSScreen * screen     = WindowScreen( ViewportWindow( vp ) );
            if ( !screen )
                return false;
            NSWorkspace * ws    = NSWorkspace.sharedWorkspace;
            NSURL * url         = [ws desktopImageURLForScreen:screen];
            NSDictionary * opts = [ws desktopImageOptionsForScreen:screen];

            NSNumber * scaling       = opts[ NSWorkspaceDesktopImageScalingKey ];
            NSNumber * clipping      = opts[ NSWorkspaceDesktopImageAllowClippingKey ];
            const NSImageScaling s   = scaling ? ( NSImageScaling )scaling.integerValue : NSImageScaleProportionallyUpOrDown;
            ImFluentWallpaperFit fit = ImFluentWallpaperFit_Fill;
            if ( s == NSImageScaleAxesIndependently )
                fit = ImFluentWallpaperFit_Stretch;
            else if ( s == NSImageScaleNone )
                fit = ImFluentWallpaperFit_Center;
            else if ( clipping && !clipping.boolValue )
                fit = ImFluentWallpaperFit_Fit;

            ImU32 color = IM_COL32( 0, 0, 0, 255 );
            NSColor * fill = [opts[ NSWorkspaceDesktopImageFillColorKey ] colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
            if ( fill )
                color = IM_COL32( ( int )( fill.redComponent * 255.0 + 0.5 ), ( int )( fill.greenComponent * 255.0 + 0.5 ), ( int )( fill.blueComponent * 255.0 + 0.5 ), 255 );

            const char * utf8    = ( url && url.isFileURL && url.path ) ? url.path.UTF8String : NULL;
            std::string path     = utf8 ? utf8 : "";
            NSDictionary * attrs = utf8 ? [NSFileManager.defaultManager attributesOfItemAtPath:url.path error:nil] : nil;
            const bool has_file  = attrs && ![attrs[ NSFileType ] isEqualToString:NSFileTypeDirectory];
            const double mtime   = has_file ? [( NSDate * )attrs[ NSFileModificationDate ] timeIntervalSince1970] : 0.0;
            const ImVec4 monitor = TopDownRect( screen.frame );

            const bool same_file   = has_file && path == st.Path && mtime == st.WriteTime && !st.Pixels.empty();
            const bool same_screen = monitor.x == st.Monitor.x && monitor.y == st.Monitor.y && monitor.z == st.Monitor.z && monitor.w == st.Monitor.w;
            if ( !same_file || fit != st.Fit || color != st.Background || !same_screen )
            {
                if ( !same_file )
                {
                    st.Pixels.clear();
                    st.Path      = path;
                    st.WriteTime = mtime;
                    if ( has_file )
                        DecodeImage( url, st );
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
    }

    bool GetScreenRects( ImGuiViewport * vp, ImVec4 * client, ImVec4 * monitor, void * )
    {
        @autoreleasepool
        {
            NSWindow * window = ViewportWindow( vp );
            NSScreen * screen = WindowScreen( window );
            if ( !window || !screen )
                return false;
            *client  = TopDownRect( [window contentRectForFrameRect:window.frame] );
            *monitor = TopDownRect( screen.frame );
            return true;
        }
    }

    bool SetNativeBackdrop( ImGuiViewport * vp, ImFluentBackdrop backdrop, bool dark, void * )
    {
        @autoreleasepool
        {
            NSWindow * window = ViewportWindow( vp );
            NSView * content  = window.contentView;
            if ( !content )
                return false;
            NSVisualEffectView * effect = nil;
            for ( NSView * v in content.subviews )
                if ( [v.identifier isEqualToString:kEffectViewId] )
                    effect = ( NSVisualEffectView * )v;
            if ( backdrop == ImFluentBackdrop_None )
            {
                [effect removeFromSuperview];
                window.opaque = YES;
                return false;
            }
            if ( @available( macOS 10.14, * ) )
            {
                if ( !effect )
                {
                    effect                  = [[NSVisualEffectView alloc] initWithFrame:content.bounds];
                    effect.identifier       = kEffectViewId;
                    effect.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
                    effect.blendingMode     = NSVisualEffectBlendingModeBehindWindow;
                    effect.state            = NSVisualEffectStateFollowsWindowActiveState;
                    [content addSubview:effect positioned:NSWindowBelow relativeTo:nil];
#if !__has_feature( objc_arc )
                    [effect release]; // The superview keeps it
#endif
                }
                effect.material        = backdrop == ImFluentBackdrop_MicaAlt ? NSVisualEffectMaterialSidebar : NSVisualEffectMaterialUnderWindowBackground;
                effect.appearance      = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
                window.opaque          = NO;
                window.backgroundColor = NSColor.clearColor;
                return true;
            }
            return false;
        }
    }
} // namespace

void ImFluent_ImplOSX_InstallBackdropHooks( bool allow_native_backdrop )
{
    ImFluentBackdropHooks & hooks = ImFluent::GetBackdropHooks();
    hooks.GetWallpaper            = &GetWallpaper;
    hooks.GetScreenRects          = &GetScreenRects;
    hooks.SetNativeBackdrop       = allow_native_backdrop ? &SetNativeBackdrop : NULL;
}

void ImFluent_ImplOSX_UninstallBackdropHooks()
{
    ImFluent::GetBackdropHooks() = ImFluentBackdropHooks();
    g_OSXBackdrop.Pixels.clear();
}
