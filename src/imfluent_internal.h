#pragma once

#include "imfluent.h"

namespace ImFluent
{
    IMGUI_API ImU32             GetColorU32( ImFluentCol idx, float alpha_mul = 1.0f );
    IMGUI_API const ImVec4 &    GetStyleColorVec4( ImFluentCol idx );

    IMGUI_API void              SetFont( ImFluentTextStyle style, ImFont * font );
    IMGUI_API ImFont *          GetFont( ImFluentTextStyle style );

    IMGUI_API const char *      LocalizeGetMsg( ImFluentLocKey key );
    IMGUI_API void              LocalizeRegisterEntries( const ImFluentLocEntry * entries, int count );

    IMGUI_API float             FluentDpx( float v );
    IMGUI_API ImVec2            FluentDpx( const ImVec2 & v );
}
