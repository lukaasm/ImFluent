#pragma once

#include "imfluent.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace ImFluentInternal
{

inline ImU32 LerpColorU32(ImU32 a, ImU32 b, float t)
{
    if (t <= 0.f) return a;
    if (t >= 1.f) return b;
    const int t8 = (int)(t * 256.f);
    const int it = 256 - t8;
    const ImU32 ar = (a >>  0) & 0xFF, br = (b >>  0) & 0xFF;
    const ImU32 ag = (a >>  8) & 0xFF, bg = (b >>  8) & 0xFF;
    const ImU32 ab = (a >> 16) & 0xFF, bb = (b >> 16) & 0xFF;
    const ImU32 aa = (a >> 24) & 0xFF, ba = (b >> 24) & 0xFF;
    return  (ImU32)((ar * it + br * t8) >> 8)
         | ((ImU32)((ag * it + bg * t8) >> 8) <<  8)
         | ((ImU32)((ab * it + bb * t8) >> 8) << 16)
         | ((ImU32)((aa * it + ba * t8) >> 8) << 24);
}

inline ImU32 AlphaMulU32(ImU32 c, float a)
{
    if (a >= 1.f) return c;
    if (a <= 0.f) return c & ~IM_COL32_A_MASK;
    const ImU32 alpha = (ImU32)((float)((c >> IM_COL32_A_SHIFT) & 0xFF) * a);
    return (c & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
}

ImU32 AnimateColorU32(ImGuiID id, ImU32 target, float seconds = 0.083f);
float AnimateFloat   (ImGuiID id, float target, float seconds = 0.20f);

void DrawFocusRing(ImDrawList* dl, const ImRect& bb, float rounding);

void DrawElevationBorder(ImDrawList* dl, const ImRect& bb, float rounding,
                         ImU32 sides, ImU32 bottom, float bottom_thickness);

void DrawElevationShadow(ImDrawList* dl, const ImRect& bb, float rounding, int layers);

void DrawAcrylicSurrogate(ImDrawList* dl, const ImRect& bb, float rounding, ImU32 base);

ImU32 ResolveControlFillState   (bool disabled, bool held, bool hovered);

ImU32 ResolveAccentFillState    (bool disabled, bool held, bool hovered);

ImU32 ResolveSubtleFillState    (bool selected, bool held, bool hovered);

ImU32 ResolveControlAltFillState(bool disabled, bool held, bool hovered);

void DrawChevron(ImDrawList* dl, ImVec2 center, ImGuiDir dir, ImU32 col, float length_dpx, float thickness = 1.5f);

void DrawSelectionIndicator(ImDrawList* dl, const ImRect& bb, ImGuiDir side, ImU32 col, float length_frac = 0.5f);

void PushControlFrameStyle(float h);
void PopControlFrameStyle ();

void PushOverlayWindowStyle(float padding_x, float padding_y);
void PopOverlayWindowStyle ();

void DrawAndConsumePendingHeader();
void DrawAndConsumePendingDescription();
const char* ConsumePendingGlyph(const char* fallback);

}
