#include "imfluent.h"
#include "imfluent_icons.h"
#include "imfluent_internal.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <math.h>
#include <stdarg.h>

#if defined (_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include "windows.h"
#endif

// [SECTION] ImFluentContext (all mutable globals)

struct ImFluentNextItemData
{
    const char * Header;
    const char * Description;
    const char * Glyph;
    const char * Error;
    bool        HasHeader;
    bool        HasDescription;
    bool        HasGlyph;
    bool        HasError;
};

struct ImFluentColorStackEntry { ImFluentCol Idx; ImVec4 Prev; };

enum ImFluentStyleVarKind { ImFluentStyleVarKind_Float, ImFluentStyleVarKind_Vec2 };
struct ImFluentStyleVarStackEntry { ImFluentStyleVar Idx; ImFluentStyleVarKind Kind; ImVec2 Prev; };

struct ImFluentWrapPanelState { float Avail; float HSpacing; float VSpacing; float CursorX; bool First; };

struct ImFluentExpanderState
{
    char                    Label[128];
    bool *                  Open;
    ImFluentExpandDirection Direction;
    bool                    BodyActive;
};

struct ImFluentNavViewState
{
    float               CurrentWidth;
    bool                ContentStarted;
    ImFluentNavViewMode Mode;
    ImFluentNavViewMode PrevMode;
    bool                NextToggleVisible;
    bool                NextToggleVisibleSet;
};

struct ImFluentPopupAnchor { ImGuiID Id; ImRect Rect; };

struct ImFluentCommandBarState
{
    char  Id[64];
    char  PopupId[80];
    float Height;
};

struct ImFluentContext
{
    static const int                            MaxPopupAnchors = 16;

    ImFluentThemePreset                         Preset;
    ImFluentStyle                               Style;
    ImFont *                                    Fonts[ImFluentTextStyle_COUNT];

    ImFluentNextItemData                        NextItem;

    ImVector<ImFluentColorStackEntry>           ColorStack;
    ImVector<ImFluentStyleVarStackEntry>        StyleVarStack;
    ImVector<ImFluentWrapPanelState>            WrapPanelStack;
    ImVector<ImFluentExpanderState>             ExpanderStack;
    ImVector<ImFluentCommandBarState>           CommandBarStack;
    ImVector<float>                             NavItemIndentStack;

    ImFluentNavViewState                        NavView;

    ImFluentPopupAnchor                         PopupAnchors[MaxPopupAnchors];
    int                                         PopupAnchorCount;

    ImFluentAppBarLabelPosition                 NextAppBarLabelPos;
    bool                                        NextAppBarLabelPosSet;

    bool                                        HasUserAccent;
    ImVec4                                      UserAccent;

    ImFluentContext()
        : Preset( ImFluentThemePreset_Dark )
        , PopupAnchorCount( 0 )
        , NextAppBarLabelPos( ImFluentAppBarLabelPosition_Bottom )
        , NextAppBarLabelPosSet( false )
        , HasUserAccent( false )
        , UserAccent( 0.f, 0.f, 0.f, 1.f )
    {
        for ( int i = 0; i < ImFluentTextStyle_COUNT; ++i ) Fonts[i] = NULL;
        NextItem = ImFluentNextItemData();
        NavView  = ImFluentNavViewState();
    }
};

static ImFluentContext g_Ctx;

// [SECTION] Internal helpers (palette, animation, draw primitives, focus, fills)

namespace ImFluent
{
    static void FillCommon( ImFluentStyle & s )
    {
        s.ControlCornerRadius = 4.f;
        s.OverlayCornerRadius = 8.f;
        s.ControlHeight = 32.f;
        s.ControlMinWidth = 120.f;
        s.FocusStrokeThicknessOuter = 2.f;
        s.FocusStrokeThicknessInner = 1.f;
        s.CardPadding = 16.f;
        s.NavPaneCompactWidth = 48.f;
        s.NavPaneOpenWidth = 320.f;
        s.ControlContentPadding = ImVec2( 11.f, 5.f );

        s.SpacingXSmall = 2.f;
        s.SpacingSmall = 4.f;
        s.SpacingMedium = 8.f;
        s.SpacingLarge = 12.f;
        s.SpacingXLarge = 16.f;
        s.SpacingXXLarge = 24.f;

        s.StrokeThin = 1.f;
        s.StrokeMedium = 1.5f;
        s.StrokeThick = 2.f;

        s.ChevronGlyphSize = 4.f;
        s.StandardIconSize = 16.f;

        s.CheckboxSize = 20.f;
        s.RadioButtonDiameter = 20.f;
        s.ToggleSwitchWidth = 40.f;
        s.ToggleSwitchHeight = 20.f;
        s.ToggleSwitchThumbRadiusOff = 6.f;
        s.ToggleSwitchThumbRadiusOn = 7.f;
        s.SliderTrackHeight = 4.f;
        s.SliderThumbRadius = 9.f;
        s.SliderThumbInnerRadius = 6.f;
        s.ProgressBarHeight = 4.f;
        s.ProgressRingThickness = 3.f;
        s.RatingStarSize = 20.f;

        s.NavItemHeight = 36.f;
        s.MenuItemHeight = 32.f;
        s.ListItemHeight = 40.f;
        s.TitleBarHeight = 40.f;
        s.AppBarButtonWidth = 64.f;
        s.AppBarButtonHeight = 56.f;
        s.SpinButtonWidth = 28.f;
        s.RevealButtonWidth = 32.f;
        s.BadgeHeight = 16.f;
        s.PipDotSize = 8.f;

        s.SeverityBarThickness = 4.f;
        s.SelectionIndicatorThickness = 3.f;
        s.SelectionIndicatorInset = 2.f;
        s.TextInputAccentLineThickness = 2.f;

        const char ** loc = s.LocalizationTable;
        loc[ImFluentLocKey_AutoSuggestNoSuggestions] = "No suggestions";
        loc[ImFluentLocKey_DatePickerPickADate] = "Pick a date";
        loc[ImFluentLocKey_DatePickerDayFormat] = "Day %d";
        loc[ImFluentLocKey_DatePickerYearFormat] = "Year %d";
        loc[ImFluentLocKey_TimePickerHourFormat] = "%02d h";
        loc[ImFluentLocKey_TimePickerMinuteFormat] = "%02d min";
        loc[ImFluentLocKey_MonthJanuary] = "January";
        loc[ImFluentLocKey_MonthFebruary] = "February";
        loc[ImFluentLocKey_MonthMarch] = "March";
        loc[ImFluentLocKey_MonthApril] = "April";
        loc[ImFluentLocKey_MonthMay] = "May";
        loc[ImFluentLocKey_MonthJune] = "June";
        loc[ImFluentLocKey_MonthJuly] = "July";
        loc[ImFluentLocKey_MonthAugust] = "August";
        loc[ImFluentLocKey_MonthSeptember] = "September";
        loc[ImFluentLocKey_MonthOctober] = "October";
        loc[ImFluentLocKey_MonthNovember] = "November";
        loc[ImFluentLocKey_MonthDecember] = "December";
    }

    static void BuildDarkPalette( ImFluentStyle & s )
    {
        FillCommon( s );
        ImVec4 * c = s.Colors;

        c[ImFluentCol_TextPrimary] = ImColor( 255, 255, 255, 228 );
        c[ImFluentCol_TextSecondary] = ImColor( 255, 255, 255, 200 );
        c[ImFluentCol_TextTertiary] = ImColor( 255, 255, 255, 144 );
        c[ImFluentCol_TextDisabled] = ImColor( 255, 255, 255, 93 );

        c[ImFluentCol_TextOnAccentPrimary] = ImColor( 0, 0, 0 );
        c[ImFluentCol_TextOnAccentSecondary] = ImColor( 0, 0, 0, 178 );
        c[ImFluentCol_TextOnAccentDisabled] = ImColor( 0, 0, 0, 87 );
        c[ImFluentCol_TextOnAccentSelected] = ImColor( 0, 0, 0 );
        c[ImFluentCol_AccentTextPrimary] = ImColor( 153, 235, 255 );
        c[ImFluentCol_AccentTextSecondary] = ImColor( 118, 215, 255 );
        c[ImFluentCol_AccentTextTertiary] = ImColor( 96, 205, 255 );
        c[ImFluentCol_AccentTextDisabled] = ImColor( 255, 255, 255, 93 );

        c[ImFluentCol_ControlFillDefault] = ImColor( 255, 255, 255, 18 );
        c[ImFluentCol_ControlFillSecondary] = ImColor( 255, 255, 255, 40 );
        c[ImFluentCol_ControlFillTertiary] = ImColor( 255, 255, 255, 8 );
        c[ImFluentCol_ControlFillQuarternary] = ImColor( 255, 255, 255, 76 );
        c[ImFluentCol_ControlFillDisabled] = ImColor( 255, 255, 255, 10 );
        c[ImFluentCol_ControlFillTransparent] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ControlFillInputActive] = ImColor( 30, 30, 30, 178 );
        c[ImFluentCol_ControlAltFillTransparent] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ControlAltFillSecondary] = ImColor( 0, 0, 0, 26 );
        c[ImFluentCol_ControlAltFillTertiary] = ImColor( 255, 255, 255, 10 );
        c[ImFluentCol_ControlAltFillQuarternary] = ImColor( 255, 255, 255, 18 );
        c[ImFluentCol_ControlAltFillDisabled] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ControlSolidFillDefault] = ImColor( 105, 105, 105 );
        c[ImFluentCol_ControlStrongFillDefault] = ImColor( 255, 255, 255, 138 );
        c[ImFluentCol_ControlStrongFillDisabled] = ImColor( 255, 255, 255, 93 );

        c[ImFluentCol_SubtleFillTransparent] = ImColor( 255, 255, 255, 0 );
        c[ImFluentCol_SubtleFillSecondary] = ImColor( 255, 255, 255, 30 );
        c[ImFluentCol_SubtleFillTertiary] = ImColor( 255, 255, 255, 18 );
        c[ImFluentCol_SubtleFillDisabled] = ImColor( 255, 255, 255, 0 );

        c[ImFluentCol_AccentFillDefault] = ImColor( 76, 194, 255 );
        c[ImFluentCol_AccentFillSecondary] = ImColor( 76, 194, 255, 230 );
        c[ImFluentCol_AccentFillTertiary] = ImColor( 76, 194, 255, 204 );
        c[ImFluentCol_AccentFillDisabled] = ImColor( 255, 255, 255, 40 );
        c[ImFluentCol_AccentFillSelectedTextBg] = ImColor( 0, 103, 192 );

        c[ImFluentCol_CardBgDefault] = ImColor( 255, 255, 255, 13 );
        c[ImFluentCol_CardBgSecondary] = ImColor( 255, 255, 255, 18 );
        c[ImFluentCol_LayerFillDefault] = ImColor( 58, 58, 58, 128 );
        c[ImFluentCol_LayerFillAlt] = ImColor( 44, 44, 44 );
        c[ImFluentCol_SmokeFill] = ImColor( 0, 0, 0, 76 );
        c[ImFluentCol_AcrylicFill] = ImColor( 44, 44, 44, 220 );
        c[ImFluentCol_SolidBgBase] = ImColor( 32, 32, 32 );
        c[ImFluentCol_SolidBgQuarternary] = ImColor( 44, 44, 44 );

        c[ImFluentCol_CardStrokeDefault] = ImColor( 255, 255, 255, 20 );
        c[ImFluentCol_CardStrokeSolid] = ImColor( 56, 56, 56 );
        c[ImFluentCol_ControlStrokeDefault] = ImColor( 255, 255, 255, 18 );
        c[ImFluentCol_ControlStrokeSecondary] = ImColor( 255, 255, 255, 41 );
        c[ImFluentCol_ControlStrokeOnAccentDefault] = ImColor( 255, 255, 255, 20 );
        c[ImFluentCol_ControlStrokeOnAccentSecondary] = ImColor( 0, 0, 0, 102 );
        c[ImFluentCol_ControlStrongStrokeDefault] = ImColor( 255, 255, 255, 138 );
        c[ImFluentCol_ControlStrongStrokeDisabled] = ImColor( 255, 255, 255, 56 );
        c[ImFluentCol_SurfaceStrokeDefault] = ImColor( 0, 0, 0, 102 );
        c[ImFluentCol_SurfaceStrokeFlyout] = ImColor( 0, 0, 0, 102 );
        c[ImFluentCol_DividerStrokeDefault] = ImColor( 255, 255, 255, 21 );

        c[ImFluentCol_FocusStrokeOuter] = ImColor( 255, 255, 255 );
        c[ImFluentCol_FocusStrokeInner] = ImColor( 28, 28, 28 );

        c[ImFluentCol_ElevationControlTop] = ImColor( 255, 255, 255, 20 );
        c[ImFluentCol_ElevationControlBottom] = ImColor( 0, 0, 0, 90 );
        c[ImFluentCol_ElevationTextControlTop] = ImColor( 255, 255, 255, 20 );
        c[ImFluentCol_ElevationTextControlBottom] = ImColor( 255, 255, 255, 138 );
        c[ImFluentCol_ElevationTextControlFocusedBottom] = ImColor( 76, 194, 255 );
        c[ImFluentCol_ElevationAccentTop] = ImColor( 255, 255, 255, 20 );
        c[ImFluentCol_ElevationAccentBottom] = ImColor( 0, 0, 0, 64 );

        c[ImFluentCol_SystemFillSuccess] = ImColor( 108, 203, 95 );
        c[ImFluentCol_SystemFillCaution] = ImColor( 252, 225, 0 );
        c[ImFluentCol_SystemFillCritical] = ImColor( 255, 153, 164 );
        c[ImFluentCol_SystemFillNeutral] = ImColor( 255, 255, 255, 138 );
        c[ImFluentCol_SystemFillAttention] = ImColor( 96, 205, 255 );
    }

    static void BuildLightPalette( ImFluentStyle & s )
    {
        BuildDarkPalette( s );
        ImVec4 * c = s.Colors;
        c[ImFluentCol_TextPrimary] = ImColor( 0, 0, 0, 228 );
        c[ImFluentCol_TextSecondary] = ImColor( 0, 0, 0, 158 );
        c[ImFluentCol_TextTertiary] = ImColor( 0, 0, 0, 114 );
        c[ImFluentCol_TextDisabled] = ImColor( 0, 0, 0, 92 );
        c[ImFluentCol_TextOnAccentPrimary] = ImColor( 255, 255, 255 );
        c[ImFluentCol_TextOnAccentSecondary] = ImColor( 255, 255, 255, 178 );
        c[ImFluentCol_TextOnAccentDisabled] = ImColor( 255, 255, 255 );
        c[ImFluentCol_TextOnAccentSelected] = ImColor( 255, 255, 255 );
        c[ImFluentCol_AccentTextPrimary] = ImColor( 0, 90, 158 );
        c[ImFluentCol_AccentTextSecondary] = ImColor( 0, 78, 140 );
        c[ImFluentCol_AccentTextTertiary] = ImColor( 0, 103, 192 );
        c[ImFluentCol_AccentTextDisabled] = ImColor( 0, 0, 0, 92 );

        c[ImFluentCol_ControlFillDefault] = ImColor( 255, 255, 255, 178 );
        c[ImFluentCol_ControlFillSecondary] = ImColor( 249, 249, 249, 128 );
        c[ImFluentCol_ControlFillTertiary] = ImColor( 249, 249, 249, 77 );
        c[ImFluentCol_ControlFillQuarternary] = ImColor( 249, 249, 249, 194 );
        c[ImFluentCol_ControlFillDisabled] = ImColor( 243, 243, 243, 178 );
        c[ImFluentCol_ControlFillInputActive] = ImColor( 255, 255, 255 );
        c[ImFluentCol_ControlAltFillTransparent] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ControlAltFillSecondary] = ImColor( 0, 0, 0, 6 );
        c[ImFluentCol_ControlAltFillTertiary] = ImColor( 0, 0, 0, 15 );
        c[ImFluentCol_ControlAltFillQuarternary] = ImColor( 0, 0, 0, 18 );
        c[ImFluentCol_ControlAltFillDisabled] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ControlSolidFillDefault] = ImColor( 255, 255, 255 );
        c[ImFluentCol_ControlStrongFillDefault] = ImColor( 0, 0, 0, 114 );
        c[ImFluentCol_ControlStrongFillDisabled] = ImColor( 0, 0, 0, 82 );

        c[ImFluentCol_SubtleFillTransparent] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_SubtleFillSecondary] = ImColor( 0, 0, 0, 10 );
        c[ImFluentCol_SubtleFillTertiary] = ImColor( 0, 0, 0, 6 );
        c[ImFluentCol_SubtleFillDisabled] = ImColor( 0, 0, 0, 0 );

        c[ImFluentCol_AccentFillDefault] = ImColor( 0, 103, 192 );
        c[ImFluentCol_AccentFillSecondary] = ImColor( 0, 103, 192, 230 );
        c[ImFluentCol_AccentFillTertiary] = ImColor( 0, 103, 192, 204 );
        c[ImFluentCol_AccentFillDisabled] = ImColor( 0, 0, 0, 56 );
        c[ImFluentCol_AccentFillSelectedTextBg] = ImColor( 0, 103, 192 );

        c[ImFluentCol_CardBgDefault] = ImColor( 255, 255, 255, 178 );
        c[ImFluentCol_CardBgSecondary] = ImColor( 246, 246, 246, 128 );
        c[ImFluentCol_LayerFillDefault] = ImColor( 255, 255, 255, 128 );
        c[ImFluentCol_LayerFillAlt] = ImColor( 255, 255, 255 );
        c[ImFluentCol_SmokeFill] = ImColor( 0, 0, 0, 76 );
        c[ImFluentCol_AcrylicFill] = ImColor( 252, 252, 252, 220 );
        c[ImFluentCol_SolidBgBase] = ImColor( 243, 243, 243 );
        c[ImFluentCol_SolidBgQuarternary] = ImColor( 238, 238, 238 );

        c[ImFluentCol_CardStrokeDefault] = ImColor( 0, 0, 0, 18 );
        c[ImFluentCol_CardStrokeSolid] = ImColor( 235, 235, 235 );
        c[ImFluentCol_ControlStrokeDefault] = ImColor( 0, 0, 0, 15 );
        c[ImFluentCol_ControlStrokeSecondary] = ImColor( 0, 0, 0, 41 );
        c[ImFluentCol_ControlStrokeOnAccentDefault] = ImColor( 255, 255, 255, 20 );
        c[ImFluentCol_ControlStrokeOnAccentSecondary] = ImColor( 0, 0, 0, 102 );
        c[ImFluentCol_ControlStrongStrokeDefault] = ImColor( 0, 0, 0, 114 );
        c[ImFluentCol_ControlStrongStrokeDisabled] = ImColor( 0, 0, 0, 56 );
        c[ImFluentCol_SurfaceStrokeDefault] = ImColor( 117, 117, 117, 102 );
        c[ImFluentCol_SurfaceStrokeFlyout] = ImColor( 0, 0, 0, 18 );
        c[ImFluentCol_DividerStrokeDefault] = ImColor( 0, 0, 0, 15 );

        c[ImFluentCol_FocusStrokeOuter] = ImColor( 0, 0, 0 );
        c[ImFluentCol_FocusStrokeInner] = ImColor( 255, 255, 255 );

        c[ImFluentCol_ElevationControlTop] = ImColor( 0, 0, 0, 15 );
        c[ImFluentCol_ElevationControlBottom] = ImColor( 0, 0, 0, 41 );
        c[ImFluentCol_ElevationTextControlTop] = ImColor( 0, 0, 0, 15 );
        c[ImFluentCol_ElevationTextControlBottom] = ImColor( 0, 0, 0, 114 );
        c[ImFluentCol_ElevationTextControlFocusedBottom] = ImColor( 0, 103, 192 );
        c[ImFluentCol_ElevationAccentTop] = ImColor( 255, 255, 255, 20 );
        c[ImFluentCol_ElevationAccentBottom] = ImColor( 0, 0, 0, 64 );
    }

    static void BuildHighContrastPalette( ImFluentStyle & s )
    {
        BuildDarkPalette( s );
        ImVec4 * c = s.Colors;

        c[ImFluentCol_TextPrimary] = ImColor( 255, 255, 255 );
        c[ImFluentCol_TextSecondary] = ImColor( 255, 255, 255 );
        c[ImFluentCol_TextTertiary] = ImColor( 255, 255, 255 );
        c[ImFluentCol_TextDisabled] = ImColor( 0, 255, 255 );
        c[ImFluentCol_TextOnAccentPrimary] = ImColor( 0, 0, 0 );
        c[ImFluentCol_TextOnAccentSecondary] = ImColor( 0, 0, 0 );
        c[ImFluentCol_AccentTextPrimary] = ImColor( 255, 255, 0 );
        c[ImFluentCol_AccentTextSecondary] = ImColor( 255, 255, 0 );
        c[ImFluentCol_AccentTextTertiary] = ImColor( 255, 255, 0 );
        c[ImFluentCol_AccentFillDefault] = ImColor( 255, 255, 0 );
        c[ImFluentCol_AccentFillSecondary] = ImColor( 255, 255, 0 );
        c[ImFluentCol_AccentFillTertiary] = ImColor( 255, 255, 0 );
        c[ImFluentCol_SolidBgBase] = ImColor( 0, 0, 0 );
        c[ImFluentCol_SolidBgQuarternary] = ImColor( 0, 0, 0 );
        c[ImFluentCol_LayerFillDefault] = ImColor( 0, 0, 0 );
        c[ImFluentCol_LayerFillAlt] = ImColor( 0, 0, 0 );
        c[ImFluentCol_CardBgDefault] = ImColor( 0, 0, 0 );
        c[ImFluentCol_CardBgSecondary] = ImColor( 0, 0, 0 );
        c[ImFluentCol_ControlFillDefault] = ImColor( 0, 0, 0 );
        c[ImFluentCol_ControlFillSecondary] = ImColor( 30, 30, 30 );
        c[ImFluentCol_ControlFillTertiary] = ImColor( 50, 50, 50 );
        c[ImFluentCol_ControlFillDisabled] = ImColor( 0, 0, 0 );
        c[ImFluentCol_SubtleFillSecondary] = ImColor( 30, 30, 30 );
        c[ImFluentCol_SubtleFillTertiary] = ImColor( 50, 50, 50 );
        c[ImFluentCol_ControlStrokeDefault] = ImColor( 255, 255, 255 );
        c[ImFluentCol_ControlStrokeSecondary] = ImColor( 255, 255, 255 );
        c[ImFluentCol_ControlStrongStrokeDefault] = ImColor( 255, 255, 255 );
        c[ImFluentCol_ControlStrongStrokeDisabled] = ImColor( 0, 255, 255 );
        c[ImFluentCol_ControlStrokeOnAccentDefault] = ImColor( 0, 0, 0 );
        c[ImFluentCol_ControlStrokeOnAccentSecondary] = ImColor( 0, 0, 0 );
        c[ImFluentCol_CardStrokeDefault] = ImColor( 255, 255, 255 );
        c[ImFluentCol_CardStrokeSolid] = ImColor( 255, 255, 255 );
        c[ImFluentCol_SurfaceStrokeDefault] = ImColor( 255, 255, 255 );
        c[ImFluentCol_SurfaceStrokeFlyout] = ImColor( 255, 255, 255 );
        c[ImFluentCol_DividerStrokeDefault] = ImColor( 255, 255, 255 );
        c[ImFluentCol_FocusStrokeOuter] = ImColor( 255, 255, 0 );
        c[ImFluentCol_FocusStrokeInner] = ImColor( 0, 0, 0 );

        c[ImFluentCol_AccentFillDisabled] = ImColor( 0, 255, 255 );
        c[ImFluentCol_ControlFillTransparent] = ImColor( 0, 0, 0 );
        c[ImFluentCol_ElevationControlTop] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ElevationControlBottom] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ElevationTextControlTop] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ElevationTextControlBottom] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ElevationTextControlFocusedBottom] = ImColor( 255, 255, 0 );
        c[ImFluentCol_ElevationAccentTop] = ImColor( 0, 0, 0, 0 );
        c[ImFluentCol_ElevationAccentBottom] = ImColor( 0, 0, 0, 0 );

        c[ImFluentCol_SystemFillSuccess] = ImColor( 0, 255, 0 );
        c[ImFluentCol_SystemFillCaution] = ImColor( 255, 255, 0 );
        c[ImFluentCol_SystemFillCritical] = ImColor( 255, 0, 0 );
        c[ImFluentCol_SystemFillNeutral] = ImColor( 255, 255, 255 );
        c[ImFluentCol_SystemFillAttention] = ImColor( 0, 255, 255 );
    }

    static inline ImVec4 LerpVec4( const ImVec4 & a, const ImVec4 & b, float t )
    {
        return ImVec4( a.x + (b.x - a.x) * t,
                       a.y + (b.y - a.y) * t,
                       a.z + (b.z - a.z) * t,
                       a.w + (b.w - a.w) * t );
    }

    static void ApplyUserAccent( ImFluentStyle & s, const ImVec4 & base, ImFluentThemePreset preset )
    {
        ImVec4 * c = s.Colors;
        const ImVec4 white( 1.f, 1.f, 1.f, 1.f );
        const ImVec4 black( 0.f, 0.f, 0.f, 1.f );

        ImVec4 base_opaque = base;
        base_opaque.w = 1.f;

        const bool dark = (preset != ImFluentThemePreset_Light);

        const ImVec4 light1 = LerpVec4( base_opaque, white, 0.20f );
        const ImVec4 light2 = LerpVec4( base_opaque, white, 0.40f );
        const ImVec4 light3 = LerpVec4( base_opaque, white, 0.65f );
        const ImVec4 dark1  = LerpVec4( base_opaque, black, 0.20f );
        const ImVec4 dark2  = LerpVec4( base_opaque, black, 0.40f );
        const ImVec4 dark3  = LerpVec4( base_opaque, black, 0.65f );

        c[ImFluentCol_AccentFillDefault]   = base_opaque;
        c[ImFluentCol_AccentFillSecondary] = ImVec4( base_opaque.x, base_opaque.y, base_opaque.z, 230.f / 255.f );
        c[ImFluentCol_AccentFillTertiary]  = ImVec4( base_opaque.x, base_opaque.y, base_opaque.z, 204.f / 255.f );
        c[ImFluentCol_AccentFillSelectedTextBg] = dark ? dark1 : base_opaque;

        if ( dark )
        {
            c[ImFluentCol_AccentTextPrimary]   = light3;
            c[ImFluentCol_AccentTextSecondary] = light2;
            c[ImFluentCol_AccentTextTertiary]  = light1;
        }
        else
        {
            c[ImFluentCol_AccentTextPrimary]   = dark2;
            c[ImFluentCol_AccentTextSecondary] = dark1;
            c[ImFluentCol_AccentTextTertiary]  = base_opaque;
        }

        c[ImFluentCol_ElevationTextControlFocusedBottom] = base_opaque;
    }

    static void BuildThemePalette( ImFluentStyle & s, ImFluentThemePreset preset )
    {
        switch ( preset )
        {
            case ImFluentThemePreset_Light:
                BuildLightPalette( s );
                break;
            case ImFluentThemePreset_HighContrast:
                BuildHighContrastPalette( s );
                break;
            case ImFluentThemePreset_Dark:
            default:
                BuildDarkPalette( s ); break;
        }
        if ( g_Ctx.HasUserAccent && preset != ImFluentThemePreset_HighContrast )
            ApplyUserAccent( s, g_Ctx.UserAccent, preset );
    }

    static inline ImU32 LerpColorU32( ImU32 a, ImU32 b, float t )
    {
        if ( t <= 0.f ) return a;
        if ( t >= 1.f ) return b;
        const int  t8 = ( int )(t * 256.f);
        const int  it = 256 - t8;
        const ImU32 ar = (a >> 0) & 0xFF, br = (b >> 0) & 0xFF;
        const ImU32 ag = (a >> 8) & 0xFF, bg = (b >> 8) & 0xFF;
        const ImU32 ab = (a >> 16) & 0xFF, bb = (b >> 16) & 0xFF;
        const ImU32 aa = (a >> 24) & 0xFF, ba = (b >> 24) & 0xFF;
        return  ( ImU32 )((ar * it + br * t8) >> 8)
            | (( ImU32 )((ag * it + bg * t8) >> 8) << 8)
            | (( ImU32 )((ab * it + bb * t8) >> 8) << 16)
            | (( ImU32 )((aa * it + ba * t8) >> 8) << 24);
    }

    static void DrawAndConsumePendingHeader()
    {
        if ( !g_Ctx.NextItem.HasHeader ) return;
        const ImFluentStyle & style = ImFluent::GetStyle();
        ImFluent::TextBlock( g_Ctx.NextItem.Header, ImFluentTextStyle_Body );
        ImGui::Dummy( ImVec2( 0.f, FluentDpx( style.SpacingXSmall ) ) );
        g_Ctx.NextItem.Header = NULL;
        g_Ctx.NextItem.HasHeader = false;
    }

    static void DrawAndConsumePendingDescription()
    {
        if ( !g_Ctx.NextItem.HasDescription ) return;
        const ImFluentStyle & style = ImFluent::GetStyle();
        ImGui::Dummy( ImVec2( 0.f, FluentDpx( style.SpacingXSmall ) ) );
        ImFluent::TextBlockColored( g_Ctx.NextItem.Description,
                                    ImFluent::GetColorU32( ImFluentCol_TextSecondary ),
                                    ImFluentTextStyle_Caption );
        g_Ctx.NextItem.Description = NULL;
        g_Ctx.NextItem.HasDescription = false;
    }

    static const char * ConsumePendingGlyph( const char * fallback )
    {
        if ( !g_Ctx.NextItem.HasGlyph ) return fallback;
        const char * g = g_Ctx.NextItem.Glyph;
        g_Ctx.NextItem.Glyph = NULL;
        g_Ctx.NextItem.HasGlyph = false;
        return g;
    }

    static bool HasPendingError() { return g_Ctx.NextItem.HasError; }

    static void DrawAndConsumePendingError( const ImRect & item_bb )
    {
        if ( !g_Ctx.NextItem.HasError ) return;
        const ImFluentStyle & style = ImFluent::GetStyle();
        const float t = FluentDpx( style.TextInputAccentLineThickness );
        const float r = FluentDpx( style.ControlCornerRadius );
        const ImU32 col = ImFluent::GetColorU32( ImFluentCol_SystemFillCritical );
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2( item_bb.Min.x + r * 0.5f, item_bb.Max.y - t ),
            ImVec2( item_bb.Max.x - r * 0.5f, item_bb.Max.y ),
            col );
        ImFluent::PushFont( ImFluentTextStyle_Caption );
        ImGui::PushStyleColor( ImGuiCol_Text, col );
        ImGui::TextUnformatted( g_Ctx.NextItem.Error );
        ImGui::PopStyleColor();
        ImFluent::PopFont();
        g_Ctx.NextItem.Error = NULL;
        g_Ctx.NextItem.HasError = false;
    }

    static void DrawAndConsumePendingError()
    {
        if ( !g_Ctx.NextItem.HasError ) return;
        DrawAndConsumePendingError( ImGui::GetCurrentContext()->LastItemData.Rect );
    }

    static const ImU32 kAnimSeed = 0xF1ECF1ECu;

    static ImU32 AnimateColorU32( ImGuiID id, ImU32 target, float seconds = 0.083f )
    {
        ImGuiStorage * s = ImGui::GetStateStorage();
        const ImGuiID kId = id ^ kAnimSeed;
        const ImGuiID kFrame = kId + 1;
        const int currentFrame = ImGui::GetFrameCount();
        const int lastFrame = s->GetInt( kFrame, -1 );
        s->SetInt( kFrame, currentFrame );
        if ( lastFrame != currentFrame - 1 )
        {
            s->SetInt( kId, ( int )target );
            return target;
        }
        ImU32 cur = ( ImU32 )s->GetInt( kId, ( int )target );
        if ( cur == target ) return target;
        const float dt = ImGui::GetIO().DeltaTime;
        const float t = (seconds <= 0.f) ? 1.f : 1.f - expf( -dt / seconds );
        cur = LerpColorU32( cur, target, t );
        s->SetInt( kId, ( int )cur );
        return cur;
    }

    static float AnimateFloat( ImGuiID id, float target, float seconds = 0.20f )
    {
        ImGuiStorage * s = ImGui::GetStateStorage();
        const ImGuiID kId = id ^ kAnimSeed;
        const ImGuiID kFrame = kId + 1;
        const int currentFrame = ImGui::GetFrameCount();
        const int lastFrame = s->GetInt( kFrame, -1 );
        s->SetInt( kFrame, currentFrame );
        if ( lastFrame != currentFrame - 1 )
        {
            s->SetFloat( kId, target );
            return target;
        }
        float cur = s->GetFloat( kId, target );
        if ( cur == target ) return target;
        const float dt = ImGui::GetIO().DeltaTime;
        const float tt = (seconds <= 0.f) ? 1.f : 1.f - expf( -dt / seconds );
        cur = cur + (target - cur) * tt;
        s->SetFloat( kId, cur );
        return cur;
    }

    static void DrawFocusRing( ImDrawList * dl, const ImRect & bb, float rounding )
    {
        const ImFluentStyle & style = ImFluent::GetStyle();
        const float outer = FluentDpx( style.FocusStrokeThicknessOuter );
        const float inner = FluentDpx( style.FocusStrokeThicknessInner );
        const float pad = outer;

        dl->AddRect( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_FocusStrokeInner ), rounding, 0, inner );

        dl->AddRect( ImVec2( bb.Min.x - pad, bb.Min.y - pad ),
                     ImVec2( bb.Max.x + pad, bb.Max.y + pad ),
                     ImFluent::GetColorU32( ImFluentCol_FocusStrokeOuter ), rounding + pad, 0, outer );
    }

    static void DrawElevationBorder( ImDrawList * dl, const ImRect & bb, float rounding,
                                     ImU32 sides, ImU32 bottom, float bottom_thickness )
    {
        dl->AddRect( bb.Min, bb.Max, sides, rounding, 0, 1.f );

        const float t = (bottom_thickness > 0.f) ? bottom_thickness : 1.f;
        dl->AddRectFilled( ImVec2( bb.Min.x + rounding, bb.Max.y - t ),
                           ImVec2( bb.Max.x - rounding, bb.Max.y ),
                           bottom );
    }

    static void DrawElevationShadow( ImDrawList * dl, const ImRect & bb, float rounding, int layers )
    {
        if ( layers < 1 ) layers = 1;
        for ( int i = 0; i < layers; ++i )
        {
            const float k = ( float )(i + 1);
            const float pad = FluentDpx( ImFluent::GetStyle().SpacingXSmall * k );
            const ImU32 col = IM_COL32( 0, 0, 0, ( int )(40.f / k) );
            dl->AddRectFilled( ImVec2( bb.Min.x - pad, bb.Min.y - pad + k ),
                               ImVec2( bb.Max.x + pad, bb.Max.y + pad + k ),
                               col, rounding + pad );
        }
    }

    static void DrawAcrylicSurrogate( ImDrawList * dl, const ImRect & bb, float rounding, ImU32 base )
    {
        dl->AddRectFilled( bb.Min, bb.Max, base, rounding );
        dl->AddRectFilled( bb.Min, bb.Max, IM_COL32( 255, 255, 255, 8 ), rounding );
    }

    static ImU32 ResolveControlFillState( bool disabled, bool held, bool hovered )
    {
        using namespace ImFluent;
        if ( disabled ) return ImFluent::GetColorU32( ImFluentCol_ControlFillDisabled );
        if ( held )     return ImFluent::GetColorU32( ImFluentCol_ControlFillTertiary );
        if ( hovered )  return ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary );
        return                ImFluent::GetColorU32( ImFluentCol_ControlFillDefault );
    }

    static ImU32 ResolveAccentFillState( bool disabled, bool held, bool hovered )
    {
        using namespace ImFluent;
        if ( disabled ) return ImFluent::GetColorU32( ImFluentCol_AccentFillDisabled );
        if ( held )     return ImFluent::GetColorU32( ImFluentCol_AccentFillTertiary );
        if ( hovered )  return ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary );
        return                ImFluent::GetColorU32( ImFluentCol_AccentFillDefault );
    }

    static ImU32 ResolveSubtleFillState( bool selected, bool held, bool hovered )
    {
        using namespace ImFluent;
        if ( selected ) return ImFluent::GetColorU32( held ? ImFluentCol_SubtleFillTertiary : ImFluentCol_SubtleFillSecondary );
        if ( held )     return ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary );
        if ( hovered )  return ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary );
        return                ImFluent::GetColorU32( ImFluentCol_SubtleFillTransparent );
    }

    static ImU32 ResolveControlAltFillState( bool disabled, bool held, bool hovered )
    {
        using namespace ImFluent;
        if ( disabled ) return ImFluent::GetColorU32( ImFluentCol_ControlAltFillDisabled );
        if ( held )     return ImFluent::GetColorU32( ImFluentCol_ControlAltFillQuarternary );
        if ( hovered )  return ImFluent::GetColorU32( ImFluentCol_ControlAltFillTertiary );
        return                ImFluent::GetColorU32( ImFluentCol_ControlAltFillSecondary );
    }

    static void DrawChevron( ImDrawList * dl, ImVec2 c, ImGuiDir dir, ImU32 col, float L, float th = 1.5f )
    {
        switch ( dir )
        {
            case ImGuiDir_Down:
                dl->AddLine( ImVec2( c.x - L, c.y - L * 0.5f ), ImVec2( c.x, c.y + L * 0.5f ), col, th );
                dl->AddLine( ImVec2( c.x, c.y + L * 0.5f ), ImVec2( c.x + L, c.y - L * 0.5f ), col, th );
                break;
            case ImGuiDir_Up:
                dl->AddLine( ImVec2( c.x - L, c.y + L * 0.5f ), ImVec2( c.x, c.y - L * 0.5f ), col, th );
                dl->AddLine( ImVec2( c.x, c.y - L * 0.5f ), ImVec2( c.x + L, c.y + L * 0.5f ), col, th );
                break;
            case ImGuiDir_Right:
                dl->AddLine( ImVec2( c.x - L * 0.5f, c.y - L ), ImVec2( c.x + L * 0.5f, c.y ), col, th );
                dl->AddLine( ImVec2( c.x + L * 0.5f, c.y ), ImVec2( c.x - L * 0.5f, c.y + L ), col, th );
                break;
            case ImGuiDir_Left:
            default:
                dl->AddLine( ImVec2( c.x + L * 0.5f, c.y - L ), ImVec2( c.x - L * 0.5f, c.y ), col, th );
                dl->AddLine( ImVec2( c.x - L * 0.5f, c.y ), ImVec2( c.x + L * 0.5f, c.y + L ), col, th );
                break;
        }
    }

    static void DrawSelectionIndicator( ImDrawList * dl, const ImRect & bb, ImGuiDir side, ImU32 col, float length_frac = 0.5f )
    {
        const ImFluentStyle & style = ImFluent::GetStyle();
        const float thickness = FluentDpx( style.SelectionIndicatorThickness );
        const float inset = FluentDpx( style.SelectionIndicatorInset );
        const float corner = FluentDpx( style.SpacingXSmall );
        if ( side == ImGuiDir_Left || side == ImGuiDir_Right )
        {
            const float h = bb.GetHeight() * length_frac;
            const float cy = (bb.Min.y + bb.Max.y) * 0.5f;
            const float x = (side == ImGuiDir_Left) ? bb.Min.x + inset
                : bb.Max.x - inset - thickness;
            dl->AddRectFilled( ImVec2( x, cy - h * 0.5f ), ImVec2( x + thickness, cy + h * 0.5f ), col, corner );
        }
        else
        {
            const float w = bb.GetWidth() * length_frac;
            const float cx = (bb.Min.x + bb.Max.x) * 0.5f;
            const float y = (side == ImGuiDir_Up) ? bb.Min.y + inset
                : bb.Max.y - inset - thickness;
            dl->AddRectFilled( ImVec2( cx - w * 0.5f, y ), ImVec2( cx + w * 0.5f, y + thickness ), col, corner );
        }
    }

    static void PushControlFrameStyle( float h )
    {
        using namespace ImFluent;
        const ImFluentStyle & style = ImFluent::GetStyle();
        ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, ImFluent::FluentDpx( style.ControlCornerRadius ) );
        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( ImFluent::FluentDpx( style.SpacingLarge ), (h - ImGui::GetFontSize()) * 0.5f ) );
        ImGui::PushStyleColor( ImGuiCol_FrameBg, ImFluent::GetColorU32( ImFluentCol_ControlFillDefault ) );
        ImGui::PushStyleColor( ImGuiCol_FrameBgHovered, ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary ) );
        ImGui::PushStyleColor( ImGuiCol_FrameBgActive, ImFluent::GetColorU32( ImFluentCol_ControlFillInputActive ) );
        ImGui::PushStyleColor( ImGuiCol_Border, ImFluent::GetColorU32( ImFluentCol_ControlStrokeDefault ) );
    }

    static void PopControlFrameStyle()
    {
        ImGui::PopStyleColor( 4 );
        ImGui::PopStyleVar( 2 );
    }

    static void PushOverlayWindowStyle( float padding_x, float padding_y )
    {
        using namespace ImFluent;
        const ImFluentStyle & style = ImFluent::GetStyle();
        ImGui::PushStyleColor( ImGuiCol_PopupBg, ImFluent::GetColorU32( ImFluentCol_SolidBgQuarternary ) );
        ImGui::PushStyleColor( ImGuiCol_Border, ImFluent::GetColorU32( ImFluentCol_SurfaceStrokeFlyout ) );
        ImGui::PushStyleVar( ImGuiStyleVar_PopupRounding, FluentDpx( style.OverlayCornerRadius ) );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( padding_x ), FluentDpx( padding_y ) ) );
    }

    static void PopOverlayWindowStyle()
    {
        ImGui::PopStyleVar( 2 );
        ImGui::PopStyleColor( 2 );
    }

    static void SetFont( ImFluentTextStyle style, ImFont * font )
    {
        if ( style >= 0 && style < ImFluentTextStyle_COUNT )
            g_Ctx.Fonts[style] = font;
    }
    static ImFont * GetFont( ImFluentTextStyle style )
    {
        if ( style >= 0 && style < ImFluentTextStyle_COUNT && g_Ctx.Fonts[style] )
            return g_Ctx.Fonts[style];
        return ImGui::GetFont();
    }

    static const char * LocalizeGetMsg( ImFluentLocKey key )
    {
        if ( key < 0 || key >= ImFluentLocKey_COUNT ) return "*Missing Text*";
        const char * msg = g_Ctx.Style.LocalizationTable[key];
        return msg ? msg : "*Missing Text*";
    }

    static void LocalizeRegisterEntries( const ImFluentLocEntry * entries, int count )
    {
        if ( !entries || count <= 0 ) return;
        for ( int n = 0; n < count; ++n )
        {
            const ImFluentLocKey k = entries[n].Key;
            if ( k < 0 || k >= ImFluentLocKey_COUNT ) continue;
            g_Ctx.Style.LocalizationTable[k] = entries[n].Text;
        }
    }

    static void StorePopupAnchor( ImGuiID id, const ImRect & rect )
    {
        for ( int i = 0; i < g_Ctx.PopupAnchorCount; ++i )
            if ( g_Ctx.PopupAnchors[i].Id == id ) { g_Ctx.PopupAnchors[i].Rect = rect; return; }
        if ( g_Ctx.PopupAnchorCount < ImFluentContext::MaxPopupAnchors )
        {
            g_Ctx.PopupAnchors[g_Ctx.PopupAnchorCount].Id = id;
            g_Ctx.PopupAnchors[g_Ctx.PopupAnchorCount].Rect = rect;
            ++g_Ctx.PopupAnchorCount;
        }
    }

    static const ImRect * FindPopupAnchor( ImGuiID id )
    {
        for ( int i = 0; i < g_Ctx.PopupAnchorCount; ++i )
            if ( g_Ctx.PopupAnchors[i].Id == id ) return &g_Ctx.PopupAnchors[i].Rect;
        return NULL;
    }

    static void ApplyPopupAnchor( const char * str_id )
    {
        if ( !ImGui::IsPopupOpen( str_id, ImGuiPopupFlags_None ) ) return;
        const ImGuiID pid = ImGui::GetID( str_id );
        const ImRect * anchor = FindPopupAnchor( pid );
        if ( !anchor ) return;
        const ImVec2 pos( anchor->Min.x, anchor->Max.y + FluentDpx( ImFluent::GetStyle().SpacingXSmall ) );
        ImGui::SetNextWindowPos( pos, ImGuiCond_Appearing );
    }

    static ImFluentAppBarLabelPosition ConsumeAppBarLabelPos()
    {
        if ( !g_Ctx.NextAppBarLabelPosSet ) return ImFluentAppBarLabelPosition_Bottom;
        const ImFluentAppBarLabelPosition v = g_Ctx.NextAppBarLabelPos;
        g_Ctx.NextAppBarLabelPosSet = false;
        return v;
    }

    static void DrawAppBarContent( ImDrawList * dl, const ImRect & bb,
                                   const char * label, const char * glyph, ImU32 textCol,
                                   ImFluentAppBarLabelPosition pos, const ImFluentStyle & style )
    {
        const float W = bb.GetWidth();
        const float H = bb.GetHeight();
        const ImVec2 ts = (pos != ImFluentAppBarLabelPosition_Collapsed && label) ? ImGui::CalcTextSize( label ) : ImVec2( 0, 0 );
        const ImVec2 gs = glyph ? ImGui::CalcTextSize( glyph ) : ImVec2( 0, 0 );

        if ( pos == ImFluentAppBarLabelPosition_Right )
        {
            const float pad = FluentDpx( style.SpacingMedium );
            const float gap = (glyph && label) ? FluentDpx( style.SpacingMedium ) : 0.f;
            const float run_w = gs.x + gap + ts.x;
            float x = bb.Min.x + (W - run_w) * 0.5f;
            if ( glyph )
            {
                dl->AddText( ImVec2( x, bb.Min.y + (H - gs.y) * 0.5f ), textCol, glyph );
                x += gs.x + gap;
            }
            if ( label && pos != ImFluentAppBarLabelPosition_Collapsed )
                dl->AddText( ImVec2( x, bb.Min.y + (H - ts.y) * 0.5f ), textCol, label );
            ( void )pad;
        }
        else if ( pos == ImFluentAppBarLabelPosition_Collapsed )
        {
            if ( glyph )
                dl->AddText( ImVec2( bb.Min.x + (W - gs.x) * 0.5f,
                             bb.Min.y + (H - gs.y) * 0.5f ), textCol, glyph );
        }
        else
        {
            if ( glyph )
                dl->AddText( ImVec2( bb.Min.x + (W - gs.x) * 0.5f,
                             bb.Min.y + FluentDpx( style.SpacingMedium ) ), textCol, glyph );
            if ( label )
                dl->AddText( ImVec2( bb.Min.x + (W - ts.x) * 0.5f,
                             bb.Max.y - ts.y - FluentDpx( style.SpacingMedium - 2.f ) ), textCol, label );
        }
    }

    static ImVec2 CalcButtonSize( const char * label, const ImVec2 & size_arg, float min_height_dpx, float pad_x, float pad_y )
    {
        const ImVec2 ts = ImGui::CalcTextSize( label, NULL, true );
        return ImVec2( size_arg.x > 0.f ? size_arg.x : ts.x + pad_x * 2.f,
                       size_arg.y > 0.f ? size_arg.y : ImMax( min_height_dpx, ts.y + pad_y * 2.f ) );
    }

    static void DrawButtonLabel( ImDrawList *, const ImRect & bb, const char * label, ImU32 col, float pad_x, float pad_y )
    {
        ImGui::PushStyleColor( ImGuiCol_Text, col );
        const ImVec2 ts = ImGui::CalcTextSize( label, NULL, true );
        const ImVec2 text_min( bb.Min.x + pad_x, bb.Min.y + pad_y );
        const ImVec2 text_max( bb.Max.x - pad_x, bb.Max.y - pad_y );
        ImGui::RenderTextClipped( text_min, text_max, label, NULL, &ts, ImVec2( 0.5f, 0.5f ), &bb );
        ImGui::PopStyleColor();
    }

    static bool IsItemFocused( ImGuiID id )
    {
        ImGuiContext & g = *ImGui::GetCurrentContext();
        return g.NavId == id && g.NavCursorVisible;
    }

    static bool ButtonExStyled( const char * label, const ImVec2 & size_arg,
                                ImU32 fillRest, ImU32 fillHover, ImU32 fillPress, ImU32 fillDisabled,
                                ImU32 textCol, ImU32 textDisabled,
                                ImU32 stroke, ImU32 strokeBottom,
                                ImGuiButtonFlags flags )
    {
        ImGuiWindow * w = ImGui::GetCurrentWindow();
        if ( w->SkipItems ) return false;

        const ImFluentStyle & style = ImFluent::GetStyle();
        const ImGuiID id = w->GetID( label );
        const float pad_x = FluentDpx( style.ControlContentPadding.x );
        const float pad_y = FluentDpx( style.ControlContentPadding.y );
        const float min_h = FluentDpx( style.ControlHeight );
        const ImVec2 sz = CalcButtonSize( label, size_arg, min_h, pad_x, pad_y );
        const ImVec2 pos = w->DC.CursorPos;
        const ImRect bb( pos, ImVec2( pos.x + sz.x, pos.y + sz.y ) );
        ImGui::ItemSize( bb );
        if ( !ImGui::ItemAdd( bb, id ) ) return false;

        bool hovered = false, held = false;
        const bool pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held, flags );
        const bool disabled = (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;

        ImU32 fillTarget;
        if ( disabled )      fillTarget = fillDisabled;
        else if ( held )     fillTarget = fillPress;
        else if ( hovered )  fillTarget = fillHover;
        else               fillTarget = fillRest;

        const ImU32 fillAnim = AnimateColorU32( id, fillTarget );
        const float r = FluentDpx( style.ControlCornerRadius );
        ImDrawList * dl = w->DrawList;

        dl->AddRectFilled( bb.Min, bb.Max, fillAnim, r );
        if ( !held && !disabled )
            DrawElevationBorder( dl, bb, r, stroke, strokeBottom, 1.f );
        else
            dl->AddRect( bb.Min, bb.Max, stroke, r, 0, 1.f );

        DrawButtonLabel( dl, bb, label, disabled ? textDisabled : textCol, pad_x, pad_y );

        if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, r );
        return pressed;
    }
}

// [SECTION] Style & Theme

IMGUI_API ImFluentStyle::ImFluentStyle()
{
    ImFluent::BuildThemePalette( *this, g_Ctx.Preset );
}

ImFluentStyle & ImFluent::GetStyle()
{
    return g_Ctx.Style;
}

float  ImFluent::FluentDpx( float v ) { return v * ImGui::GetStyle().FontScaleDpi; }
ImVec2 ImFluent::FluentDpx( const ImVec2 & v ) { return ImVec2( FluentDpx( v.x ), FluentDpx( v.y ) ); }


ImU32 ImFluent::GetColorU32( ImFluentCol idx, float alpha_mul )
{
    if ( idx < 0 || idx >= ImFluentCol_COUNT ) return 0;
    const ImU32 c = ( ImColor )g_Ctx.Style.Colors[idx];
    if ( alpha_mul >= 1.f ) return c;
    if ( alpha_mul <= 0.f ) return c & ~IM_COL32_A_MASK;
    const ImU32 a = ( ImU32 )(( float )((c >> IM_COL32_A_SHIFT) & 0xFF) * alpha_mul);
    return (c & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

const ImVec4 & ImFluent::GetStyleColorVec4( ImFluentCol idx )
{
    static const ImVec4 zero( 0, 0, 0, 0 );
    if ( idx < 0 || idx >= ImFluentCol_COUNT ) return zero;
    return g_Ctx.Style.Colors[idx];
}

ImFluentThemePreset ImFluent::GetThemePreset() { return g_Ctx.Preset; }

void ImFluent::SetAccentColor( const ImColor & color )
{
    g_Ctx.HasUserAccent = true;
    g_Ctx.UserAccent    = color.Value;
    SetThemePreset( g_Ctx.Preset );
}

ImColor ImFluent::GetAccentColor()
{
    if ( g_Ctx.HasUserAccent ) return ImColor( g_Ctx.UserAccent );
    return ImColor( g_Ctx.Style.Colors[ImFluentCol_AccentFillDefault] );
}

bool ImFluent::HasUserAccentColor() { return g_Ctx.HasUserAccent; }

void ImFluent::SetThemePreset( ImFluentThemePreset preset )
{
    g_Ctx.Preset = preset;
    BuildThemePalette( g_Ctx.Style, preset );

    ImGuiStyle & s = ImGui::GetStyle();
    ImVec4 * c = s.Colors;
    const ImVec4 * fc = g_Ctx.Style.Colors;

    c[ImGuiCol_Text] = fc[ImFluentCol_TextPrimary];
    c[ImGuiCol_TextDisabled] = fc[ImFluentCol_TextDisabled];
    c[ImGuiCol_WindowBg] = fc[ImFluentCol_SolidBgBase];
    c[ImGuiCol_ChildBg] = fc[ImFluentCol_LayerFillDefault];
    c[ImGuiCol_PopupBg] = fc[ImFluentCol_LayerFillAlt];
    c[ImGuiCol_Border] = fc[ImFluentCol_ControlStrokeDefault];
    c[ImGuiCol_BorderShadow] = fc[ImFluentCol_ElevationControlBottom];
    c[ImGuiCol_FrameBg] = fc[ImFluentCol_ControlFillDefault];
    c[ImGuiCol_FrameBgHovered] = fc[ImFluentCol_ControlFillSecondary];
    c[ImGuiCol_FrameBgActive] = fc[ImFluentCol_ControlFillTertiary];
    c[ImGuiCol_TitleBg] = fc[ImFluentCol_SolidBgBase];
    c[ImGuiCol_TitleBgActive] = fc[ImFluentCol_SolidBgBase];
    c[ImGuiCol_TitleBgCollapsed] = fc[ImFluentCol_SolidBgBase];
    c[ImGuiCol_MenuBarBg] = fc[ImFluentCol_LayerFillDefault];
    c[ImGuiCol_ScrollbarBg] = fc[ImFluentCol_ControlFillTransparent];
    c[ImGuiCol_ScrollbarGrab] = fc[ImFluentCol_ControlStrongFillDefault];
    c[ImGuiCol_ScrollbarGrabHovered] = fc[ImFluentCol_ControlStrongFillDefault];
    c[ImGuiCol_ScrollbarGrabActive] = fc[ImFluentCol_ControlStrongFillDefault];
    c[ImGuiCol_CheckMark] = fc[ImFluentCol_TextOnAccentPrimary];
    c[ImGuiCol_SliderGrab] = fc[ImFluentCol_AccentFillDefault];
    c[ImGuiCol_SliderGrabActive] = fc[ImFluentCol_AccentFillSecondary];
    c[ImGuiCol_Button] = fc[ImFluentCol_ControlFillDefault];
    c[ImGuiCol_ButtonHovered] = fc[ImFluentCol_ControlFillSecondary];
    c[ImGuiCol_ButtonActive] = fc[ImFluentCol_ControlFillTertiary];
    c[ImGuiCol_Header] = fc[ImFluentCol_SubtleFillSecondary];
    c[ImGuiCol_HeaderHovered] = fc[ImFluentCol_SubtleFillTertiary];
    c[ImGuiCol_HeaderActive] = fc[ImFluentCol_SubtleFillSecondary];
    c[ImGuiCol_Separator] = fc[ImFluentCol_DividerStrokeDefault];
    c[ImGuiCol_SeparatorHovered] = fc[ImFluentCol_DividerStrokeDefault];
    c[ImGuiCol_SeparatorActive] = fc[ImFluentCol_DividerStrokeDefault];
    c[ImGuiCol_ResizeGrip] = fc[ImFluentCol_ControlStrokeDefault];
    c[ImGuiCol_ResizeGripHovered] = fc[ImFluentCol_ControlStrokeSecondary];
    c[ImGuiCol_ResizeGripActive] = fc[ImFluentCol_ControlStrokeSecondary];
    c[ImGuiCol_Tab] = fc[ImFluentCol_SubtleFillTransparent];
    c[ImGuiCol_TabHovered] = fc[ImFluentCol_SubtleFillSecondary];
    c[ImGuiCol_TabSelected] = fc[ImFluentCol_LayerFillAlt];
    c[ImGuiCol_TabDimmed] = fc[ImFluentCol_SubtleFillTransparent];
    c[ImGuiCol_TabDimmedSelected] = fc[ImFluentCol_LayerFillAlt];
    c[ImGuiCol_PlotLines] = fc[ImFluentCol_AccentFillDefault];
    c[ImGuiCol_PlotHistogram] = fc[ImFluentCol_AccentFillDefault];
    c[ImGuiCol_TableHeaderBg] = fc[ImFluentCol_LayerFillDefault];
    c[ImGuiCol_TableBorderStrong] = fc[ImFluentCol_DividerStrokeDefault];
    c[ImGuiCol_TableBorderLight] = fc[ImFluentCol_DividerStrokeDefault];
    c[ImGuiCol_TableRowBg] = fc[ImFluentCol_SubtleFillTransparent];
    c[ImGuiCol_TableRowBgAlt] = fc[ImFluentCol_SubtleFillSecondary];
    c[ImGuiCol_TextSelectedBg] = fc[ImFluentCol_AccentFillSelectedTextBg];
    c[ImGuiCol_NavCursor] = fc[ImFluentCol_FocusStrokeOuter];
    c[ImGuiCol_NavWindowingHighlight] = fc[ImFluentCol_FocusStrokeOuter];
    c[ImGuiCol_NavWindowingDimBg] = fc[ImFluentCol_SmokeFill];
    c[ImGuiCol_ModalWindowDimBg] = fc[ImFluentCol_SmokeFill];

    s.WindowPadding = ImTrunc( ImVec2( 16.f * s.FontScaleDpi, 16.f * s.FontScaleDpi ) );
    s.FramePadding = ImTrunc( ImVec2( g_Ctx.Style.ControlContentPadding.x * s.FontScaleDpi, g_Ctx.Style.ControlContentPadding.y * s.FontScaleDpi ) );
    s.ItemSpacing = ImTrunc( ImVec2( 8.f * s.FontScaleDpi, 4.f * s.FontScaleDpi ) );
    s.ItemInnerSpacing = ImTrunc( ImVec2( 8.f * s.FontScaleDpi, 4.f * s.FontScaleDpi ) );
    s.IndentSpacing = ImTrunc( 16.f * s.FontScaleDpi );
    s.ScrollbarSize = ImTrunc( 12.f * s.FontScaleDpi );
    s.GrabMinSize = ImTrunc( 16.f * s.FontScaleDpi );
    s.WindowBorderSize = ImTrunc( 1.f * s.FontScaleDpi );
    s.ChildBorderSize = ImTrunc( 1.f * s.FontScaleDpi );
    s.PopupBorderSize = ImTrunc( 1.f * s.FontScaleDpi );
    s.FrameBorderSize = ImTrunc( 1. * s.FontScaleDpi );
    s.WindowRounding = ImTrunc( g_Ctx.Style.OverlayCornerRadius * s.FontScaleDpi );
    s.ChildRounding = ImTrunc( g_Ctx.Style.ControlCornerRadius * s.FontScaleDpi );
    s.PopupRounding = ImTrunc( g_Ctx.Style.OverlayCornerRadius * s.FontScaleDpi );
    s.FrameRounding = ImTrunc( g_Ctx.Style.ControlCornerRadius * s.FontScaleDpi );
    s.GrabRounding = ImTrunc( g_Ctx.Style.ControlCornerRadius * s.FontScaleDpi );
    s.ScrollbarRounding = ImTrunc( g_Ctx.Style.OverlayCornerRadius * s.FontScaleDpi );
    s.TabRounding = ImTrunc( g_Ctx.Style.ControlCornerRadius * s.FontScaleDpi );
}

// [SECTION] Style stack (PushFluentStyle / PushStyleColor / PushStyleVar / BeginDisabled)

void ImFluent::PushFluentStyle()
{
    if ( g_Ctx.Fonts[ImFluentTextStyle_Body] )
        ImGui::PushFont( g_Ctx.Fonts[ImFluentTextStyle_Body] );
    else
        ImGui::PushFont( ImGui::GetFont() );
}
void ImFluent::PopFluentStyle() { ImGui::PopFont(); }

static float * StyleVarFloatPtr( ImFluentStyle & s, ImFluentStyleVar idx )
{
    switch ( idx )
    {
        case ImFluentStyleVar_ControlCornerRadius:       return &s.ControlCornerRadius;
        case ImFluentStyleVar_OverlayCornerRadius:       return &s.OverlayCornerRadius;
        case ImFluentStyleVar_ControlHeight:             return &s.ControlHeight;
        case ImFluentStyleVar_ControlMinWidth:           return &s.ControlMinWidth;
        case ImFluentStyleVar_CardPadding:               return &s.CardPadding;
        case ImFluentStyleVar_SpacingXSmall:             return &s.SpacingXSmall;
        case ImFluentStyleVar_SpacingSmall:              return &s.SpacingSmall;
        case ImFluentStyleVar_SpacingMedium:             return &s.SpacingMedium;
        case ImFluentStyleVar_SpacingLarge:              return &s.SpacingLarge;
        case ImFluentStyleVar_SpacingXLarge:             return &s.SpacingXLarge;
        case ImFluentStyleVar_SpacingXXLarge:            return &s.SpacingXXLarge;
        case ImFluentStyleVar_StrokeThin:                return &s.StrokeThin;
        case ImFluentStyleVar_StrokeMedium:              return &s.StrokeMedium;
        case ImFluentStyleVar_StrokeThick:               return &s.StrokeThick;
        case ImFluentStyleVar_FocusStrokeThicknessOuter: return &s.FocusStrokeThicknessOuter;
        case ImFluentStyleVar_FocusStrokeThicknessInner: return &s.FocusStrokeThicknessInner;
        case ImFluentStyleVar_NavPaneCompactWidth:       return &s.NavPaneCompactWidth;
        case ImFluentStyleVar_NavPaneOpenWidth:          return &s.NavPaneOpenWidth;
        default:                                          return NULL;
    }
}

static ImVec2 * StyleVarVec2Ptr( ImFluentStyle & s, ImFluentStyleVar idx )
{
    if ( idx == ImFluentStyleVar_ControlContentPadding ) return &s.ControlContentPadding;
    return NULL;
}

void ImFluent::PushStyleColor( ImFluentCol idx, ImU32 col )
{
    PushStyleColor( idx, ImGui::ColorConvertU32ToFloat4( col ) );
}

void ImFluent::PushStyleColor( ImFluentCol idx, const ImVec4 & col )
{
    if ( idx < 0 || idx >= ImFluentCol_COUNT ) return;
    ImFluentColorStackEntry e; e.Idx = idx; e.Prev = g_Ctx.Style.Colors[idx];
    g_Ctx.ColorStack.push_back( e );
    g_Ctx.Style.Colors[idx] = col;
}

void ImFluent::PopStyleColor( int count )
{
    while ( count > 0 && !g_Ctx.ColorStack.empty() )
    {
        const ImFluentColorStackEntry & e = g_Ctx.ColorStack.back();
        g_Ctx.Style.Colors[e.Idx] = e.Prev;
        g_Ctx.ColorStack.pop_back();
        --count;
    }
}

void ImFluent::PushStyleVar( ImFluentStyleVar idx, float val )
{
    float * p = StyleVarFloatPtr( g_Ctx.Style, idx );
    if ( !p ) { IM_ASSERT( 0 && "PushStyleVar: idx is not a float" ); return; }
    ImFluentStyleVarStackEntry e; e.Idx = idx; e.Kind = ImFluentStyleVarKind_Float;
    e.Prev = ImVec2( *p, 0.f );
    g_Ctx.StyleVarStack.push_back( e );
    *p = val;
}

void ImFluent::PushStyleVar( ImFluentStyleVar idx, const ImVec2 & val )
{
    ImVec2 * p = StyleVarVec2Ptr( g_Ctx.Style, idx );
    if ( !p ) { IM_ASSERT( 0 && "PushStyleVar: idx is not a Vec2" ); return; }
    ImFluentStyleVarStackEntry e; e.Idx = idx; e.Kind = ImFluentStyleVarKind_Vec2;
    e.Prev = *p;
    g_Ctx.StyleVarStack.push_back( e );
    *p = val;
}

void ImFluent::PopStyleVar( int count )
{
    while ( count > 0 && !g_Ctx.StyleVarStack.empty() )
    {
        const ImFluentStyleVarStackEntry & e = g_Ctx.StyleVarStack.back();
        if ( e.Kind == ImFluentStyleVarKind_Float )
        {
            float * p = StyleVarFloatPtr( g_Ctx.Style, e.Idx );
            if ( p ) *p = e.Prev.x;
        }
        else
        {
            ImVec2 * p = StyleVarVec2Ptr( g_Ctx.Style, e.Idx );
            if ( p ) *p = e.Prev;
        }
        g_Ctx.StyleVarStack.pop_back();
        --count;
    }
}

void ImFluent::BeginDisabled( bool disabled )
{
    ImGui::BeginDisabled( disabled );
}

void ImFluent::EndDisabled()
{
    ImGui::EndDisabled();
}

// [SECTION] Fonts (LoadFluentFonts / PushFont / PopFont)

void ImFluent::LoadFluentFonts()
{

#if defined (_WIN32)
    char fontsDirectory[MAX_PATH] = { 0 };
    ::GetWindowsDirectoryA( fontsDirectory, MAX_PATH );
#endif

    struct TextStyleFontInfo { ImFluentTextStyle style; float dp; const char * fontFile; };

    const TextStyleFontInfo info[] =
    {
        { ImFluentTextStyle_Caption,     16.0f, "segoeui.ttf"     },
        { ImFluentTextStyle_Body,        20.0f, "segoeui.ttf"     },
        { ImFluentTextStyle_BodyStrong,  20.0f, "segoeuib.ttf"   },
        { ImFluentTextStyle_Subtitle,    28.0f, "segoeuib.ttf"   },
        { ImFluentTextStyle_Title,       36.0f, "segoeuib.ttf"   },
        { ImFluentTextStyle_TitleLarge,  52.0f, "segoeuib.ttf"   },
        { ImFluentTextStyle_Display,     92.0f, "segoeuib.ttf"   },
    };

    ImGuiIO & io = ImGui::GetIO();
    for ( const TextStyleFontInfo & i : info )
    {
        ImFontConfig cfg;
        cfg.SizePixels = i.dp;
        cfg.RasterizerMultiply = 1.2f;

        const char * fontPath = nullptr;
        ImFormatStringToTempBuffer( &fontPath, nullptr, "%s/Fonts/%s", fontsDirectory, i.fontFile );

        ImFont * font = io.Fonts->AddFontFromFileTTF( fontPath, i.dp, &cfg );
        IM_ASSERT( font != nullptr );

        static const ImWchar fluentIconsRange[] = { 0xE000, 0xF8FF, 0 };

        ImFormatStringToTempBuffer( &fontPath, nullptr, "%s/Fonts/SegoeIcons.ttf", fontsDirectory, i.fontFile );

        float iconSize = i.dp * 0.75f;
        cfg.MergeMode = true;
        cfg.PixelSnapH = true;
        cfg.GlyphOffset = { 0.0f, iconSize / 8.0f };

        io.Fonts->AddFontFromFileTTF( fontPath, iconSize, &cfg, fluentIconsRange );

        SetFont( i.style, font );
    }

    ImGui::GetIO().FontDefault = GetFont( ImFluentTextStyle_Body );
}

void ImFluent::PushFont( ImFluentTextStyle style )
{
    ImFont * f = GetFont( style );
    ImGui::PushFont( f );
}
void ImFluent::PopFont() { ImGui::PopFont(); }

// [SECTION] Next-item attributes

void ImFluent::SetNextItemHeader( const char * text ) { g_Ctx.NextItem.Header = text;  g_Ctx.NextItem.HasHeader = (text != NULL); }
void ImFluent::SetNextItemDescription( const char * text ) { g_Ctx.NextItem.Description = text;  g_Ctx.NextItem.HasDescription = (text != NULL); }
void ImFluent::SetNextItemGlyph( const char * glyph ) { g_Ctx.NextItem.Glyph = glyph; g_Ctx.NextItem.HasGlyph = (glyph != NULL); }
void ImFluent::SetNextItemError( const char * error ) { g_Ctx.NextItem.Error = error; g_Ctx.NextItem.HasError = (error != NULL); }

// [SECTION] Buttons

bool ImFluent::Button( const char * label, const ImVec2 & size )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    return ButtonExStyled( label, size,
                           ImFluent::GetColorU32( ImFluentCol_ControlFillDefault ), ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary ), ImFluent::GetColorU32( ImFluentCol_ControlFillTertiary ), ImFluent::GetColorU32( ImFluentCol_ControlFillDisabled ),
                           ImFluent::GetColorU32( ImFluentCol_TextPrimary ), ImFluent::GetColorU32( ImFluentCol_TextDisabled ),
                           ImFluent::GetColorU32( ImFluentCol_ControlStrokeDefault ), ImFluent::GetColorU32( ImFluentCol_ElevationControlBottom ), ImGuiButtonFlags_None );
}

bool ImFluent::AccentButton( const char * label, const ImVec2 & size )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    return ButtonExStyled( label, size,
                           ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary ), ImFluent::GetColorU32( ImFluentCol_AccentFillTertiary ), ImFluent::GetColorU32( ImFluentCol_AccentFillDisabled ),
                           ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary ), ImFluent::GetColorU32( ImFluentCol_TextOnAccentDisabled ),
                           ImFluent::GetColorU32( ImFluentCol_ControlStrokeOnAccentDefault ), ImFluent::GetColorU32( ImFluentCol_ElevationAccentBottom ), ImGuiButtonFlags_None );
}

bool ImFluent::RepeatButton( const char * label, const ImVec2 & size )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGui::PushItemFlag( ImGuiItemFlags_ButtonRepeat, true );
    const bool pressed = ButtonExStyled( label, size,
                                         ImFluent::GetColorU32( ImFluentCol_ControlFillDefault ), ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary ), ImFluent::GetColorU32( ImFluentCol_ControlFillTertiary ), ImFluent::GetColorU32( ImFluentCol_ControlFillDisabled ),
                                         ImFluent::GetColorU32( ImFluentCol_TextPrimary ), ImFluent::GetColorU32( ImFluentCol_TextDisabled ),
                                         ImFluent::GetColorU32( ImFluentCol_ControlStrokeDefault ), ImFluent::GetColorU32( ImFluentCol_ElevationControlBottom ), ImGuiButtonFlags_None );
    ImGui::PopItemFlag();
    return pressed;
}

bool ImFluent::HyperlinkButton( const char * label )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const ImVec2 ts = ImGui::CalcTextSize( label, NULL, true );
    const float pad_y = FluentDpx( style.SpacingXSmall );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + ts.x, pos.y + ts.y + pad_y * 2.f ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );
    const ImU32 col = held ? ImFluent::GetColorU32( ImFluentCol_AccentTextTertiary ) : (hovered ? ImFluent::GetColorU32( ImFluentCol_AccentTextSecondary ) : ImFluent::GetColorU32( ImFluentCol_AccentTextPrimary ));
    const ImU32 anim = AnimateColorU32( id, col );

    ImDrawList * dl = w->DrawList;
    dl->AddText( ImVec2( bb.Min.x, bb.Min.y + pad_y ), anim, label );
    if ( hovered )
        dl->AddLine( ImVec2( bb.Min.x, bb.Max.y - 1.f ), ImVec2( bb.Min.x + ts.x, bb.Max.y - 1.f ), anim, 1.f );
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, FluentDpx( style.ControlCornerRadius ) );
    return pressed;
}

bool ImFluent::ToggleButton( const char * label, bool * v, const ImVec2 & size )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    bool pressed;
    if ( v && *v )
    {
        pressed = ButtonExStyled( label, size,
                                  ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary ), ImFluent::GetColorU32( ImFluentCol_AccentFillTertiary ), ImFluent::GetColorU32( ImFluentCol_AccentFillDisabled ),
                                  ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary ), ImFluent::GetColorU32( ImFluentCol_TextOnAccentDisabled ),
                                  ImFluent::GetColorU32( ImFluentCol_ControlStrokeOnAccentDefault ), ImFluent::GetColorU32( ImFluentCol_ElevationAccentBottom ), ImGuiButtonFlags_None );
    }
    else
    {
        pressed = ButtonExStyled( label, size,
                                  ImFluent::GetColorU32( ImFluentCol_ControlFillDefault ), ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary ), ImFluent::GetColorU32( ImFluentCol_ControlFillTertiary ), ImFluent::GetColorU32( ImFluentCol_ControlFillDisabled ),
                                  ImFluent::GetColorU32( ImFluentCol_TextPrimary ), ImFluent::GetColorU32( ImFluentCol_TextDisabled ),
                                  ImFluent::GetColorU32( ImFluentCol_ControlStrokeDefault ), ImFluent::GetColorU32( ImFluentCol_ElevationControlBottom ), ImGuiButtonFlags_None );
    }
    if ( pressed && v ) *v = !*v;
    return pressed;
}

bool ImFluent::DropDownButtonEx( const char * label, bool * v_state,
                                bool * dropdown_clicked,
                                const ImVec2 & size, bool split, bool toggled )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;

    const ImFluentStyle & style = ImFluent::GetStyle();
    const float pad_x = FluentDpx( style.ControlContentPadding.x );
    const float pad_y = FluentDpx( style.ControlContentPadding.y );
    const float min_h = FluentDpx( style.ControlHeight );
    const float chev_w = FluentDpx( style.SpinButtonWidth );
    const ImVec2 ts = ImGui::CalcTextSize( label, NULL, true );
    const float total_w = (size.x > 0.f ? size.x : ts.x + pad_x * 2.f + chev_w);
    const float total_h = (size.y > 0.f ? size.y : ImMax( min_h, ts.y + pad_y * 2.f ));

    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb_total( pos, ImVec2( pos.x + total_w, pos.y + total_h ) );
    const ImRect bb_main( pos, ImVec2( pos.x + total_w - (split ? chev_w : 0.f), pos.y + total_h ) );
    const ImRect bb_chev( ImVec2( bb_main.Max.x, pos.y ), bb_total.Max );

    ImGui::ItemSize( bb_total );
    const ImGuiID id_main = w->GetID( label );
    const ImGuiID id_chev = w->GetID( ( const void * )(( intptr_t )id_main + 1) );
    if ( !ImGui::ItemAdd( bb_total, id_main ) ) return false;

    bool main_hovered = false, main_held = false;
    const bool main_pressed = ImGui::ButtonBehavior( bb_main, id_main, &main_hovered, &main_held );

    bool chev_hovered = false, chev_held = false, chev_pressed = false;
    if ( split )
        chev_pressed = ImGui::ButtonBehavior( bb_chev, id_chev, &chev_hovered, &chev_held );

    const bool disabled = (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;

    ImU32 fillRest, fillHover, fillPress, fillDisabled, textCol, textDisabled, strokeCol, strokeBottom;
    if ( toggled )
    {
        fillRest = ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ); fillHover = ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary ); fillPress = ImFluent::GetColorU32( ImFluentCol_AccentFillTertiary ); fillDisabled = ImFluent::GetColorU32( ImFluentCol_AccentFillDisabled );
        textCol = ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary ); textDisabled = ImFluent::GetColorU32( ImFluentCol_TextOnAccentDisabled );
        strokeCol = ImFluent::GetColorU32( ImFluentCol_ControlStrokeOnAccentDefault ); strokeBottom = ImFluent::GetColorU32( ImFluentCol_ElevationAccentBottom );
    }
    else
    {
        fillRest = ImFluent::GetColorU32( ImFluentCol_ControlFillDefault ); fillHover = ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary ); fillPress = ImFluent::GetColorU32( ImFluentCol_ControlFillTertiary ); fillDisabled = ImFluent::GetColorU32( ImFluentCol_ControlFillDisabled );
        textCol = ImFluent::GetColorU32( ImFluentCol_TextPrimary ); textDisabled = ImFluent::GetColorU32( ImFluentCol_TextDisabled );
        strokeCol = ImFluent::GetColorU32( ImFluentCol_ControlStrokeDefault ); strokeBottom = ImFluent::GetColorU32( ImFluentCol_ElevationControlBottom );
    }

    auto pickFill = [ & ]( bool hov, bool hel ) -> ImU32
    {
        if ( disabled ) return fillDisabled;
        if ( hel )  return fillPress;
        if ( hov )  return fillHover;
        return fillRest;
    };
    const ImU32 mainTarget = pickFill( main_hovered, main_held );
    const ImU32 mainAnim = AnimateColorU32( id_main, mainTarget );
    const float r = FluentDpx( style.ControlCornerRadius );

    ImDrawList * dl = w->DrawList;
    if ( split )
    {

        dl->AddRectFilled( bb_main.Min, bb_main.Max, mainAnim, r,
                           ImDrawFlags_RoundCornersLeft );
        const ImU32 chevTarget = pickFill( chev_hovered, chev_held );
        const ImU32 chevAnim = AnimateColorU32( id_chev, chevTarget );
        dl->AddRectFilled( bb_chev.Min, bb_chev.Max, chevAnim, r,
                           ImDrawFlags_RoundCornersRight );
        dl->AddLine( ImVec2( bb_main.Max.x, bb_main.Min.y + FluentDpx( style.SpacingMedium ) ),
                     ImVec2( bb_main.Max.x, bb_main.Max.y - FluentDpx( style.SpacingMedium ) ),
                     ImFluent::GetColorU32( ImFluentCol_DividerStrokeDefault ), 1.f );
        if ( !disabled )
            DrawElevationBorder( dl, bb_total, r, strokeCol, strokeBottom, 1.f );
        else
            dl->AddRect( bb_total.Min, bb_total.Max, strokeCol, r, 0, 1.f );
    }
    else
    {
        dl->AddRectFilled( bb_total.Min, bb_total.Max, mainAnim, r );
        if ( !main_held && !disabled )
            DrawElevationBorder( dl, bb_total, r, strokeCol, strokeBottom, 1.f );
        else
            dl->AddRect( bb_total.Min, bb_total.Max, strokeCol, r, 0, 1.f );
    }

    DrawButtonLabel( dl, ImRect( bb_main.Min,
                     ImVec2( bb_main.Max.x - (split ? 0.f : chev_w), bb_main.Max.y ) ),
                     label, disabled ? textDisabled : textCol, pad_x, pad_y );

    {
        const ImVec2 cen( (split ? bb_chev.Min.x + bb_chev.GetWidth() * 0.5f
                          : bb_main.Max.x - chev_w * 0.5f),
                          (bb_total.Min.y + bb_total.Max.y) * 0.5f );
        DrawChevron( dl, cen, ImGuiDir_Down, disabled ? textDisabled : textCol, FluentDpx( style.ChevronGlyphSize ) );
    }

    if ( IsItemFocused( id_main ) ) DrawFocusRing( dl, bb_total, r );

    if ( dropdown_clicked ) *dropdown_clicked = split ? chev_pressed : false;
    if ( v_state && main_pressed && !split ) *v_state = !*v_state;
    return main_pressed;
}

bool ImFluent::DropDownButton( const char * label, const ImVec2 & size )
{
    bool dummy;
    return DropDownButtonEx( label, NULL, &dummy, size, false, false );
}
bool ImFluent::SplitButton( const char * label, bool * dropdown_clicked, const ImVec2 & size )
{
    return DropDownButtonEx( label, NULL, dropdown_clicked, size, true, false );
}
bool ImFluent::ToggleSplitButton( const char * label, bool * v, bool * dropdown_clicked, const ImVec2 & size )
{
    bool main = DropDownButtonEx( label, v, dropdown_clicked, size, true, (v && *v) );
    if ( main && v ) *v = !*v;
    return main;
}

// [SECTION] Selection (Checkbox / RadioButton / ToggleSwitch / RatingControl)

bool ImFluent::CheckboxEx( const char * label, int * v_tri, bool * v_bool )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;

    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float box = FluentDpx( style.CheckboxSize );
    const float gap = FluentDpx( style.SpacingMedium );
    const ImVec2 ts = ImGui::CalcTextSize( label, NULL, true );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect box_bb( pos, ImVec2( pos.x + box, pos.y + box ) );
    const ImRect total_bb( pos, ImVec2( pos.x + box + (ts.x > 0 ? gap + ts.x : 0.f),
                           pos.y + ImMax( box, ts.y ) ) );
    ImGui::ItemSize( total_bb );
    if ( !ImGui::ItemAdd( total_bb, id ) ) return false;

    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( total_bb, id, &hovered, &held );
    if ( pressed )
    {
        if ( v_tri ) { int & s = *v_tri;  s = (s == 1) ? 0 : 1; }
        if ( v_bool ) { *v_bool = !*v_bool; }
    }

    const int  state = v_tri ? *v_tri : (v_bool && *v_bool ? 1 : 0);
    const bool checked = state == 1;
    const bool indet = state < 0;
    const bool disabled = (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;

    ImDrawList * dl = w->DrawList;
    const float r = FluentDpx( style.ControlCornerRadius );

    ImU32 boxFill, boxStroke;
    if ( disabled ) { boxFill = ImFluent::GetColorU32( ImFluentCol_ControlFillDisabled );        boxStroke = ImFluent::GetColorU32( ImFluentCol_ControlStrongStrokeDisabled ); }
    else if ( checked || indet )
    {
        boxFill = held ? ImFluent::GetColorU32( ImFluentCol_AccentFillTertiary )
            : hovered ? ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary )
            : ImFluent::GetColorU32( ImFluentCol_AccentFillDefault );
        boxStroke = boxFill;
    }
    else
    {
        boxFill = held ? ImFluent::GetColorU32( ImFluentCol_ControlAltFillQuarternary )
            : hovered ? ImFluent::GetColorU32( ImFluentCol_ControlAltFillTertiary )
            : ImFluent::GetColorU32( ImFluentCol_ControlAltFillSecondary );
        boxStroke = ImFluent::GetColorU32( ImFluentCol_ControlStrongStrokeDefault );
    }
    const ImU32 fillAnim = AnimateColorU32( id, boxFill );
    const ImU32 strokeAnim = AnimateColorU32( id ^ 0xC1A0, boxStroke );
    dl->AddRectFilled( box_bb.Min, box_bb.Max, fillAnim, r );
    dl->AddRect( box_bb.Min, box_bb.Max, strokeAnim, r, 0, 1.f );

    if ( checked )
    {
        const ImU32 mark = disabled ? ImFluent::GetColorU32( ImFluentCol_TextOnAccentDisabled ) : ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary );
        const ImVec2 a( box_bb.Min.x + box * 0.22f, box_bb.Min.y + box * 0.52f );
        const ImVec2 b( box_bb.Min.x + box * 0.42f, box_bb.Min.y + box * 0.72f );
        const ImVec2 c( box_bb.Min.x + box * 0.78f, box_bb.Min.y + box * 0.32f );
        dl->AddLine( a, b, mark, FluentDpx( style.StrokeMedium ) );
        dl->AddLine( b, c, mark, FluentDpx( style.StrokeMedium ) );
    }
    else if ( indet )
    {
        const ImU32 mark = disabled ? ImFluent::GetColorU32( ImFluentCol_TextOnAccentDisabled ) : ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary );
        dl->AddRectFilled( ImVec2( box_bb.Min.x + box * 0.25f, box_bb.Min.y + box * 0.45f ),
                           ImVec2( box_bb.Max.x - box * 0.25f, box_bb.Max.y - box * 0.45f ),
                           mark, FluentDpx( style.StrokeThin ) );
    }

    if ( ts.x > 0 )
    {
        const ImU32 lc = disabled ? ImFluent::GetColorU32( ImFluentCol_TextDisabled ) : ImFluent::GetColorU32( ImFluentCol_TextPrimary );
        dl->AddText( ImVec2( box_bb.Max.x + gap, pos.y + (box - ts.y) * 0.5f ), lc, label );
    }
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, box_bb, r );
    return pressed;
}

bool ImFluent::Checkbox( const char * label, bool * v ) { return CheckboxEx( label, NULL, v ); }
bool ImFluent::CheckboxTristate( const char * label, int * v ) { return CheckboxEx( label, v, NULL ); }

bool ImFluent::RadioButton( const char * label, bool active )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float D = FluentDpx( style.CheckboxSize );
    const float gap = FluentDpx( style.SpacingMedium );
    const ImVec2 ts = ImGui::CalcTextSize( label, NULL, true );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect ring_bb( pos, ImVec2( pos.x + D, pos.y + D ) );
    const ImRect total_bb( pos, ImVec2( pos.x + D + (ts.x > 0 ? gap + ts.x : 0.f),
                           pos.y + ImMax( D, ts.y ) ) );
    ImGui::ItemSize( total_bb );
    if ( !ImGui::ItemAdd( total_bb, id ) ) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( total_bb, id, &hovered, &held );
    const bool disabled = (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;

    ImDrawList * dl = w->DrawList;
    const ImVec2 c( (ring_bb.Min.x + ring_bb.Max.x) * 0.5f,
                    (ring_bb.Min.y + ring_bb.Max.y) * 0.5f );
    const float ring_r = D * 0.5f;
    const ImU32 strokeCol = disabled ? ImFluent::GetColorU32( ImFluentCol_ControlStrongStrokeDisabled )
        : active ? ImFluent::GetColorU32( ImFluentCol_AccentFillDefault )
        : ImFluent::GetColorU32( ImFluentCol_ControlStrongStrokeDefault );
    const ImU32 ringFill = active ? (held ? ImFluent::GetColorU32( ImFluentCol_AccentFillTertiary )
                                      : hovered ? ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary ) : ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ))
        : (held ? ImFluent::GetColorU32( ImFluentCol_ControlAltFillTertiary )
            : hovered ? ImFluent::GetColorU32( ImFluentCol_ControlAltFillSecondary ) : ImFluent::GetColorU32( ImFluentCol_ControlAltFillTransparent ));
    dl->AddCircleFilled( c, ring_r, AnimateColorU32( id, ringFill ), 32 );
    dl->AddCircle( c, ring_r, AnimateColorU32( id ^ 0xB1A1, strokeCol ), 32, 1.f );
    if ( active )
    {
        const float dot_r = AnimateFloat( id ^ 0xC0FE,
                                          (held ? D * 0.20f : (hovered ? D * 0.30f : D * 0.25f)), 0.083f );
        dl->AddCircleFilled( c, dot_r, ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary ), 24 );
    }
    if ( ts.x > 0 )
    {
        const ImU32 lc = disabled ? ImFluent::GetColorU32( ImFluentCol_TextDisabled ) : ImFluent::GetColorU32( ImFluentCol_TextPrimary );
        dl->AddText( ImVec2( ring_bb.Max.x + gap, pos.y + (D - ts.y) * 0.5f ), lc, label );
    }
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, ring_bb, ring_r + FluentDpx( style.StrokeThin ) );
    return pressed;
}

bool ImFluent::RadioButton( const char * label, int * v, int v_button )
{
    const bool active = v && (*v == v_button);
    if ( RadioButton( label, active ) )
    {
        if ( v ) *v = v_button;
        return true;
    }
    return false;
}

bool ImFluent::RadioButtons( const char * label, int * v, const char * const items[], int items_count, int max_columns )
{
    if ( !items || items_count <= 0 ) return false;
    if ( max_columns < 1 ) max_columns = 1;
    bool changed = false;
    ImGui::PushID( label );
    if ( label && *label )
    {
        DrawAndConsumePendingHeader();
        ImGui::TextUnformatted( label );
    }
    const int rows = (items_count + max_columns - 1) / max_columns;
    for ( int row = 0; row < rows; ++row )
    {
        for ( int col = 0; col < max_columns; ++col )
        {
            const int i = col * rows + row;
            if ( i >= items_count ) continue;
            if ( col > 0 ) ImGui::SameLine();
            if ( RadioButton( items[i], v, i ) ) changed = true;
        }
    }
    DrawAndConsumePendingDescription();
    ImGui::PopID();
    return changed;
}

bool ImFluent::ToggleSwitch( const char * label, bool * v, const char * on_text, const char * off_text )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float W = FluentDpx( style.ToggleSwitchWidth );
    const float H = FluentDpx( style.ToggleSwitchHeight );
    const float gap = FluentDpx( style.SpacingLarge );
    const ImVec2 lab_size = ImGui::CalcTextSize( label, NULL, true );
    const char * state_text = (v && *v) ? on_text : off_text;
    const ImVec2 state_size = state_text ? ImGui::CalcTextSize( state_text, NULL, true ) : ImVec2( 0, 0 );

    const ImVec2 pos = w->DC.CursorPos;
    const ImRect track_bb( pos, ImVec2( pos.x + W, pos.y + H ) );
    const ImRect state_bb( ImVec2( track_bb.Max.x + gap, pos.y ),
                           ImVec2( track_bb.Max.x + gap + state_size.x, pos.y + H ) );
    const float right_x = (state_size.x > 0 ? state_bb.Max.x + (lab_size.x > 0 ? gap : 0) : track_bb.Max.x + (lab_size.x > 0 ? gap : 0));
    const ImRect total_bb( pos, ImVec2( right_x + lab_size.x, pos.y + ImMax( H, ImMax( lab_size.y, state_size.y ) ) ) );
    ImGui::ItemSize( total_bb );
    if ( !ImGui::ItemAdd( total_bb, id ) ) return false;

    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( track_bb, id, &hovered, &held );
    if ( pressed && v ) *v = !*v;
    const bool on = v && *v;
    const bool disabled = (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;

    ImU32 trackTarget;
    if ( disabled ) trackTarget = on ? ImFluent::GetColorU32( ImFluentCol_AccentFillDisabled ) : ImFluent::GetColorU32( ImFluentCol_ControlAltFillDisabled );
    else if ( on )  trackTarget = held ? ImFluent::GetColorU32( ImFluentCol_AccentFillTertiary ) : (hovered ? ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary ) : ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ));
    else          trackTarget = held ? ImFluent::GetColorU32( ImFluentCol_ControlAltFillQuarternary ) : (hovered ? ImFluent::GetColorU32( ImFluentCol_ControlAltFillTertiary ) : ImFluent::GetColorU32( ImFluentCol_ControlAltFillSecondary ));
    const ImU32 trackAnim = AnimateColorU32( id, trackTarget );

    ImDrawList * dl = w->DrawList;
    dl->AddRectFilled( track_bb.Min, track_bb.Max, trackAnim, H * 0.5f );
    if ( !on )
        dl->AddRect( track_bb.Min, track_bb.Max, ImFluent::GetColorU32( ImFluentCol_ControlStrongStrokeDefault ), H * 0.5f, 0, 1.f );

    const float pad = FluentDpx( style.SpacingSmall );
    const float thumb_r_target = on ? FluentDpx( style.ToggleSwitchThumbRadiusOn ) : FluentDpx( style.ToggleSwitchThumbRadiusOff );
    const float thumb_r = AnimateFloat( id ^ 0xA1A1, thumb_r_target, 0.083f );
    const float cx_off = track_bb.Min.x + pad + FluentDpx( style.ToggleSwitchThumbRadiusOff );
    const float cx_on = track_bb.Max.x - pad - FluentDpx( style.ToggleSwitchThumbRadiusOff );
    const float t_anim = AnimateFloat( id ^ 0xB2B2, on ? 1.f : 0.f, 0.083f );
    const float cx = cx_off + (cx_on - cx_off) * t_anim;
    const float cy = (track_bb.Min.y + track_bb.Max.y) * 0.5f;
    const ImU32 thumbColTarget = disabled ? ImFluent::GetColorU32( ImFluentCol_ControlStrongFillDisabled )
        : on ? ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary )
        : ImFluent::GetColorU32( ImFluentCol_ControlStrongFillDefault );
    const ImU32 thumbCol = AnimateColorU32( id ^ 0xD3D3, thumbColTarget );
    dl->AddCircleFilled( ImVec2( cx, cy ), thumb_r, thumbCol, 24 );

    float cursor_x = track_bb.Max.x + gap;
    if ( state_size.x > 0 )
    {
        dl->AddText( ImVec2( cursor_x, pos.y + (H - state_size.y) * 0.5f ),
                     disabled ? ImFluent::GetColorU32( ImFluentCol_TextDisabled ) : ImFluent::GetColorU32( ImFluentCol_TextSecondary ), state_text );
        cursor_x += state_size.x + gap;
    }
    if ( lab_size.x > 0 )
        dl->AddText( ImVec2( cursor_x, pos.y + (H - lab_size.y) * 0.5f ),
                     disabled ? ImFluent::GetColorU32( ImFluentCol_TextDisabled ) : ImFluent::GetColorU32( ImFluentCol_TextPrimary ), label );
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, track_bb, H * 0.5f );
    return pressed;
}

bool ImFluent::RatingControl( const char * label, float * value, int max_stars )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float star = FluentDpx( style.RatingStarSize );
    const float gap = FluentDpx( style.SpacingSmall );
    const ImVec2 pos = w->DC.CursorPos;
    const float total_w = max_stars * star + (max_stars - 1) * gap;
    const ImRect bb( pos, ImVec2( pos.x + total_w, pos.y + star ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;

    const bool hovered = ImGui::ItemHoverable( bb, id, ImGuiItemFlags_None );
    bool changed = false;
    if ( hovered && ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
    {
        const float mx = ImGui::GetMousePos().x - bb.Min.x;
        float new_val = (mx / (star + gap));
        if ( new_val < 0 ) new_val = 0;
        if ( new_val > ( float )max_stars ) new_val = ( float )max_stars;

        const float snapped = ( float )(( int )(new_val * 2.f + 0.5f)) * 0.5f;
        if ( value && *value != snapped ) { *value = snapped; changed = true; }
    }
    const float v = value ? *value : 0.f;
    ImDrawList * dl = w->DrawList;
    for ( int i = 0; i < max_stars; ++i )
    {
        const ImVec2 c( pos.x + i * (star + gap) + star * 0.5f, pos.y + star * 0.5f );
        const float fill = ImClamp( v - ( float )i, 0.f, 1.f );
        const ImU32 trackCol = ImFluent::GetColorU32( ImFluentCol_ControlStrongStrokeDefault );
        const ImU32 fillCol = ImFluent::GetColorU32( ImFluentCol_AccentFillDefault );

        dl->AddNgon( c, star * 0.45f, trackCol, 5, 1.5f );
        if ( fill > 0.f )
        {
            dl->PushClipRect( ImVec2( c.x - star * 0.5f, c.y - star * 0.5f ),
                              ImVec2( c.x - star * 0.5f + star * fill, c.y + star * 0.5f ), true );
            dl->AddNgonFilled( c, star * 0.45f, fillCol, 5 );
            dl->PopClipRect();
        }
    }
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, FluentDpx( style.ControlCornerRadius ) );
    return changed;
}

// [SECTION] Sliders & Progress

bool ImFluent::Slider( const char * label, ImGuiDataType dtype, void * v, const void * v_min, const void * v_max, const char * format, ImGuiSliderFlags flags )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    DrawAndConsumePendingHeader();
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float h = FluentDpx( style.ControlHeight );
    const float w_avail = ImGui::CalcItemWidth();
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect frame_bb( pos, ImVec2( pos.x + w_avail, pos.y + h ) );

    ImGui::ItemSize( frame_bb );
    if ( !ImGui::ItemAdd( frame_bb, id ) ) return false;

    ImGuiContext & g = *ImGui::GetCurrentContext();
    const bool hovered = ImGui::ItemHoverable( frame_bb, id, g.LastItemData.ItemFlags );
    const bool clicked = hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left );
    if ( clicked || g.NavActivateId == id )
    {
        ImGui::SetActiveID( id, w );
        ImGui::SetFocusID( id, w );
        ImGui::FocusWindow( w );
        g.ActiveIdUsingNavDirMask |= (1u << ImGuiDir_Left) | (1u << ImGuiDir_Right);
    }

    ImRect grab_bb;
    const bool changed = ImGui::SliderBehavior( frame_bb, id, dtype, v, v_min, v_max, format, flags, &grab_bb );
    if ( changed ) ImGui::MarkItemEdited( id );

    const bool active = ImGui::IsItemActive();

    ImDrawList * dl = w->DrawList;
    const float track_h = FluentDpx( style.SliderTrackHeight );
    const ImVec2 track_min( frame_bb.Min.x, (frame_bb.Min.y + frame_bb.Max.y) * 0.5f - track_h * 0.5f );
    const ImVec2 track_max( frame_bb.Max.x, track_min.y + track_h );
    dl->AddRectFilled( track_min, track_max, ImFluent::GetColorU32( ImFluentCol_ControlStrongFillDefault ), track_h * 0.5f );
    const float thumb_cx = (grab_bb.Min.x + grab_bb.Max.x) * 0.5f;
    if ( thumb_cx > track_min.x )
        dl->AddRectFilled( track_min, ImVec2( thumb_cx, track_max.y ), ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), track_h * 0.5f );

    const float thumb_r = FluentDpx( style.SliderThumbRadius );
    const float inner_rest = FluentDpx( style.SliderThumbInnerRadius );
    const float inner_delta = FluentDpx( 1.f );
    const float inner_target = active ? inner_rest - inner_delta
        : hovered ? inner_rest + inner_delta
        : inner_rest;
    const float inner_r = AnimateFloat( id ^ 0xC110, inner_target, 0.083f );
    const ImVec2 c( thumb_cx, (track_min.y + track_max.y) * 0.5f );
    dl->AddCircleFilled( c, thumb_r, ImFluent::GetColorU32( ImFluentCol_ControlSolidFillDefault ), 32 );
    dl->AddCircleFilled( c, inner_r, ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), 24 );

    if ( IsItemFocused( id ) ) DrawFocusRing( dl, frame_bb, FluentDpx( style.ControlCornerRadius ) );
    DrawAndConsumePendingDescription();
    return changed;
}

bool ImFluent::Slider( const char * label, float * v, float v_min, float v_max, const char * format, ImGuiSliderFlags flags )
{
    return Slider( label, ImGuiDataType_Float, v, &v_min, &v_max, format, flags );
}
bool ImFluent::SliderInt( const char * label, int * v, int v_min, int v_max, const char * format, ImGuiSliderFlags flags )
{
    return Slider( label, ImGuiDataType_S32, v, &v_min, &v_max, format, flags );
}

bool ImFluent::RangeSlider( const char * label, float * v_min, float * v_max, float v_lo, float v_hi, const char * format )
{
    if ( !v_min || !v_max || v_hi <= v_lo ) return false;
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    DrawAndConsumePendingHeader();
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id_lo = w->GetID( ImGui::GetID( label ) );
    const ImGuiID id_hi = w->GetID( ( const void * )(( intptr_t )id_lo + 1) );
    const float h = FluentDpx( style.ControlHeight );
    const float w_avail = ImGui::CalcItemWidth();
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect frame_bb( pos, ImVec2( pos.x + w_avail, pos.y + h ) );
    ImGui::ItemSize( frame_bb );
    if ( !ImGui::ItemAdd( frame_bb, id_lo ) ) return false;
    ImGui::SetNextItemAllowOverlap();
    ImGui::ItemAdd( frame_bb, id_hi );

    const float thumb_r = FluentDpx( style.SliderThumbRadius );
    const float track_h = FluentDpx( style.SliderTrackHeight );
    const float track_left = frame_bb.Min.x + thumb_r;
    const float track_right = frame_bb.Max.x - thumb_r;
    const float track_w = ImMax( 1.f, track_right - track_left );
    const float track_y = (frame_bb.Min.y + frame_bb.Max.y) * 0.5f;
    const float range = v_hi - v_lo;

    auto value_to_x = [ & ]( float val )
    {
        return track_left + ((val - v_lo) / range) * track_w;
    };
    auto x_to_value = [ & ]( float x )
    {
        return v_lo + ImClamp( (x - track_left) / track_w, 0.f, 1.f ) * range;
    };

    const float lo_x = value_to_x( *v_min );
    const float hi_x = value_to_x( *v_max );

    const ImRect lo_bb( ImVec2( lo_x - thumb_r, track_y - thumb_r ), ImVec2( lo_x + thumb_r, track_y + thumb_r ) );
    const ImRect hi_bb( ImVec2( hi_x - thumb_r, track_y - thumb_r ), ImVec2( hi_x + thumb_r, track_y + thumb_r ) );

    bool changed = false;
    bool lo_hov = false, lo_held = false;
    bool hi_hov = false, hi_held = false;
    ImGui::ButtonBehavior( lo_bb, id_lo, &lo_hov, &lo_held );
    ImGui::ButtonBehavior( hi_bb, id_hi, &hi_hov, &hi_held );
    if ( lo_held )
    {
        const float nv = ImClamp( x_to_value( ImGui::GetMousePos().x ), v_lo, *v_max );
        if ( nv != *v_min ) { *v_min = nv; changed = true; }
    }
    if ( hi_held )
    {
        const float nv = ImClamp( x_to_value( ImGui::GetMousePos().x ), *v_min, v_hi );
        if ( nv != *v_max ) { *v_max = nv; changed = true; }
    }

    ImDrawList * dl = w->DrawList;
    dl->AddRectFilled( ImVec2( track_left, track_y - track_h * 0.5f ),
                       ImVec2( track_right, track_y + track_h * 0.5f ),
                       ImFluent::GetColorU32( ImFluentCol_ControlStrongFillDefault ), track_h * 0.5f );
    dl->AddRectFilled( ImVec2( lo_x, track_y - track_h * 0.5f ),
                       ImVec2( hi_x, track_y + track_h * 0.5f ),
                       ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), track_h * 0.5f );

    const float inner_rest = FluentDpx( style.SliderThumbInnerRadius );
    for ( int side = 0; side < 2; ++side )
    {
        const ImVec2 c = (side == 0) ? ImVec2( lo_x, track_y ) : ImVec2( hi_x, track_y );
        const bool   hov = (side == 0) ? lo_hov : hi_hov;
        const bool   act = (side == 0) ? lo_held : hi_held;
        const float inner_target = act ? inner_rest - FluentDpx( style.SpacingXSmall )
            : hov ? inner_rest + FluentDpx( style.SpacingXSmall )
            : inner_rest;
        const float inner_r = AnimateFloat( (side == 0 ? id_lo : id_hi) ^ 0xC110, inner_target, 0.083f );
        dl->AddCircleFilled( c, thumb_r, ImFluent::GetColorU32( ImFluentCol_ControlSolidFillDefault ), 32 );
        dl->AddCircle( c, thumb_r, ImFluent::GetColorU32( ImFluentCol_ControlStrokeDefault ), 32, FluentDpx( style.StrokeThin ) );
        dl->AddCircleFilled( c, inner_r, ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), 24 );
    }

    if ( format && *format )
    {
        const char * txt; const char * txt_end;
        ImFormatStringToTempBuffer( &txt, &txt_end, format, *v_min );
        ImGui::SameLine();
        ImGui::TextUnformatted( txt, txt_end );
        ImGui::SameLine();
        ImGui::TextUnformatted( "\xe2\x80\x93" );
        ImGui::SameLine();
        ImFormatStringToTempBuffer( &txt, &txt_end, format, *v_max );
        ImGui::TextUnformatted( txt, txt_end );
    }

    DrawAndConsumePendingDescription();
    return changed;
}

void ImFluent::ProgressBar( float fraction, const ImVec2 & size_arg, const char * overlay, ImFluentProgressBarState state )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return;
    const ImFluentStyle & style = ImFluent::GetStyle();

    const float h = (size_arg.y > 0 ? size_arg.y : FluentDpx( style.ProgressBarHeight ));
    const float W = (size_arg.x < 0.f) ? ImGui::GetContentRegionAvail().x
        : (size_arg.x > 0.f) ? size_arg.x
        : ImGui::CalcItemWidth();
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + W, pos.y + h ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, 0 ) ) return;
    if ( fraction < 0 ) fraction = 0;
    if ( fraction > 1 ) fraction = 1;

    const ImU32 fill_col = (state == ImFluentProgressBarState_Error) ? ImFluent::GetColorU32( ImFluentCol_SystemFillCritical )
        : (state == ImFluentProgressBarState_Paused) ? ImFluent::GetColorU32( ImFluentCol_SystemFillCaution )
        : ImFluent::GetColorU32( ImFluentCol_AccentFillDefault );

    ImDrawList * dl = w->DrawList;
    dl->AddRectFilled( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_ControlStrongFillDefault, 0.20f ), h * 0.5f );
    if ( fraction > 0.f )
        dl->AddRectFilled( bb.Min, ImVec2( bb.Min.x + W * fraction, bb.Max.y ), fill_col, h * 0.5f );
    if ( overlay )
    {
        const ImVec2 ts = ImGui::CalcTextSize( overlay );
        dl->AddText( ImVec2( bb.Min.x + (W - ts.x) * 0.5f, bb.Max.y + FluentDpx( style.SpacingSmall ) ),
                     ImFluent::GetColorU32( ImFluentCol_TextSecondary ), overlay );
    }
}

void ImFluent::ProgressRing( float diameter_dpx, float fraction )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float D = FluentDpx( diameter_dpx );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + D, pos.y + D ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, 0 ) ) return;
    ImDrawList * dl = w->DrawList;
    const ImVec2 c( pos.x + D * 0.5f, pos.y + D * 0.5f );
    const float radius = D * 0.5f - FluentDpx( style.SpacingXSmall );
    const float thickness = FluentDpx( style.ProgressRingThickness );
    if ( fraction < 0.f )
    {
        const float t              = ( float )ImGui::GetTime();
        const float period         = 1.333f;
        const float sweep_per_half = IM_PI * 1.5f;
        const int   cycle_index    = ( int )(t / period);
        const float phase          = (t - cycle_index * period) / period;
        const float base_rotation  = t * (IM_PI * 0.5f);
        const float cycle_anchor   = cycle_index * sweep_per_half;

        const float head_p_raw = ImClamp( phase * 2.f,           0.f, 1.f );
        const float tail_p_raw = ImClamp( (phase - 0.5f) * 2.f,  0.f, 1.f );
        const float head_p     = (head_p_raw < 0.5f)
            ? 2.f * head_p_raw * head_p_raw
            : 1.f - 2.f * (1.f - head_p_raw) * (1.f - head_p_raw);
        const float tail_p     = (tail_p_raw < 0.5f)
            ? 2.f * tail_p_raw * tail_p_raw
            : 1.f - 2.f * (1.f - tail_p_raw) * (1.f - tail_p_raw);

        const float tail_angle = base_rotation + cycle_anchor + tail_p * sweep_per_half;
        const float head_angle = base_rotation + cycle_anchor + head_p * sweep_per_half;
        const float arc_span   = head_angle - tail_angle;

        if ( arc_span > 0.0005f )
        {
            const int segs = 48;
            dl->PathClear();
            for ( int i = 0; i <= segs; ++i )
            {
                const float a = tail_angle + (arc_span * ( float )i / ( float )segs);
                dl->PathLineTo( ImVec2( c.x + cosf( a ) * radius, c.y + sinf( a ) * radius ) );
            }
            dl->PathStroke( ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), 0, thickness );
        }
    }
    else
    {
        if ( fraction > 1.f ) fraction = 1.f;
        const int segs = 64;

        dl->AddCircle( c, radius, ImFluent::GetColorU32( ImFluentCol_ControlStrongFillDefault, 0.20f ), segs, thickness );
        if ( fraction > 0.f )
        {
            const float a0 = -IM_PI * 0.5f;
            const float a1 = a0 + IM_PI * 2.f * fraction;
            dl->PathClear();
            dl->PathArcTo( c, radius, a0, a1, ( int )(segs * fraction) );
            dl->PathStroke( ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), 0, thickness );
        }
    }
}

// [SECTION] Text input (TextBox / PasswordBox / NumberBox / RichEditBox / AutoSuggestBox / TextBlock)

bool ImFluent::TextBox( const char * label, char * buf, size_t buf_size, const char * hint, ImGuiInputTextFlags extra_flags, ImFluentTextBoxFlags fluent_flags, int max_length )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    DrawAndConsumePendingHeader();

    const ImFluentStyle & style = ImFluent::GetStyle();
    PushControlFrameStyle( FluentDpx( style.ControlHeight ) );

    struct CharLimit { char * Buf; int Max; };
    CharLimit limit = { buf, max_length };
    ImGuiInputTextCallback cb = NULL;
    void * cb_user = NULL;
    if ( max_length > 0 )
    {
        extra_flags |= ImGuiInputTextFlags_CallbackCharFilter;
        cb = []( ImGuiInputTextCallbackData * data ) -> int
        {
            const CharLimit * lim = ( const CharLimit * )data->UserData;
            if ( !lim || data->EventFlag != ImGuiInputTextFlags_CallbackCharFilter ) return 0;
            int n_chars = 0;
            for ( const char * p = lim->Buf; *p; ++p )
                if ( ( *p & 0xC0 ) != 0x80 ) ++n_chars;
            return ( n_chars >= lim->Max ) ? 1 : 0;
        };
        cb_user = &limit;
    }

    if ( ( fluent_flags & ImFluentTextBoxFlags_ClearButton ) && buf && buf[0] )
        ImGui::SetNextItemAllowOverlap();

    bool changed = hint
        ? ImGui::InputTextWithHint( label, hint, buf, buf_size, extra_flags, cb, cb_user )
        : ImGui::InputText( label, buf, buf_size, extra_flags, cb, cb_user );

    PopControlFrameStyle();

    const ImRect input_bb = ImGui::GetCurrentContext()->LastItemData.Rect;
    const ImGuiID input_id = ImGui::GetCurrentContext()->LastItemData.ID;
    const bool   active   = ImGui::IsItemActive();

    if ( ( fluent_flags & ImFluentTextBoxFlags_ClearButton ) && buf && buf[0] )
    {
        const float btn_w = FluentDpx( style.RevealButtonWidth );
        const float inset = FluentDpx( style.SpacingXSmall );
        const ImRect btn_bb(
            ImVec2( input_bb.Max.x - btn_w - inset, input_bb.Min.y + inset ),
            ImVec2( input_bb.Max.x - inset,         input_bb.Max.y - inset ) );
        const ImGuiID id = input_id ^ 0xC1EA8u;
        ImGui::KeepAliveID( id );
        bool hov = false, held = false;
        const bool added = ImGui::ItemAdd( btn_bb, id );
        const bool pr = added && ImGui::ButtonBehavior( btn_bb, id, &hov, &held );
        ImDrawList * dl = w->DrawList;
        const float r = FluentDpx( style.ControlCornerRadius );
        const ImU32 clear_target = (hov || held)
            ? (held ? ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary )
                    : ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary ))
            : ImFluent::GetColorU32( ImFluentCol_SubtleFillTransparent );
        const ImU32 clear_anim = ImFluent::AnimateColorU32( id, clear_target );
        if ( hov || held )
        {
            const ImU32 frame_bg = active ? ImFluent::GetColorU32( ImFluentCol_ControlFillInputActive )
                                          : ImFluent::GetColorU32( ImFluentCol_ControlFillDefault );
            dl->AddRectFilled( btn_bb.Min, btn_bb.Max, ImFluent::GetColorU32( ImFluentCol_LayerFillAlt ),
                               r, ImDrawFlags_RoundCornersRight );
            dl->AddRectFilled( btn_bb.Min, btn_bb.Max, frame_bg, r, ImDrawFlags_RoundCornersRight );
        }
        dl->AddRectFilled( btn_bb.Min, btn_bb.Max, clear_anim, r, ImDrawFlags_RoundCornersRight );
        const char * x_glyph = ImFluentIcon_Cancel;
        const ImVec2 ts = ImGui::CalcTextSize( x_glyph );
        dl->AddText( ImVec2( btn_bb.Min.x + ( btn_bb.GetWidth() - ts.x ) * 0.5f,
                             btn_bb.Min.y + ( btn_bb.GetHeight() - ts.y ) * 0.5f ),
                     ImFluent::GetColorU32( ImFluentCol_TextSecondary ), x_glyph );
        if ( pr )
        {
            buf[0] = 0;
            changed = true;
        }
    }

    if ( active && !HasPendingError() )
    {
        ImDrawList * dl = w->DrawList;
        const float t = FluentDpx( style.SpacingXSmall );
        dl->AddRectFilled( ImVec2( input_bb.Min.x + FluentDpx( style.ControlCornerRadius ), input_bb.Max.y - t ),
                           ImVec2( input_bb.Max.x - FluentDpx( style.ControlCornerRadius ), input_bb.Max.y ),
                           ImFluent::GetColorU32( ImFluentCol_ElevationTextControlFocusedBottom ) );
    }

    if ( ( fluent_flags & ImFluentTextBoxFlags_ShowCounter ) && max_length > 0 && buf )
    {
        int n_chars = 0;
        for ( const char * p = buf; *p; ++p )
            if ( ( *p & 0xC0 ) != 0x80 ) ++n_chars;
        const char * counter; const char * counter_end;
        ImFormatStringToTempBuffer( &counter, &counter_end, "%d / %d", n_chars, max_length );
        const ImVec2 ts = ImGui::CalcTextSize( counter, counter_end );
        ImFluent::PushFont( ImFluentTextStyle_Caption );
        ImGui::PushStyleColor( ImGuiCol_Text, ImFluent::GetColorU32( ImFluentCol_TextSecondary ) );
        ImGui::SetCursorScreenPos( ImVec2( input_bb.Max.x - ts.x, input_bb.Max.y + FluentDpx( style.SpacingXSmall ) ) );
        ImGui::TextUnformatted( counter, counter_end );
        ImGui::PopStyleColor();
        ImFluent::PopFont();
    }

    DrawAndConsumePendingDescription();
    DrawAndConsumePendingError( input_bb );
    return changed;
}

bool ImFluent::PasswordBox( const char * label, char * buf, size_t buf_size, const char * hint, ImGuiInputTextFlags flags )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;

    DrawAndConsumePendingHeader();

    const ImFluentStyle & style = ImFluent::GetStyle();
    const float h = FluentDpx( style.ControlHeight );

    const ImVec2 cursor = w->DC.CursorPos;
    const float  full_w = ImGui::CalcItemWidth();
    const ImRect input_predicted = ImRect( cursor, ImVec2( cursor.x + full_w, cursor.y + h ) );
    const float  reveal_w = FluentDpx( style.RevealButtonWidth );
    const float  inset = FluentDpx( style.SpacingXSmall );
    const ImRect btn_pred(
        ImVec2( input_predicted.Max.x - reveal_w - inset, input_predicted.Min.y + inset ),
        ImVec2( input_predicted.Max.x - inset, input_predicted.Max.y - inset ) );

    const bool hov = ImGui::IsMouseHoveringRect( btn_pred.Min, btn_pred.Max );
    const bool held = hov && ImGui::IsMouseDown( ImGuiMouseButton_Left );
    const bool revealed = held;

    const ImGuiInputTextFlags fl = revealed ? flags : (flags | ImGuiInputTextFlags_Password);

    PushControlFrameStyle( h );
    const bool changed = hint ? ImGui::InputTextWithHint( label, hint, buf, buf_size, fl )
        : ImGui::InputText( label, buf, buf_size, fl );
    PopControlFrameStyle();

    const bool   input_active = ImGui::IsItemActive();
    const ImRect input_rect = ImGui::GetCurrentContext()->LastItemData.Rect;

    if ( input_active && !HasPendingError() )
    {
        ImDrawList * dl0 = w->DrawList;
        const float t = FluentDpx( style.SpacingXSmall );
        dl0->AddRectFilled( ImVec2( input_rect.Min.x + FluentDpx( style.ControlCornerRadius ), input_rect.Max.y - t ),
                            ImVec2( input_rect.Max.x - FluentDpx( style.ControlCornerRadius ), input_rect.Max.y ),
                            ImFluent::GetColorU32( ImFluentCol_ElevationTextControlFocusedBottom ) );
    }

    const ImRect btn_bb(
        ImVec2( input_rect.Max.x - reveal_w - inset, input_rect.Min.y + inset ),
        ImVec2( input_rect.Max.x - inset, input_rect.Max.y - inset ) );

    ImDrawList * dl = w->DrawList;
    const float  r = FluentDpx( style.ControlCornerRadius );
    if ( hov || held )
    {
        const ImU32 frame_bg = input_active ? ImFluent::GetColorU32( ImFluentCol_ControlFillInputActive )
            : ImFluent::GetColorU32( ImFluentCol_ControlFillDefault );
        dl->AddRectFilled( btn_bb.Min, btn_bb.Max,
                           ImFluent::GetColorU32( ImFluentCol_LayerFillAlt ),
                           r, ImDrawFlags_RoundCornersRight );
        dl->AddRectFilled( btn_bb.Min, btn_bb.Max, frame_bg, r, ImDrawFlags_RoundCornersRight );
        const ImU32 hi = held ? ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary )
            : ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary );
        dl->AddRectFilled( btn_bb.Min, btn_bb.Max, hi, r, ImDrawFlags_RoundCornersRight );
    }

    const char * eye_glyph = ImFluentIcon_View;
    const ImVec2 ts = ImGui::CalcTextSize( eye_glyph );
    dl->AddText( ImVec2( btn_bb.Min.x + (btn_bb.GetWidth() - ts.x) * 0.5f,
                 btn_bb.Min.y + (btn_bb.GetHeight() - ts.y) * 0.5f ),
                 ImFluent::GetColorU32( ImFluentCol_TextSecondary ), eye_glyph );

    DrawAndConsumePendingDescription();
    DrawAndConsumePendingError( input_rect );
    return changed;
}

bool ImFluent::NumberBox( const char * label, double * v, double step, double step_fast, const char * format, ImGuiInputTextFlags flags )
{
    DrawAndConsumePendingHeader();
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;

    const ImFluentStyle & style = ImFluent::GetStyle();
    const float h = FluentDpx( style.ControlHeight );

    PushControlFrameStyle( h );

    ImGui::SetNextItemAllowOverlap();

    bool changed = ImGui::InputDouble( label, v, 0.0, 0.0, format, flags );
    PopControlFrameStyle();

    const bool   input_active = ImGui::IsItemActive();
    const ImRect input_rect = ImGui::GetCurrentContext()->LastItemData.Rect;
    const ImGuiID input_id = ImGui::GetCurrentContext()->LastItemData.ID;

    const float  spin_w = FluentDpx( style.SpinButtonWidth );
    const float  inset = FluentDpx( style.SpacingXSmall );
    const ImRect spin_area(
        ImVec2( input_rect.Max.x - spin_w - inset, input_rect.Min.y + inset ),
        ImVec2( input_rect.Max.x - inset, input_rect.Max.y - inset ) );
    const float  btn_h = (spin_area.GetHeight() - inset) * 0.5f;
    const ImRect up_bb( spin_area.Min, ImVec2( spin_area.Max.x, spin_area.Min.y + btn_h ) );
    const ImRect dn_bb( ImVec2( spin_area.Min.x, spin_area.Max.y - btn_h ), spin_area.Max );

    ImDrawList * dl = w->DrawList;
    const float  r = FluentDpx( style.ControlCornerRadius );

    ImGui::PushItemFlag( ImGuiItemFlags_ButtonRepeat, true );

    struct SpinBtn { const ImRect bb; const ImGuiID id; const ImGuiDir dir; const double sign; };
    const SpinBtn btns[2] = {
        { up_bb, input_id ^ 0x5817u, ImGuiDir_Up,   +1.0 },
        { dn_bb, input_id ^ 0xD0E1u, ImGuiDir_Down, -1.0 },
    };

    bool b_state[2][2] = {};
    for ( int i = 0; i < 2; ++i )
    {
        const SpinBtn & b = btns[i];
        ImGui::ItemAdd( b.bb, b.id );
        bool hov = false, held = false;
        const bool pr = ImGui::ButtonBehavior( b.bb, b.id, &hov, &held );
        if ( pr && v )
        {
            const double s = ImGui::GetIO().KeyCtrl ? step_fast : step;
            *v += b.sign * s;
            changed = true;
        }
        b_state[i][0] = hov;
        b_state[i][1] = held;
    }

    const ImU32 spin_layer_on = ImFluent::GetColorU32( ImFluentCol_LayerFillAlt );
    const ImU32 spin_frame_on = input_active ? ImFluent::GetColorU32( ImFluentCol_ControlFillInputActive )
        : ImFluent::GetColorU32( ImFluentCol_ControlFillDefault );
    const ImU32 spin_transparent = IM_COL32( 0, 0, 0, 0 );

    for ( int i = 0; i < 2; ++i )
    {
        const SpinBtn & b = btns[i];
        const bool      hov = b_state[i][0];
        const bool      held = b_state[i][1];

        const ImU32 layer_target = (hov || held) ? spin_layer_on : spin_transparent;
        const ImU32 frame_target = (hov || held) ? spin_frame_on : spin_transparent;
        const ImU32 hi_target    = held ? ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary )
                                 : hov  ? ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary )
                                        : spin_transparent;

        dl->AddRectFilled( b.bb.Min, b.bb.Max, ImFluent::AnimateColorU32( b.id ^ 0xA1A1, layer_target ), r );
        dl->AddRectFilled( b.bb.Min, b.bb.Max, ImFluent::AnimateColorU32( b.id ^ 0xB2B2, frame_target ), r );
        dl->AddRectFilled( b.bb.Min, b.bb.Max, ImFluent::AnimateColorU32( b.id,           hi_target    ), r );

        const ImVec2 c( (b.bb.Min.x + b.bb.Max.x) * 0.5f,
                        (b.bb.Min.y + b.bb.Max.y) * 0.5f );
        DrawChevron( dl, c, b.dir, ImFluent::GetColorU32( ImFluentCol_TextSecondary ), FluentDpx( style.ChevronGlyphSize - 1.f ) );
    }

    ImGui::PopItemFlag();
    DrawAndConsumePendingDescription();
    DrawAndConsumePendingError( input_rect );
    return changed;
}

bool ImFluent::RichEditBox( const char * label, char * buf, size_t buf_size, const ImVec2 & size, ImGuiInputTextFlags flags )
{
    DrawAndConsumePendingHeader();
    const ImFluentStyle & style = ImFluent::GetStyle();

    PushControlFrameStyle( ImGui::GetFontSize() + FluentDpx( style.SpacingXLarge ) );
    const bool changed = ImGui::InputTextMultiline( label, buf, buf_size, size, flags );
    PopControlFrameStyle();
    const ImRect input_rect = ImGui::GetCurrentContext()->LastItemData.Rect;
    DrawAndConsumePendingDescription();
    DrawAndConsumePendingError( input_rect );
    return changed;
}

bool ImFluent::AutoSuggestBox( const char * label, char * buf, size_t buf_size, const char * const items[], int items_count, int * selected_index, const char * hint, ImGuiInputTextFlags flags )
{
    DrawAndConsumePendingHeader();
    const ImFluentStyle & style = ImFluent::GetStyle();
    bool changed = TextBox( label, buf, buf_size, hint, flags );

    const ImGuiID input_id = ImGui::GetCurrentContext()->LastItemData.ID;
    const ImRect  input_rect = ImGui::GetCurrentContext()->LastItemData.Rect;
    const bool    input_activated = ImGui::IsItemActivated();

    const ImGuiID popup_id = input_id ^ 0xA17051E5u;

    if ( input_activated && items_count > 0 )
        ImGui::OpenPopupEx( popup_id, ImGuiPopupFlags_None );

    if ( items_count > 0 )
    {
        ImGui::SetNextWindowPos( ImVec2( input_rect.Min.x, input_rect.Max.y + FluentDpx( style.SpacingXSmall ) ) );
        ImGui::SetNextWindowSize( ImVec2( input_rect.GetWidth(), 0.f ) );
        PushOverlayWindowStyle( 4.f, 4.f );

        const ImGuiWindowFlags wflags = ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav;
        if ( ImGui::BeginPopupEx( popup_id, wflags ) )
        {
            int shown = 0;
            for ( int i = 0; i < items_count; ++i )
            {
                if ( !items[i] || !*items[i] ) continue;
                if ( buf[0] && !ImStristr( items[i], NULL, buf, NULL ) ) continue;
                if ( ImGui::Selectable( items[i] ) )
                {
                    ImStrncpy( buf, items[i], buf_size );
                    if ( selected_index ) *selected_index = i;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                ++shown;
            }
            if ( shown == 0 )
                ImGui::TextDisabled( "%s", ImFluent::LocalizeGetMsg( ImFluentLocKey_AutoSuggestNoSuggestions ) );
            ImGui::EndPopup();
        }
        PopOverlayWindowStyle();
    }

    DrawAndConsumePendingDescription();
    return changed;
}

void ImFluent::TextBlock( const char * text, ImFluentTextStyle style )
{
    ImGui::AlignTextToFramePadding();

    PushFont( style );
    ImGui::TextUnformatted( text );
    PopFont();
}

void ImFluent::TextBlockColored( const char * text, ImU32 col, ImFluentTextStyle style )
{
    ImGui::AlignTextToFramePadding();

    PushFont( style );
    ImGui::PushStyleColor( ImGuiCol_Text, col );
    ImGui::TextUnformatted( text );
    ImGui::PopStyleColor();
    PopFont();
}

// [SECTION] Separator & Tooltip

void ImFluent::Separator()
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float thickness = 1.f;
    const ImVec2 pos = w->DC.CursorPos;
    const float W = ImGui::GetContentRegionAvail().x;
    const ImRect bb( pos, ImVec2( pos.x + W, pos.y + thickness ) );
    ImGui::ItemSize( ImVec2( W, thickness + ImGui::GetStyle().ItemSpacing.y ) );
    if ( !ImGui::ItemAdd( bb, 0 ) ) return;
    w->DrawList->AddRectFilled( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_DividerStrokeDefault ) );
}

void ImFluent::SetItemTooltip( const char * fmt, ... )
{
    if ( !ImGui::IsItemHovered( ImGuiHoveredFlags_ForTooltip ) ) return;
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImFluentStackGuard g;
    g.PushStyleColor( ImGuiCol_PopupBg, ImFluent::GetColorU32( ImFluentCol_SolidBgQuarternary ) );
    g.PushStyleColor( ImGuiCol_Border, ImFluent::GetColorU32( ImFluentCol_SurfaceStrokeFlyout ) );
    g.PushStyleVar( ImGuiStyleVar_PopupRounding, FluentDpx( style.OverlayCornerRadius ) );
    g.PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( style.SpacingLarge ), FluentDpx( style.SpacingMedium ) ) );
    va_list ap;
    va_start( ap, fmt );
    ImGui::SetTooltipV( fmt, ap );
    va_end( ap );
}

// [SECTION] Card & SettingsCard

bool ImFluent::BeginCard( const char * id, const ImVec2 & size, ImFluentCardStyle card_style )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImU32 bg = (card_style == ImFluentCardStyle_Outlined) ? ImFluent::GetColorU32( ImFluentCol_CardBgSecondary ) : ImFluent::GetColorU32( ImFluentCol_CardBgDefault );
    const ImU32 stroke = ImFluent::GetColorU32( ImFluentCol_CardStrokeDefault );
    ImGui::PushStyleColor( ImGuiCol_ChildBg, bg );
    ImGui::PushStyleColor( ImGuiCol_Border, stroke );
    ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, FluentDpx( style.OverlayCornerRadius ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ChildBorderSize, 1.f );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( style.CardPadding ), FluentDpx( style.CardPadding ) ) );
    return ImGui::BeginChild( id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY );
}

void ImFluent::EndCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar( 3 );
    ImGui::PopStyleColor( 2 );
}

bool ImFluent::BeginSettingsCard( const char * id, const char * header, const char * description, const char * glyph )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    const bool card_open = BeginCard( id, ImVec2( -FLT_MIN, 0 ), ImFluentCardStyle_Filled );
    if ( !card_open )
    {
        EndCard();
        return false;
    }
    ImGui::PushID( id );

    const float font_h = ImGui::GetFontSize();
    const float gap_y = description ? FluentDpx( style.SpacingXSmall ) : 0.f;
    const float text_h = font_h + (description ? font_h + gap_y : 0.f);
    const float control_h = FluentDpx( style.ControlHeight );
    const float row_h = ImMax( text_h, control_h );
    const float glyph_w = glyph ? FluentDpx( style.SpinButtonWidth ) : 0.f;
    const float glyph_gap = glyph ? FluentDpx( style.SpacingLarge ) : 0.f;
    const float trail_w = FluentDpx( style.ControlMinWidth );

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float  avail_w = ImGui::GetContentRegionAvail().x;
    ImDrawList * dl = ImGui::GetWindowDrawList();

    if ( glyph )
    {
        const ImVec2 gs = ImGui::CalcTextSize( glyph );
        dl->AddText( ImVec2( pos.x + (glyph_w - gs.x) * 0.5f,
                     pos.y + (row_h - gs.y) * 0.5f ),
                     ImFluent::GetColorU32( ImFluentCol_TextPrimary ), glyph );
    }
    const float text_x = pos.x + glyph_w + glyph_gap;
    const float text_y = pos.y + (row_h - text_h) * 0.5f;
    if ( header )
        dl->AddText( ImVec2( text_x, text_y ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), header );
    if ( description )
        dl->AddText( ImVec2( text_x, text_y + font_h + gap_y ),
                     ImFluent::GetColorU32( ImFluentCol_TextSecondary ), description );

    ImGui::Dummy( ImVec2( avail_w - trail_w, row_h ) );
    ImGui::SameLine();
    ImGui::SetCursorScreenPos( ImVec2( pos.x + avail_w - trail_w,
                               pos.y + (row_h - control_h) * 0.5f ) );
    ImGui::PushItemWidth( trail_w );
    return true;
}

void ImFluent::EndSettingsCard()
{
    ImGui::PopItemWidth();
    ImGui::PopID();
    EndCard();
}

// [SECTION] StackPanel & WrapPanel

void ImFluent::BeginStackPanelHorizontal( float spacing )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    if ( spacing < 0.f ) spacing = FluentDpx( style.SpacingMedium );
    ImGui::BeginGroup();
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( spacing, ImGui::GetStyle().ItemSpacing.y ) );
}

void ImFluent::BeginStackPanelVertical( float spacing )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    if ( spacing < 0.f ) spacing = FluentDpx( style.SpacingMedium );
    ImGui::BeginGroup();
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( ImGui::GetStyle().ItemSpacing.x, spacing ) );
}

void ImFluent::EndStackPanel()
{
    ImGui::PopStyleVar();
    ImGui::EndGroup();
}

void ImFluent::BeginWrapPanel( float h_spacing, float v_spacing )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    if ( h_spacing < 0.f ) h_spacing = FluentDpx( style.SpacingMedium );
    if ( v_spacing < 0.f ) v_spacing = FluentDpx( style.SpacingMedium );
    ImFluentWrapPanelState s;
    s.Avail = ImGui::GetContentRegionAvail().x;
    s.HSpacing = h_spacing;
    s.VSpacing = v_spacing;
    s.CursorX = 0.f;
    s.First = true;
    g_Ctx.WrapPanelStack.push_back( s );
    ImGui::BeginGroup();
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( h_spacing, v_spacing ) );
}

bool ImFluent::WrapPanelNextItem( float item_width )
{
    if ( g_Ctx.WrapPanelStack.empty() ) return false;
    ImFluentWrapPanelState & s = g_Ctx.WrapPanelStack.back();
    if ( s.First )
    {
        s.First = false;
        s.CursorX = item_width;
        return true;
    }
    const float needed = item_width + s.HSpacing;
    if ( s.CursorX + needed > s.Avail )
    {
        s.CursorX = item_width;
        return true;
    }
    s.CursorX += needed;
    ImGui::SameLine();
    return true;
}

void ImFluent::EndWrapPanel()
{
    if ( g_Ctx.WrapPanelStack.empty() ) { IM_ASSERT( 0 && "EndWrapPanel without matching Begin" ); return; }
    g_Ctx.WrapPanelStack.pop_back();
    ImGui::PopStyleVar();
    ImGui::EndGroup();
}

// [SECTION] Expander

static bool ExpanderHeader( const char * label, bool * open, ImFluentExpandDirection dir )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float row_h = ImFluent::FluentDpx( style.ControlHeight );
    const float W = ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + W, pos.y + row_h ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return open && *open;

    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );
    if ( pressed && open ) *open = !*open;
    const bool isOpen = open && *open;

    const float r = ImFluent::FluentDpx( style.ControlCornerRadius );
    ImU32 fill = ImFluent::ResolveControlFillState( false, held, hovered );
    fill = ImFluent::AnimateColorU32( id, fill );
    ImDrawList * dl = w->DrawList;
    dl->AddRectFilled( bb.Min, bb.Max, fill, r );
    ImFluent::DrawElevationBorder( dl, bb, r, ImFluent::GetColorU32( ImFluentCol_ControlStrokeDefault ), ImFluent::GetColorU32( ImFluentCol_ElevationControlBottom ), 1.f );

    const float closed_phase = ( dir == ImFluentExpandDirection_Up ) ? 1.f : 0.f;
    const float open_phase   = ( dir == ImFluentExpandDirection_Up ) ? 0.f : 1.f;
    const float t_anim = ImFluent::AnimateFloat( id ^ 0xEEEE, isOpen ? open_phase : closed_phase, 0.20f );
    const float cx = bb.Max.x - ImFluent::FluentDpx( style.CheckboxSize );
    const float cy = (bb.Min.y + bb.Max.y) * 0.5f;
    const float L = ImFluent::FluentDpx( style.ChevronGlyphSize + 1.f );
    const ImVec2 a( cx - L, cy + L * 0.5f - t_anim * L );
    const ImVec2 b( cx, cy - L * 0.5f + t_anim * L );
    const ImVec2 c( cx + L, cy + L * 0.5f - t_anim * L );
    dl->AddLine( a, b, ImFluent::GetColorU32( ImFluentCol_TextPrimary ), ImFluent::FluentDpx( style.StrokeMedium ) );
    dl->AddLine( b, c, ImFluent::GetColorU32( ImFluentCol_TextPrimary ), ImFluent::FluentDpx( style.StrokeMedium ) );

    dl->AddText( ImVec2( bb.Min.x + ImFluent::FluentDpx( style.SpacingXLarge ), cy - ImGui::GetFontSize() * 0.5f ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), label );
    if ( ImFluent::IsItemFocused( id ) ) ImFluent::DrawFocusRing( dl, bb, r );
    return isOpen;
}

static bool ExpanderPushBody( const char * label )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float r = ImFluent::FluentDpx( style.ControlCornerRadius );
    const float W = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor( ImGuiCol_ChildBg, ImFluent::GetColorU32( ImFluentCol_CardBgDefault ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, r );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( ImFluent::FluentDpx( style.SpacingXLarge ), ImFluent::FluentDpx( style.SpacingLarge ) ) );
    const bool open = ImGui::BeginChild( label, ImVec2( W, 0.f ), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders );
    if ( !open )
    {
        ImGui::EndChild();
        ImGui::PopStyleVar( 2 );
        ImGui::PopStyleColor();
        return false;
    }
    return true;
}

static void ExpanderPopBody()
{
    ImGui::EndChild();
    ImGui::PopStyleVar( 2 );
    ImGui::PopStyleColor();
}

bool ImFluent::BeginExpander( const char * label, bool * open, ImFluentExpandDirection direction, bool * out_just_expanded, bool * out_just_collapsed )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;

    const ImGuiID id = w->GetID( label );
    ImGuiStorage * st = ImGui::GetStateStorage();
    const ImGuiID prev_key = id ^ 0xEC0CACEDu;
    const bool prev_open = st->GetBool( prev_key, false );
    const bool cur_open  = open && *open;
    if ( out_just_expanded )  *out_just_expanded  = ( !prev_open && cur_open );
    if ( out_just_collapsed ) *out_just_collapsed = ( prev_open && !cur_open );
    st->SetBool( prev_key, cur_open );

    if ( direction == ImFluentExpandDirection_Down )
    {
        const bool isOpen = ExpanderHeader( label, open, direction );
        if ( !isOpen ) return false;
        ImFluentExpanderState s; ImStrncpy( s.Label, label, sizeof( s.Label ) );
        s.Open = open; s.Direction = direction;
        s.BodyActive = ExpanderPushBody( label );
        g_Ctx.ExpanderStack.push_back( s );
        return s.BodyActive;
    }
    else
    {
        if ( !cur_open )
        {
            ExpanderHeader( label, open, direction );
            return false;
        }
        ImFluentExpanderState s; ImStrncpy( s.Label, label, sizeof( s.Label ) );
        s.Open = open; s.Direction = direction;
        s.BodyActive = ExpanderPushBody( label );
        g_Ctx.ExpanderStack.push_back( s );
        return s.BodyActive;
    }
}

void ImFluent::EndExpander()
{
    if ( g_Ctx.ExpanderStack.empty() ) return;
    const ImFluentExpanderState s = g_Ctx.ExpanderStack.back();
    g_Ctx.ExpanderStack.pop_back();
    if ( s.BodyActive ) ExpanderPopBody();
    if ( s.Direction == ImFluentExpandDirection_Up )
        ExpanderHeader( s.Label, s.Open, s.Direction );
}

// [SECTION] ScrollView

bool ImFluent::BeginScrollView( const char * id, const ImVec2 & size )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGui::PushStyleColor( ImGuiCol_ChildBg, ImFluent::GetColorU32( ImFluentCol_SubtleFillTransparent ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, FluentDpx( style.ControlCornerRadius ) );
    return ImGui::BeginChild( id, size, ImGuiChildFlags_None,
                              ImGuiWindowFlags_HorizontalScrollbar );
}

void ImFluent::EndScrollView()
{
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// [SECTION] TabView

bool ImFluent::BeginTabView( const char * id )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGui::PushStyleColor( ImGuiCol_Tab, ImFluent::GetColorU32( ImFluentCol_SubtleFillTransparent ) );
    ImGui::PushStyleColor( ImGuiCol_TabHovered, ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary ) );
    ImGui::PushStyleColor( ImGuiCol_TabSelected, ImFluent::GetColorU32( ImFluentCol_LayerFillAlt ) );
    ImGui::PushStyleColor( ImGuiCol_TabDimmed, ImFluent::GetColorU32( ImFluentCol_SubtleFillTransparent ) );
    ImGui::PushStyleColor( ImGuiCol_TabDimmedSelected, ImFluent::GetColorU32( ImFluentCol_LayerFillAlt ) );
    ImGui::PushStyleVar( ImGuiStyleVar_TabRounding, FluentDpx( style.ControlCornerRadius ) );
    if ( ImGui::BeginTabBar( id, ImGuiTabBarFlags_None ) )
        return true;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor( 5 );
    return false;
}

bool ImFluent::BeginTabItem( const char * label, bool * p_open, ImGuiTabItemFlags flags )
{
    const bool open = ImGui::BeginTabItem( label, p_open, flags );
    if ( p_open && ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenBlockedByPopup ) &&
         ImGui::IsMouseClicked( ImGuiMouseButton_Middle ) )
        *p_open = false;
    return open;
}
void ImFluent::EndTabItem() { ImGui::EndTabItem(); }

bool ImFluent::TabAddButton()
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float h = ImGui::GetFrameHeight();
    if ( ImGui::TabItemButton( ImFluentIcon_Add,
         ImGuiTabItemFlags_Trailing |
         ImGuiTabItemFlags_NoTooltip ) )
        return true;
    ( void )h; ( void )style;
    return false;
}

void ImFluent::EndTabView()
{
    ImGui::EndTabBar();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor( 5 );
}

// [SECTION] SelectorBar

bool ImFluent::BeginSelectorBar( const char * id )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGui::PushID( id );
    ImGui::BeginGroup();
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( FluentDpx( style.SpacingXSmall ), 0.f ) );
    return true;
}

bool ImFluent::SelectorBarItem( const char * label, bool selected, const char * glyph )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    glyph = ConsumePendingGlyph( glyph );
    const ImFluentStyle & style = ImFluent::GetStyle();
    const char * txt; const char * txt_end;
    if ( glyph ) ImFormatStringToTempBuffer( &txt, &txt_end, "%s  %s", glyph, label );
    else         ImFormatStringToTempBuffer( &txt, &txt_end, "%s", label );
    const ImGuiID id = w->GetID( label );
    const float pad_x = FluentDpx( style.SpacingXLarge );
    const float pad_y = FluentDpx( style.SpacingMedium - 2.f );
    const ImVec2 ts = ImGui::CalcTextSize( txt, txt_end, true );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + ts.x + pad_x * 2.f, pos.y + ts.y + pad_y * 2.f ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) { ImGui::SameLine(); return false; }
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );
    ImDrawList * dl = w->DrawList;
    ImU32 fillTarget;
    fillTarget = ResolveSubtleFillState( selected, held, hovered );
    const ImU32 fillAnim = AnimateColorU32( id, fillTarget );
    dl->AddRectFilled( bb.Min, bb.Max, fillAnim, FluentDpx( style.ControlCornerRadius ) );
    dl->AddText( ImGui::GetFont(), ImGui::GetFontSize(),
                 ImVec2( bb.Min.x + pad_x, bb.Min.y + pad_y ),
                 ImFluent::GetColorU32( ImFluentCol_TextPrimary ), txt, txt_end );
    if ( selected )
    {
        const float bar_w = FluentDpx( style.SpacingXLarge );
        const float bx = (bb.Min.x + bb.Max.x) * 0.5f;
        dl->AddRectFilled( ImVec2( bx - bar_w * 0.5f, bb.Max.y - FluentDpx( style.SpacingXSmall ) ),
                           ImVec2( bx + bar_w * 0.5f, bb.Max.y ),
                           ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), FluentDpx( style.StrokeThin ) );
    }
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, FluentDpx( style.ControlCornerRadius ) );
    ImGui::SameLine();
    return pressed;
}

void ImFluent::EndSelectorBar()
{
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::PopID();

    ImGui::NewLine();
}

// [SECTION] NavigationView

void ImFluent::SetNextNavPaneToggleButtonVisible( bool visible )
{
    g_Ctx.NavView.NextToggleVisible    = visible;
    g_Ctx.NavView.NextToggleVisibleSet = true;
}

bool ImFluent::IsNavPaneOpening()
{
    return g_Ctx.NavView.PrevMode != ImFluentNavViewMode_LeftOpen
        && g_Ctx.NavView.Mode     == ImFluentNavViewMode_LeftOpen;
}

bool ImFluent::IsNavPaneClosing()
{
    return g_Ctx.NavView.PrevMode == ImFluentNavViewMode_LeftOpen
        && g_Ctx.NavView.Mode     != ImFluentNavViewMode_LeftOpen;
}

bool ImFluent::BeginNavigationView( const char * id, ImFluentNavViewMode * mode_io )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    g_Ctx.NavView.PrevMode = g_Ctx.NavView.Mode;
    g_Ctx.NavView.Mode = mode_io ? *mode_io : ImFluentNavViewMode_LeftCompact;

    const bool toggle_visible = g_Ctx.NavView.NextToggleVisibleSet ? g_Ctx.NavView.NextToggleVisible : true;
    g_Ctx.NavView.NextToggleVisibleSet = false;

    if ( g_Ctx.NavView.Mode == ImFluentNavViewMode_Top )
    {
        const float h = FluentDpx( style.NavItemHeight );
        g_Ctx.NavView.CurrentWidth = ImGui::GetContentRegionAvail().x;

        ImGui::PushID( id );
        ImGui::BeginGroup();
        ImGui::PushStyleColor( ImGuiCol_ChildBg, ImFluent::GetColorU32( ImFluentCol_LayerFillDefault ) );
        ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, 0.f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( style.SpacingSmall ), FluentDpx( style.SpacingSmall ) ) );
        const bool pane_open = ImGui::BeginChild( "##nav-pane", ImVec2( 0, h + FluentDpx( style.SpacingSmall ) * 2.f ),
                                                  ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar );
        return pane_open ? true : true;
    }

    const float target_w = (g_Ctx.NavView.Mode == ImFluentNavViewMode_LeftOpen)
        ? FluentDpx( style.NavPaneOpenWidth ) : FluentDpx( style.NavPaneCompactWidth );
    g_Ctx.NavView.CurrentWidth = AnimateFloat( ImGui::GetID( id ), target_w, 0.20f );

    ImGui::PushID( id );
    ImGui::BeginGroup();
    ImGui::PushStyleColor( ImGuiCol_ChildBg, ImFluent::GetColorU32( ImFluentCol_LayerFillDefault ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, 0.f );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( style.SpacingSmall ), FluentDpx( style.SpacingSmall ) ) );
    const bool pane_open = ImGui::BeginChild( "##nav-pane", ImVec2( g_Ctx.NavView.CurrentWidth, 0.f ),
                                              ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar );
    if ( !pane_open )
        return true;

    if ( !toggle_visible )
        return true;

    const float row_h = FluentDpx( style.NavItemHeight );
    if ( ImGui::InvisibleButton( "##nav-toggle", ImVec2( g_Ctx.NavView.CurrentWidth - FluentDpx( style.SpacingMedium ), row_h ) ) )
    {
        if ( mode_io )
            *mode_io = (g_Ctx.NavView.Mode == ImFluentNavViewMode_LeftOpen)
            ? ImFluentNavViewMode_LeftCompact : ImFluentNavViewMode_LeftOpen;
    }
    {
        const ImRect bb = ImGui::GetCurrentContext()->LastItemData.Rect;
        ImDrawList * dl = ImGui::GetWindowDrawList();
        const bool hov = ImGui::IsItemHovered();
        if ( hov ) dl->AddRectFilled( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary ), FluentDpx( style.ControlCornerRadius ) );

        const float cx = bb.Min.x + FluentDpx( style.SpacingXXLarge );
        const float cy = (bb.Min.y + bb.Max.y) * 0.5f;
        const float lw = FluentDpx( style.StandardIconSize - 2.f );
        for ( int i = -1; i <= 1; ++i )
            dl->AddLine( ImVec2( cx - lw * 0.5f, cy + ( float )i * FluentDpx( style.SpacingSmall + 1.f ) ),
                         ImVec2( cx + lw * 0.5f, cy + ( float )i * FluentDpx( style.SpacingSmall + 1.f ) ),
                         ImFluent::GetColorU32( ImFluentCol_TextPrimary ), FluentDpx( style.StrokeMedium ) );
    }
    return true;
}

bool ImFluent::NavItem( const char * label, bool selected, const char * glyph )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    glyph = ConsumePendingGlyph( glyph );
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );

    if ( g_Ctx.NavView.Mode == ImFluentNavViewMode_Top )
    {
        const float h = FluentDpx( style.NavItemHeight );
        const float pad_x = FluentDpx( style.SpacingLarge );
        const ImVec2 ts = ImGui::CalcTextSize( label );
        const float gw = glyph ? ImGui::CalcTextSize( glyph ).x + FluentDpx( style.SpacingMedium ) : 0.f;
        const float w_item = pad_x * 2.f + gw + ts.x;
        const ImVec2 pos = w->DC.CursorPos;
        const ImRect bb( pos, ImVec2( pos.x + w_item, pos.y + h ) );
        ImGui::ItemSize( bb );
        if ( !ImGui::ItemAdd( bb, id ) ) { ImGui::SameLine(); return false; }
        bool hov = false, held = false;
        const bool pressed = ImGui::ButtonBehavior( bb, id, &hov, &held );
        const float r = FluentDpx( style.ControlCornerRadius );
        const ImU32 fill = AnimateColorU32( id, ResolveSubtleFillState( selected, held, hov ) );
        ImDrawList * dl = w->DrawList;
        dl->AddRectFilled( bb.Min, bb.Max, fill, r );
        const float cy = ( bb.Min.y + bb.Max.y - ImGui::GetFontSize() ) * 0.5f;
        float text_x = bb.Min.x + pad_x;
        if ( glyph )
        {
            dl->AddText( ImVec2( text_x, cy ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), glyph );
            text_x += ImGui::CalcTextSize( glyph ).x + FluentDpx( style.SpacingMedium );
        }
        dl->AddText( ImVec2( text_x, cy ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), label );
        if ( selected )
        {
            const float bw = bb.GetWidth() * 0.5f;
            const float bx = ( bb.Min.x + bb.Max.x ) * 0.5f;
            dl->AddRectFilled( ImVec2( bx - bw * 0.5f, bb.Max.y - FluentDpx( style.SelectionIndicatorThickness ) ),
                               ImVec2( bx + bw * 0.5f, bb.Max.y ),
                               ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ),
                               FluentDpx( style.SpacingXSmall ) );
        }
        if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, r );
        ImGui::SameLine();
        return pressed;
    }

    const float row_h = FluentDpx( style.NavItemHeight );
    const ImVec2 pos = w->DC.CursorPos;
    const float indent_x = w->DC.Indent.x;
    const ImRect bb( ImVec2( w->Pos.x, pos.y ),
                     ImVec2( w->Pos.x + w->Size.x, pos.y + row_h ) );
    ImGui::ItemSize( ImVec2( w->Size.x, row_h ) );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );
    const float r = FluentDpx( style.ControlCornerRadius );

    ImU32 fillTarget;
    fillTarget = ResolveSubtleFillState( selected, held, hovered );
    const ImU32 fillAnim = AnimateColorU32( id, fillTarget );
    ImDrawList * dl = w->DrawList;
    dl->AddRectFilled( bb.Min, bb.Max, fillAnim, r );
    if ( selected )
    {
        const float bx = bb.Min.x + indent_x + FluentDpx( style.SpacingSmall );
        const float bh = row_h * 0.5f;
        dl->AddRectFilled( ImVec2( bx, (bb.Min.y + bb.Max.y - bh) * 0.5f ),
                           ImVec2( bx + FluentDpx( style.SelectionIndicatorThickness ), (bb.Min.y + bb.Max.y + bh) * 0.5f ),
                           ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), FluentDpx( style.SpacingXSmall ) );
    }
    const float icon_x = bb.Min.x + indent_x + FluentDpx( (style.NavPaneCompactWidth - style.StandardIconSize) * 0.5f );
    const float text_x = bb.Min.x + indent_x + FluentDpx( style.NavPaneCompactWidth );
    const float cy = (bb.Min.y + bb.Max.y - ImGui::GetFontSize()) * 0.5f;
    if ( glyph ) dl->AddText( ImVec2( icon_x, cy ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), glyph );
    if ( g_Ctx.NavView.CurrentWidth > FluentDpx( style.AppBarButtonWidth ) )
        dl->AddText( ImVec2( text_x, cy ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), label );
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, r );
    return pressed;
}

bool ImFluent::BeginNavItem( const char * label, bool selected, const char * glyph )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    glyph = ImFluent::ConsumePendingGlyph( glyph );
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );

    const float row_h = FluentDpx( style.NavItemHeight );
    const ImVec2 pos = w->DC.CursorPos;
    const float indent_x = w->DC.Indent.x;
    const ImRect bb( ImVec2( w->Pos.x, pos.y ),
                     ImVec2( w->Pos.x + w->Size.x, pos.y + row_h ) );
    ImGui::ItemSize( ImVec2( w->Size.x, row_h ) );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;

    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );

    ImGuiStorage * storage = ImGui::GetStateStorage();
    bool expanded = storage->GetBool( id, false );
    if ( pressed )
    {
        expanded = !expanded;
        storage->SetBool( id, expanded );
    }

    const float r = FluentDpx( style.ControlCornerRadius );
    ImU32 fillTarget = ImFluent::ResolveSubtleFillState( selected, held, hovered );
    const ImU32 fillAnim = ImFluent::AnimateColorU32( id, fillTarget );
    ImDrawList * dl = w->DrawList;
    dl->AddRectFilled( bb.Min, bb.Max, fillAnim, r );

    if ( selected )
    {
        const float bx = bb.Min.x + indent_x + FluentDpx( style.SpacingSmall );
        const float bh = row_h * 0.5f;
        dl->AddRectFilled( ImVec2( bx, (bb.Min.y + bb.Max.y - bh) * 0.5f ),
                           ImVec2( bx + FluentDpx( style.SelectionIndicatorThickness ), (bb.Min.y + bb.Max.y + bh) * 0.5f ),
                           ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), FluentDpx( style.SpacingXSmall ) );
    }
    const float icon_x = bb.Min.x + indent_x + FluentDpx( (style.NavPaneCompactWidth - style.StandardIconSize) * 0.5f );
    const float text_x = bb.Min.x + indent_x + FluentDpx( style.NavPaneCompactWidth );
    const float cy = (bb.Min.y + bb.Max.y - ImGui::GetFontSize()) * 0.5f;
    if ( glyph ) dl->AddText( ImVec2( icon_x, cy ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), glyph );

    const bool show_text = g_Ctx.NavView.CurrentWidth > FluentDpx( style.AppBarButtonWidth );
    if ( show_text )
        dl->AddText( ImVec2( text_x, cy ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), label );

    if ( show_text )
    {
        const float chev_phase = ImFluent::AnimateFloat( id ^ 0xCAFE, expanded ? 1.f : 0.f, 0.20f );
        const float L = FluentDpx( style.ChevronGlyphSize + 1.f );
        const float cx_start = bb.Max.x - FluentDpx( style.SpacingLarge ) - L * 2.f;
        const float ccy = (bb.Min.y + bb.Max.y) * 0.5f;
        const ImU32 chev_col = ImFluent::GetColorU32( ImFluentCol_TextSecondary );
        const ImVec2 a ( cx_start,         ccy - L * 0.5f + chev_phase * L );
        const ImVec2 mb( cx_start + L,     ccy + L * 0.5f - chev_phase * L );
        const ImVec2 c2( cx_start + L * 2.f, ccy - L * 0.5f + chev_phase * L );
        dl->AddLine( a,  mb, chev_col, FluentDpx( style.StrokeMedium ) );
        dl->AddLine( mb, c2, chev_col, FluentDpx( style.StrokeMedium ) );
    }

    if ( ImFluent::IsItemFocused( id ) ) ImFluent::DrawFocusRing( dl, bb, r );

    if ( expanded )
    {
        const float indent_amt = FluentDpx( style.SpacingXXLarge );
        ImGui::Indent( indent_amt );
        g_Ctx.NavItemIndentStack.push_back( indent_amt );
        return true;
    }
    return false;
}

void ImFluent::EndNavItem()
{
    if ( g_Ctx.NavItemIndentStack.empty() ) { IM_ASSERT( 0 && "EndNavItem without matching Begin" ); return; }
    const float indent_amt = g_Ctx.NavItemIndentStack.back();
    g_Ctx.NavItemIndentStack.pop_back();
    ImGui::Unindent( indent_amt );
}

void ImFluent::NavSubHeader( const char * text )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    if ( g_Ctx.NavView.CurrentWidth < FluentDpx( style.AppBarButtonWidth + style.SpacingXXLarge + style.SpacingMedium ) ) return;
    ImGui::Dummy( ImVec2( 0.f, FluentDpx( style.SpacingMedium ) ) );
    ImGui::PushStyleColor( ImGuiCol_Text, ImFluent::GetColorU32( ImFluentCol_TextSecondary ) );
    ImGui::SetCursorPosX( FluentDpx( style.StandardIconSize - 2.f ) );
    ImGui::TextUnformatted( text );
    ImGui::PopStyleColor();
}

void ImFluent::NavPaneTitle( const char * text )
{
    if ( !text || !*text ) return;
    const ImFluentStyle & style = ImFluent::GetStyle();
    if ( g_Ctx.NavView.CurrentWidth < FluentDpx( style.AppBarButtonWidth + style.SpacingXXLarge + style.SpacingMedium ) ) return;
    ImGui::SetCursorPosX( FluentDpx( style.StandardIconSize - 2.f ) );
    ImFluent::TextBlock( text, ImFluentTextStyle_BodyStrong );
    ImGui::Dummy( ImVec2( 0.f, FluentDpx( style.SpacingXSmall ) ) );
}

bool ImFluent::NavPaneAutoSuggestBox( const char * label, char * buf, size_t buf_size, const char * const items[], int items_count, int * selected_index, const char * hint )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    if ( g_Ctx.NavView.CurrentWidth < FluentDpx( style.AppBarButtonWidth + style.SpacingXXLarge + style.SpacingMedium ) )
        return false;
    ImGui::SetCursorPosX( FluentDpx( style.SpacingSmall ) );
    ImGui::PushItemWidth( g_Ctx.NavView.CurrentWidth - FluentDpx( style.SpacingSmall ) * 2.f );
    const bool changed = ImFluent::AutoSuggestBox( label, buf, buf_size, items, items_count, selected_index, hint );
    ImGui::PopItemWidth();
    ImGui::Dummy( ImVec2( 0.f, FluentDpx( style.SpacingXSmall ) ) );
    return changed;
}

bool ImFluent::NavBackButton( bool enabled, bool visible )
{
    if ( !visible ) return false;
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float h = FluentDpx( style.NavItemHeight );
    const float W = ImMax( h, g_Ctx.NavView.CurrentWidth - FluentDpx( style.SpacingMedium ) );
    const ImGuiID id = w->GetID( "##nav-back" );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + W, pos.y + h ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;

    bool hovered = false, held = false;
    bool pressed = false;
    if ( enabled )
        pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );

    ImDrawList * dl = w->DrawList;
    const float r = FluentDpx( style.ControlCornerRadius );
    if ( enabled && (hovered || held) )
    {
        const ImU32 fill = held ? ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary )
            : ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary );
        dl->AddRectFilled( bb.Min, bb.Max, fill, r );
    }

    const ImU32 textCol = enabled ? ImFluent::GetColorU32( ImFluentCol_TextPrimary )
        : ImFluent::GetColorU32( ImFluentCol_TextDisabled );
    const float cy = (bb.Min.y + bb.Max.y - ImGui::GetFontSize()) * 0.5f;
    dl->AddText( ImVec2( bb.Min.x + FluentDpx( style.StandardIconSize - 2.f ), cy ),
                 textCol, ImFluentIcon_BackArrow );

    if ( enabled && IsItemFocused( id ) ) DrawFocusRing( dl, bb, r );
    return pressed;
}

void ImFluent::NavPaneFooterBegin()
{
    ImGui::Dummy( ImVec2( 0.f, ImGui::GetContentRegionAvail().y - GetStyle().NavItemHeight * ImGui::GetStyle().FontScaleDpi - GetStyle().SpacingMedium * ImGui::GetStyle().FontScaleDpi ) );
}

void ImFluent::NavPaneFooterEnd()
{
}

bool ImFluent::NavSettingsItem( bool selected )
{
    SetNextItemGlyph( ImFluentIcon_Settings );
    return NavItem( "Settings", selected );
}

void ImFluent::EndNavigationView()
{
    ImGui::EndChild();
    ImGui::PopStyleVar( 2 );
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    ImGui::PopID();
    ImGui::SameLine( 0.f, 0.f );
}

void ImFluent::NavigationViewBeginContent()
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGui::PushStyleColor( ImGuiCol_ChildBg, ImFluent::GetColorU32( ImFluentCol_LayerFillAlt ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( style.SpacingXXLarge ), FluentDpx( style.SpacingXXLarge ) ) );
    ImGui::BeginChild( "##nav-content", ImVec2( 0, 0 ), ImGuiChildFlags_AlwaysUseWindowPadding );
    g_Ctx.NavView.ContentStarted = true;
}

void ImFluent::NavContentHeader( const char * title )
{
    if ( !title || !*title ) return;
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImFluent::TextBlock( title, ImFluentTextStyle_Title );
    ImGui::Dummy( ImVec2( 0.f, FluentDpx( style.SpacingMedium ) ) );
}

void ImFluent::NavigationViewEndContent()
{
    if ( !g_Ctx.NavView.ContentStarted ) return;
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    g_Ctx.NavView.ContentStarted = false;
}

// [SECTION] Lists & Pickers (ComboBox / ListBox / TreeNode / GridView / PipsPager / BreadcrumbBar)

bool ImFluent::ComboBox( const char * label, int * current_item, const char * const items[], int items_count )
{
    DrawAndConsumePendingHeader();
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float h = FluentDpx( style.ControlHeight );

    ImFluentStackGuard g;
    g.PushStyleVar( ImGuiStyleVar_FrameRounding, FluentDpx( style.ControlCornerRadius ) );
    g.PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( FluentDpx( style.SpacingLarge ), (h - ImGui::GetFontSize()) * 0.5f ) );
    g.PushStyleColor( ImGuiCol_FrameBg, ImFluent::GetColorU32( ImFluentCol_ControlFillDefault ) );
    g.PushStyleColor( ImGuiCol_FrameBgHovered, ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary ) );
    g.PushStyleColor( ImGuiCol_FrameBgActive, ImFluent::GetColorU32( ImFluentCol_ControlFillTertiary ) );
    g.PushStyleColor( ImGuiCol_Button, ImFluent::GetColorU32( ImFluentCol_ControlFillDefault ) );
    g.PushStyleColor( ImGuiCol_ButtonHovered, ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary ) );
    g.PushStyleColor( ImGuiCol_ButtonActive, ImFluent::GetColorU32( ImFluentCol_ControlFillTertiary ) );

    bool changed = false;
    const char * preview = (current_item && *current_item >= 0 && *current_item < items_count) ? items[*current_item] : "";
    if ( ImGui::BeginCombo( label, preview ) )
    {
        ImFluentStackGuard popup;
        popup.PushStyleVar( ImGuiStyleVar_SelectableTextAlign, ImVec2( 0.f, 0.5f ) );
        for ( int i = 0; i < items_count; ++i )
        {
            const bool sel = (current_item && *current_item == i);
            if ( ImGui::Selectable( items[i], sel, 0, ImVec2( 0, h ) ) )
            {
                if ( current_item ) *current_item = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    g.Restore();
    const ImRect input_rect = ImGui::GetCurrentContext()->LastItemData.Rect;
    DrawAndConsumePendingDescription();
    DrawAndConsumePendingError( input_rect );
    return changed;
}

bool ImFluent::ListBox( const char * label, int * current_item, const char * const items[], int items_count, int height_in_items )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImFluentStackGuard g;
    g.PushStyleColor( ImGuiCol_FrameBg, ImFluent::GetColorU32( ImFluentCol_LayerFillDefault ) );
    g.PushStyleVar( ImGuiStyleVar_FrameRounding, FluentDpx( style.ControlCornerRadius ) );
    bool changed = false;
    const ImVec2 sz( 0, FluentDpx( ( float )height_in_items * style.ControlHeight ) );
    if ( ImGui::BeginListBox( label, sz ) )
    {
        ImFluentStackGuard items_g;
        items_g.PushStyleVar( ImGuiStyleVar_SelectableTextAlign, ImVec2( 0.f, 0.5f ) );
        for ( int i = 0; i < items_count; ++i )
        {
            const bool sel = (current_item && *current_item == i);
            if ( ImGui::Selectable( items[i], sel, 0, ImVec2( 0, FluentDpx( style.ControlHeight ) ) ) )
            {
                if ( current_item ) *current_item = i;
                changed = true;
            }
        }
        items_g.Restore();
        ImGui::EndListBox();
    }
    return changed;
}

bool ImFluent::ListViewItem( const char * label, bool selected, const char * glyph )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    glyph = ConsumePendingGlyph( glyph );
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float h = FluentDpx( style.ControlHeight + 4.f );
    const float W = ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + W, pos.y + h ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );
    ImDrawList * dl = w->DrawList;
    const float r = FluentDpx( style.ControlCornerRadius );
    ImU32 fillTarget;
    fillTarget = ResolveSubtleFillState( selected, held, hovered );
    dl->AddRectFilled( bb.Min, bb.Max, AnimateColorU32( id, fillTarget ), r );
    if ( selected )
    {
        const float bx = bb.Min.x + FluentDpx( style.SpacingSmall );
        const float bh = h * 0.5f;
        dl->AddRectFilled( ImVec2( bx, (bb.Min.y + bb.Max.y - bh) * 0.5f ),
                           ImVec2( bx + FluentDpx( style.SelectionIndicatorThickness ), (bb.Min.y + bb.Max.y + bh) * 0.5f ),
                           ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), FluentDpx( style.SpacingXSmall ) );
    }
    const float text_x = bb.Min.x + FluentDpx( glyph ? style.NavPaneCompactWidth - 4.f : style.SpacingXLarge );
    const float cy = (bb.Min.y + bb.Max.y - ImGui::GetFontSize()) * 0.5f;
    if ( glyph ) dl->AddText( ImVec2( bb.Min.x + FluentDpx( style.StandardIconSize - 2.f ), cy ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), glyph );
    dl->AddText( ImVec2( text_x, cy ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), label );
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, r );
    return pressed;
}

bool ImFluent::TreeNode( const char * label, bool * p_open ) { return TreeNode( label, p_open, NULL ); }

bool ImFluent::TreeNode( const char * label, bool * p_open, bool * p_checked )
{ 
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float   h = FluentDpx( style.ControlHeight );
    const ImVec2  pos = w->DC.CursorPos;

    const float row_left = w->WorkRect.Min.x;
    const float row_right = w->WorkRect.Max.x;
    const ImRect bb( ImVec2( row_left, pos.y ), ImVec2( row_right, pos.y + h ) );

    const char * glyph = ConsumePendingGlyph( NULL );

    ImGui::ItemSize( ImVec2( row_right - pos.x, h ) );

    const float pad = FluentDpx( style.SpacingMedium );
    const float check_w = FluentDpx( style.CheckboxSize );
    const float check_gap = FluentDpx( style.SpacingMedium );
    const float chevron_w = FluentDpx( style.SpacingXLarge );
    const float chevron_gap = FluentDpx( style.SpacingMedium );
    const float glyph_w = FluentDpx( style.StandardIconSize + 2.f );
    const float glyph_gap = FluentDpx( style.SpacingMedium );

    float x = pos.x + pad;
    ImRect check_bb;
    if ( p_checked )
    {
        check_bb = ImRect( ImVec2( x, bb.Min.y + (h - check_w) * 0.5f ),
                           ImVec2( x + check_w, bb.Min.y + (h + check_w) * 0.5f ) );
        x += check_w + check_gap;
    }
    const float chevron_cx = x + chevron_w * 0.5f;
    x += chevron_w + chevron_gap;

    const float glyph_x = x;
    if ( glyph ) x += glyph_w + glyph_gap;
    const float label_x = x;

    if ( p_checked ) ImGui::SetNextItemAllowOverlap();
    if ( !ImGui::ItemAdd( bb, id ) ) return p_open && *p_open;

    bool hovered = false, held = false;
    const bool row_pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );

    bool check_pressed = false, check_hov = false, check_held = false;
    if ( p_checked )
    {
        const ImGuiID cid = w->GetID( ( const void * )(( intptr_t )id ^ 0xC4ECu) );
        ImGui::ItemAdd( check_bb, cid );
        check_pressed = ImGui::ButtonBehavior( check_bb, cid, &check_hov, &check_held );
        if ( check_pressed ) *p_checked = !*p_checked;
    }
    if ( row_pressed && !check_pressed && p_open ) *p_open = !*p_open;

    const bool isOpen = p_open && *p_open;
    const bool selected = p_checked && *p_checked;

    ImDrawList * dl = w->DrawList;
    const float r = FluentDpx( style.ControlCornerRadius );
    const float cy = (bb.Min.y + bb.Max.y) * 0.5f;

    const ImU32 fill = ResolveSubtleFillState( selected, held, hovered );
    dl->AddRectFilled( bb.Min, bb.Max, AnimateColorU32( id, fill ), r );

    if ( selected )
        DrawSelectionIndicator( dl, bb, ImGuiDir_Left,
                                ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), 0.5f );

    if ( p_checked )
    {
        const float box_r = FluentDpx( style.ControlCornerRadius );
        const ImGuiID cid = w->GetID( ( const void * )(( intptr_t )id ^ 0xC4ECu) );
        ImU32 box_fill, box_stroke;
        if ( *p_checked )
        {
            box_fill = check_held ? ImFluent::GetColorU32( ImFluentCol_AccentFillTertiary )
                : check_hov ? ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary )
                : ImFluent::GetColorU32( ImFluentCol_AccentFillDefault );
            box_stroke = box_fill;
        }
        else
        {
            box_fill = ResolveControlAltFillState( false, check_held, check_hov );
            box_stroke = ImFluent::GetColorU32( ImFluentCol_ControlStrongStrokeDefault );
        }
        const ImU32 box_fill_anim   = AnimateColorU32( cid, box_fill );
        const ImU32 box_stroke_anim = AnimateColorU32( cid ^ 0xC1A0, box_stroke );
        dl->AddRectFilled( check_bb.Min, check_bb.Max, box_fill_anim, box_r );
        dl->AddRect( check_bb.Min, check_bb.Max, box_stroke_anim, box_r, 0, 1.f );
        if ( *p_checked )
        {
            const ImU32  mark = ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary );
            const ImVec2 a( check_bb.Min.x + check_w * 0.22f, check_bb.Min.y + check_w * 0.52f );
            const ImVec2 mid( check_bb.Min.x + check_w * 0.42f, check_bb.Min.y + check_w * 0.72f );
            const ImVec2 c( check_bb.Min.x + check_w * 0.78f, check_bb.Min.y + check_w * 0.32f );
            dl->AddLine( a, mid, mark, FluentDpx( style.StrokeMedium ) );
            dl->AddLine( mid, c, mark, FluentDpx( style.StrokeMedium ) );
        }
    }

    DrawChevron( dl, ImVec2( chevron_cx, cy ),
                 isOpen ? ImGuiDir_Down : ImGuiDir_Right,
                 ImFluent::GetColorU32( ImFluentCol_TextPrimary ), FluentDpx( style.ChevronGlyphSize ) );

    if ( glyph )
        dl->AddText( ImVec2( glyph_x, cy - ImGui::GetFontSize() * 0.5f ),
                     ImFluent::GetColorU32( ImFluentCol_TextPrimary ), glyph );

    dl->AddText( ImVec2( label_x, cy - ImGui::GetFontSize() * 0.5f ),
                 ImFluent::GetColorU32( ImFluentCol_TextPrimary ), label );

    if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, r );
    if ( isOpen ) ImGui::Indent( FluentDpx( style.CheckboxSize ) );
    return isOpen;
}

void ImFluent::TreePop() { ImGui::Unindent( FluentDpx( GetStyle().CheckboxSize ) ); }

bool ImFluent::GridViewItem( const char * label, bool selected, const ImVec2 & size )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const ImVec2 sz = (size.x > 0 && size.y > 0) ? size : ImVec2( FluentDpx( style.ControlMinWidth + style.SpacingXXLarge + style.SpacingXLarge ), FluentDpx( style.ControlMinWidth ) );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + sz.x, pos.y + sz.y ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, id, &hovered, &held );
    ImDrawList * dl = w->DrawList;
    const float r = FluentDpx( style.OverlayCornerRadius );
    ImU32 fill = selected ? ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary )
        : held ? ImFluent::GetColorU32( ImFluentCol_ControlFillTertiary )
        : hovered ? ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary )
        : ImFluent::GetColorU32( ImFluentCol_CardBgDefault );
    dl->AddRectFilled( bb.Min, bb.Max, AnimateColorU32( id, fill ), r );
    dl->AddRect( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_CardStrokeDefault ), r, 0, 1.f );
    const ImVec2 ts = ImGui::CalcTextSize( label );
    dl->AddText( ImVec2( bb.Min.x + (sz.x - ts.x) * 0.5f, bb.Max.y - ts.y - FluentDpx( style.SpacingLarge ) ),
                 selected ? ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary ) : ImFluent::GetColorU32( ImFluentCol_TextPrimary ), label );
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, r );
    return pressed;
}

bool ImFluent::PipsPager( const char * id, int * current_item, int total_pages )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle(); ( void )style;
    const float dot = FluentDpx( style.SpacingMedium );
    const float gap = FluentDpx( style.SpacingMedium );
    const float total_w = total_pages * dot + (total_pages - 1) * gap;
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + total_w, pos.y + dot ) );
    ImGui::ItemSize( bb );
    const ImGuiID gid = w->GetID( id );
    if ( !ImGui::ItemAdd( bb, gid ) ) return false;
    bool changed = false;
    ImDrawList * dl = w->DrawList;
    for ( int i = 0; i < total_pages; ++i )
    {
        const ImVec2 c( pos.x + i * (dot + gap) + dot * 0.5f, pos.y + dot * 0.5f );
        const bool sel = current_item && (*current_item == i);
        ImGui::PushID( i );
        const ImGuiID iid = w->GetID( i );
        const ImRect rect( ImVec2( c.x - dot * 0.5f, c.y - dot * 0.5f ),
                           ImVec2( c.x + dot * 0.5f, c.y + dot * 0.5f ) );
        bool hov = false, held = false;
        const bool pr = ImGui::ButtonBehavior( rect, iid, &hov, &held );
        if ( pr && current_item ) { *current_item = i; changed = true; }
        const ImU32 col_target = sel ? ImFluent::GetColorU32( ImFluentCol_AccentFillDefault )
            : hov ? ImFluent::GetColorU32( ImFluentCol_TextSecondary )
            : ImFluent::GetColorU32( ImFluentCol_TextDisabled );
        dl->AddCircleFilled( c, dot * 0.5f, AnimateColorU32( iid, col_target ), 16 );
        ImGui::PopID();
    }
    return changed;
}

int ImFluent::BreadcrumbBar( const char * id, const char * const items[], int items_count )
{
    int clicked = -1;
    ImFluentStackGuard g;
    g.PushID( id );
    g.BeginGroup();
    for ( int i = 0; i < items_count; ++i )
    {
        const bool last = (i == items_count - 1);
        ImFluentStackGuard item_g;
        item_g.PushID( i );
        if ( last )
        {
            item_g.PushStyleColor( ImGuiCol_Text, ImFluent::GetColorU32( ImFluentCol_TextPrimary ) );
            ImFluent::TextBlock( items[i], ImFluentTextStyle_BodyStrong );
        }
        else
        {
            if ( ImFluent::HyperlinkButton( items[i] ) ) clicked = i;
            ImGui::SameLine();
            item_g.PushStyleColor( ImGuiCol_Text, ImFluent::GetColorU32( ImFluentCol_TextSecondary ) );
            ImGui::TextUnformatted( " / " );
            item_g.Restore();
            ImGui::SameLine();
        }
    }
    return clicked;
}

// [SECTION] Flyout

void ImFluent::OpenFlyout( const char * id )
{
    ImGui::OpenPopup( id );
    ImGuiContext & g = *ImGui::GetCurrentContext();
    StorePopupAnchor( ImGui::GetID( id ), g.LastItemData.Rect );
}

bool ImFluent::BeginFlyout( const char * id )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImFluentStackGuard g;
    g.PushStyleColor( ImGuiCol_PopupBg, ImFluent::GetColorU32( ImFluentCol_SolidBgQuarternary ) );
    g.PushStyleColor( ImGuiCol_Border, ImFluent::GetColorU32( ImFluentCol_SurfaceStrokeFlyout ) );
    g.PushStyleVar( ImGuiStyleVar_PopupRounding, FluentDpx( style.OverlayCornerRadius ) );
    g.PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( style.SpacingXLarge ), FluentDpx( style.SpacingXLarge ) ) );
    ApplyPopupAnchor( id );
    if ( !ImGui::BeginPopup( id ) )
        return false;
    g.Forget();
    return true;
}

void ImFluent::EndFlyout()
{
    if ( ImGui::IsPopupOpen( "##__noop", 0 ) ) {}

    if ( ImGuiWindow * w = ImGui::GetCurrentWindow() )
    {
        const ImFluentStyle & style = ImFluent::GetStyle();
        const ImRect bb( w->Pos, ImVec2( w->Pos.x + w->Size.x, w->Pos.y + w->Size.y ) );

        ImDrawList * fg = ImGui::GetForegroundDrawList( w );
        DrawElevationShadow( fg, bb, FluentDpx( style.OverlayCornerRadius ), 4 );
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar( 2 );
    ImGui::PopStyleColor( 2 );
}

// [SECTION] MenuFlyout

void ImFluent::OpenMenuFlyout( const char * id )
{
    ImGui::OpenPopup( id );
    ImGuiContext & g = *ImGui::GetCurrentContext();
    StorePopupAnchor( ImGui::GetID( id ), g.LastItemData.Rect );
}

bool ImFluent::BeginMenuFlyout( const char * id )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImFluentStackGuard g;
    g.PushStyleColor( ImGuiCol_PopupBg, ImFluent::GetColorU32( ImFluentCol_SolidBgQuarternary ) );
    g.PushStyleColor( ImGuiCol_Border, ImFluent::GetColorU32( ImFluentCol_SurfaceStrokeFlyout ) );
    g.PushStyleColor( ImGuiCol_HeaderHovered, ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary ) );
    g.PushStyleColor( ImGuiCol_HeaderActive, ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary ) );
    g.PushStyleVar( ImGuiStyleVar_PopupRounding, FluentDpx( style.OverlayCornerRadius ) );
    g.PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( style.SpacingSmall ), FluentDpx( style.SpacingSmall ) ) );
    g.PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.f, 0.f ) );
    ApplyPopupAnchor( id );
    if ( !ImGui::BeginPopup( id ) )
        return false;
    g.Forget();
    return true;
}

bool ImFluent::MenuFlyoutItem( const char * label, const char * shortcut, const char * glyph, bool selected, bool enabled )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    glyph = ConsumePendingGlyph( glyph );
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float h = FluentDpx( style.MenuItemHeight );
    const float min_w = FluentDpx( style.NavPaneCompactWidth * 3.75f );
    const ImVec2 ts = ImGui::CalcTextSize( label );
    const ImVec2 ss = shortcut ? ImGui::CalcTextSize( shortcut ) : ImVec2( 0, 0 );
    const ImVec2 pos = w->DC.CursorPos;
    const float W = ImMax( min_w, ts.x + ss.x + FluentDpx( style.AppBarButtonWidth ) );
    const ImRect bb( pos, ImVec2( pos.x + W, pos.y + h ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;
    bool hov = false, held = false;
    const bool pressed = enabled ? ImGui::ButtonBehavior( bb, id, &hov, &held ) : false;
    ImDrawList * dl = w->DrawList;
    const ImU32 mf_target = (hov || selected)
        ? (held ? ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary )
                : ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary ))
        : ImFluent::GetColorU32( ImFluentCol_SubtleFillTransparent );
    dl->AddRectFilled( bb.Min, bb.Max, AnimateColorU32( id, mf_target ), FluentDpx( style.ControlCornerRadius ) );
    const float cy = (bb.Min.y + bb.Max.y - ts.y) * 0.5f;
    if ( glyph )
        dl->AddText( ImVec2( bb.Min.x + FluentDpx( style.SpacingLarge ), cy ), enabled ? ImFluent::GetColorU32( ImFluentCol_TextPrimary ) : ImFluent::GetColorU32( ImFluentCol_TextDisabled ), glyph );
    dl->AddText( ImVec2( bb.Min.x + FluentDpx( style.NavItemHeight ), cy ), enabled ? ImFluent::GetColorU32( ImFluentCol_TextPrimary ) : ImFluent::GetColorU32( ImFluentCol_TextDisabled ), label );
    if ( shortcut )
        dl->AddText( ImVec2( bb.Max.x - FluentDpx( style.SpacingLarge ) - ss.x, cy ), ImFluent::GetColorU32( ImFluentCol_TextSecondary ), shortcut );
    return pressed;
}

bool ImFluent::ToggleMenuFlyoutItem( const char * label, bool * v, const char * shortcut, bool enabled )
{
    const bool selected = v && *v;
    const char * mark = selected ? ImFluentIcon_Check : NULL;
    if ( MenuFlyoutItem( label, shortcut, mark, false, enabled ) )
    {
        if ( v ) *v = !*v;
        return true;
    }
    return false;
}

bool ImFluent::RadioMenuFlyoutItem( const char * label, int * v, int v_button, const char * shortcut, bool enabled )
{
    const bool selected = v && (*v == v_button);
    const char * mark = selected ? ImFluentIcon_Check : NULL;
    if ( MenuFlyoutItem( label, shortcut, mark, false, enabled ) )
    {
        if ( v ) *v = v_button;
        return true;
    }
    return false;
}

bool ImFluent::BeginMenuFlyoutSubItem( const char * label, const char * glyph, bool enabled )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    glyph = ConsumePendingGlyph( glyph );
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );
    const float h = FluentDpx( style.MenuItemHeight );
    const float min_w = FluentDpx( style.NavPaneCompactWidth * 3.75f );
    const ImVec2 ts = ImGui::CalcTextSize( label );
    const ImVec2 pos = w->DC.CursorPos;
    const float W = ImMax( min_w, ts.x + FluentDpx( style.AppBarButtonWidth ) );
    const ImRect bb( pos, ImVec2( pos.x + W, pos.y + h ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;

    bool hov = false, held = false;
    const bool pressed = enabled ? ImGui::ButtonBehavior( bb, id, &hov, &held,
                                                          ImGuiButtonFlags_PressedOnClick |
                                                          ImGuiButtonFlags_NoNavFocus ) : false;
    const bool open = ImGui::IsPopupOpen( label, ImGuiPopupFlags_None );
    if ( pressed || (hov && !open) )
    {
        ImGui::OpenPopup( label );
    }

    ImDrawList * dl = w->DrawList;
    const ImU32 mfs_target = (hov || open)
        ? (held ? ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary )
                : ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary ))
        : ImFluent::GetColorU32( ImFluentCol_SubtleFillTransparent );
    dl->AddRectFilled( bb.Min, bb.Max, AnimateColorU32( id, mfs_target ), FluentDpx( style.ControlCornerRadius ) );
    const float cy = (bb.Min.y + bb.Max.y - ts.y) * 0.5f;
    if ( glyph )
        dl->AddText( ImVec2( bb.Min.x + FluentDpx( style.SpacingLarge ), cy ),
                     enabled ? ImFluent::GetColorU32( ImFluentCol_TextPrimary ) : ImFluent::GetColorU32( ImFluentCol_TextDisabled ), glyph );
    dl->AddText( ImVec2( bb.Min.x + FluentDpx( style.NavItemHeight ), cy ),
                 enabled ? ImFluent::GetColorU32( ImFluentCol_TextPrimary ) : ImFluent::GetColorU32( ImFluentCol_TextDisabled ), label );
    DrawChevron( dl,
                 ImVec2( bb.Max.x - FluentDpx( style.SpacingLarge ),
                 (bb.Min.y + bb.Max.y) * 0.5f ),
                 ImGuiDir_Right,
                 enabled ? ImFluent::GetColorU32( ImFluentCol_TextSecondary ) : ImFluent::GetColorU32( ImFluentCol_TextDisabled ),
                 FluentDpx( style.ChevronGlyphSize ) );

    ImFluentStackGuard g;
    g.PushStyleColor( ImGuiCol_PopupBg, ImFluent::GetColorU32( ImFluentCol_SolidBgQuarternary ) );
    g.PushStyleColor( ImGuiCol_Border, ImFluent::GetColorU32( ImFluentCol_SurfaceStrokeFlyout ) );
    g.PushStyleColor( ImGuiCol_HeaderHovered, ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary ) );
    g.PushStyleColor( ImGuiCol_HeaderActive, ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary ) );
    g.PushStyleVar( ImGuiStyleVar_PopupRounding, FluentDpx( style.OverlayCornerRadius ) );
    g.PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( style.SpacingSmall ), FluentDpx( style.SpacingSmall ) ) );
    g.PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.f, 0.f ) );
    ImGui::SetNextWindowPos( ImVec2( bb.Max.x, bb.Min.y ), ImGuiCond_Appearing );
    if ( ImGui::BeginPopup( label ) )
    {
        g.Forget();
        return true;
    }
    return false;
}

void ImFluent::EndMenuFlyoutSubItem()
{
    ImGui::EndPopup();
    ImGui::PopStyleVar( 3 );
    ImGui::PopStyleColor( 4 );
}

void ImFluent::MenuFlyoutSeparator()
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return;
    const ImVec2 pos = w->DC.CursorPos;
    const float W = ImGui::GetContentRegionAvail().x;
    const float pad = FluentDpx( style.SpacingSmall );
    const ImRect bb( ImVec2( pos.x + pad, pos.y + 2.f ), ImVec2( pos.x + W - pad, pos.y + 3.f ) );
    ImGui::ItemSize( ImVec2( W, FluentDpx( style.SpacingMedium - 2.f ) ) );
    if ( !ImGui::ItemAdd( bb, 0 ) ) return;
    w->DrawList->AddRectFilled( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_DividerStrokeDefault ) );
}

void ImFluent::EndMenuFlyout()
{
    ImGui::EndPopup();
    ImGui::PopStyleVar( 3 );
    ImGui::PopStyleColor( 4 );
}

// [SECTION] CommandBarFlyout

bool ImFluent::BeginCommandBarFlyout( const char * id ) { return BeginFlyout( id ); }
void ImFluent::EndCommandBarFlyout() { EndFlyout(); }

// [SECTION] ContentDialog

void ImFluent::OpenContentDialog( const char * id ) { ImGui::OpenPopup( id ); }

bool ImFluent::BeginContentDialog( const char * id, const char * title )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImFluentStackGuard g;
    g.PushStyleColor( ImGuiCol_PopupBg, ImFluent::GetColorU32( ImFluentCol_SolidBgBase ) );
    g.PushStyleColor( ImGuiCol_Border, ImFluent::GetColorU32( ImFluentCol_SurfaceStrokeDefault ) );
    g.PushStyleColor( ImGuiCol_ModalWindowDimBg, ImFluent::GetColorU32( ImFluentCol_SmokeFill ) );
    g.PushStyleVar( ImGuiStyleVar_PopupRounding, FluentDpx( style.OverlayCornerRadius ) );
    g.PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( FluentDpx( style.SpacingXXLarge ), FluentDpx( style.SpacingXXLarge ) ) );
    const ImVec2 vp_size = ImGui::GetMainViewport()->WorkSize;
    const ImVec2 vp_pos = ImGui::GetMainViewport()->WorkPos;
    ImGui::SetNextWindowPos( ImVec2( vp_pos.x + vp_size.x * 0.5f, vp_pos.y + vp_size.y * 0.5f ),
                             ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
    ImGui::SetNextWindowSize( ImVec2( FluentDpx( style.ControlMinWidth * 4.f ), 0.f ), ImGuiCond_Always );
    if ( !ImGui::BeginPopupModal( id, NULL,
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar ) )
        return false;
    g.Forget();
    ImFluent::TextBlock( title, ImFluentTextStyle_Subtitle );
    ImGui::Dummy( ImVec2( 0, FluentDpx( style.SpacingMedium ) ) );
    return true;
}

int ImFluent::EndContentDialog( const char * primary, const char * secondary, const char * close_text, ImFluentContentDialogButton default_button )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    int result = 0;
    ImGui::Dummy( ImVec2( 0, FluentDpx( style.SpacingXLarge ) ) );
    const float gap = FluentDpx( style.SpacingMedium );
    const float w = ImGui::GetContentRegionAvail().x;
    int btn_count = (primary ? 1 : 0) + (secondary ? 1 : 0) + (close_text ? 1 : 0);
    if ( btn_count == 0 ) btn_count = 1;
    const float btn_w = (w - gap * (btn_count - 1)) / ( float )btn_count;
    const ImVec2 btn_sz( btn_w, FluentDpx( style.ControlHeight ) );

    auto run = [ & ]( const char * label, ImFluentContentDialogButton kind, int code )
    {
        const bool use_accent = (default_button == kind);
        const bool clicked = use_accent ? ImFluent::AccentButton( label, btn_sz )
            : ImFluent::Button( label, btn_sz );
        if ( clicked ) { result = code; ImGui::CloseCurrentPopup(); }
    };

    int placed = 0;
    if ( primary )
    {
        run( primary, ImFluentContentDialogButton_Primary, 1 );
        if ( ++placed < btn_count ) ImGui::SameLine( 0.f, gap );
    }
    if ( secondary )
    {
        run( secondary, ImFluentContentDialogButton_Secondary, 2 );
        if ( ++placed < btn_count ) ImGui::SameLine( 0.f, gap );
    }
    if ( close_text )
    {
        run( close_text, ImFluentContentDialogButton_Close, 3 );
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar( 2 );
    ImGui::PopStyleColor( 3 );
    return result;
}

// [SECTION] InfoBar / InfoBadge / PersonPicture

bool ImFluent::InfoBar( ImFluentInfoSeverity sev, const char * title, const char * msg, bool * is_open, const char * glyph_override, bool show_icon, const char * action_label )
{
    if ( is_open && !*is_open ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();

    ImU32 sevCol; const char * glyph;
    switch ( sev )
    {
        case ImFluentInfoSeverity_Success:  sevCol = ImFluent::GetColorU32( ImFluentCol_SystemFillSuccess );     glyph = "\xEE\xA1\x9F"; break;
        case ImFluentInfoSeverity_Warning:  sevCol = ImFluent::GetColorU32( ImFluentCol_SystemFillCaution );     glyph = "\xEE\x9D\xA3"; break;
        case ImFluentInfoSeverity_Critical: sevCol = ImFluent::GetColorU32( ImFluentCol_SystemFillCritical );    glyph = "\xEE\xA3\x83"; break;
        case ImFluentInfoSeverity_Informational:
        default:                            sevCol = ImFluent::GetColorU32( ImFluentCol_SystemFillNeutral );     glyph = "\xEE\xA5\x86"; break;
    }
    if ( glyph_override ) glyph = glyph_override;

    const ImU32 bgTint = (sev == ImFluentInfoSeverity_Informational)
        ? 0u
        : ((sevCol & ~IM_COL32_A_MASK) | (36u << IM_COL32_A_SHIFT));

    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;

    const bool   has_action  = (action_label && *action_label);
    const float  font_h      = ImGui::GetFontSize();
    const float  W           = ImGui::GetContentRegionAvail().x;
    const float  pad_x       = FluentDpx( style.SpacingXLarge );
    const float  pad_y       = FluentDpx( style.SpacingLarge );
    const float  icon_col    = show_icon ? FluentDpx( style.SpinButtonWidth ) : 0.f;
    const float  close_w     = is_open ? FluentDpx( style.ControlHeight ) : 0.f;
    const float  action_pad  = has_action ? FluentDpx( style.SpacingMedium ) : 0.f;
    const ImVec2 action_text_sz = has_action ? ImGui::CalcTextSize( action_label ) : ImVec2( 0, 0 );
    const float  action_w    = has_action ? ( action_text_sz.x + FluentDpx( style.SpacingLarge ) * 2.f ) : 0.f;
    const float  text_x      = pad_x + icon_col;
    const float  text_avail  = ImMax( 1.f, W - text_x - close_w - action_w - action_pad - pad_x );

    const ImVec2 title_sz = title && *title ? ImGui::CalcTextSize( title, NULL, false, text_avail ) : ImVec2( 0, 0 );
    const ImVec2 msg_sz = msg && *msg ? ImGui::CalcTextSize( msg, NULL, false, text_avail ) : ImVec2( 0, 0 );
    const float gap = (title_sz.y > 0 && msg_sz.y > 0) ? FluentDpx( style.SpacingXSmall ) : 0.f;
    const float text_h = title_sz.y + gap + msg_sz.y;
    const float min_h = FluentDpx( style.NavPaneCompactWidth );
    const float h = ImMax( min_h, text_h + pad_y * 2.f );

    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + W, pos.y + h ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, 0 ) ) return false;

    ImDrawList * dl = w->DrawList;
    const float r = FluentDpx( style.OverlayCornerRadius );

    dl->AddRectFilled( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_CardBgDefault ), r );
    if ( bgTint ) dl->AddRectFilled( bb.Min, bb.Max, bgTint, r );
    dl->AddRect( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_CardStrokeDefault ), r, 0, 1.f );

    dl->AddRectFilled( bb.Min, ImVec2( bb.Min.x + FluentDpx( style.SeverityBarThickness ), bb.Max.y ), sevCol, r );

    if ( show_icon )
        dl->AddText( ImVec2( bb.Min.x + FluentDpx( style.SpacingXLarge ), bb.Min.y + pad_y ), sevCol, glyph );

    const float text_left = bb.Min.x + text_x;
    float       text_y = bb.Min.y + pad_y;

    if ( title_sz.y > 0 )
    {
        dl->AddText( ImGui::GetFont(), font_h,
                     ImVec2( text_left, text_y ),
                     ImFluent::GetColorU32( ImFluentCol_TextPrimary ),
                     title, NULL, text_avail );
        text_y += title_sz.y + gap;
    }
    if ( msg_sz.y > 0 )
    {
        dl->AddText( ImGui::GetFont(), font_h,
                     ImVec2( text_left, text_y ),
                     ImFluent::GetColorU32( ImFluentCol_TextSecondary ),
                     msg, NULL, text_avail );
    }

    bool action_clicked = false;
    if ( has_action )
    {
        const float btn_h = FluentDpx( style.ControlHeight );
        const float by    = bb.Min.y + ( h - btn_h ) * 0.5f;
        const float bx_max = bb.Max.x - close_w - ( close_w > 0.f ? FluentDpx( style.SpacingXSmall ) : pad_x );
        const ImRect btn_bb( ImVec2( bx_max - action_w, by ),
                             ImVec2( bx_max, by + btn_h ) );
        const ImGuiID aid = w->GetID( ( const void * )action_label );
        bool ahov = false, aheld = false;
        if ( ImGui::ItemAdd( btn_bb, aid ) )
        {
            action_clicked = ImGui::ButtonBehavior( btn_bb, aid, &ahov, &aheld );
        }
        ImU32 fill_target;
        if ( aheld )      fill_target = ImFluent::GetColorU32( ImFluentCol_ControlFillTertiary );
        else if ( ahov )  fill_target = ImFluent::GetColorU32( ImFluentCol_ControlFillSecondary );
        else              fill_target = ImFluent::GetColorU32( ImFluentCol_ControlFillDefault );
        const float r_btn = FluentDpx( style.ControlCornerRadius );
        dl->AddRectFilled( btn_bb.Min, btn_bb.Max, ImFluent::AnimateColorU32( aid, fill_target ), r_btn );
        DrawElevationBorder( dl, btn_bb, r_btn,
                             ImFluent::GetColorU32( ImFluentCol_ControlStrokeDefault ),
                             ImFluent::GetColorU32( ImFluentCol_ElevationControlBottom ), 1.f );
        const float ax = btn_bb.Min.x + ( btn_bb.GetWidth() - action_text_sz.x ) * 0.5f;
        const float ay = btn_bb.Min.y + ( btn_bb.GetHeight() - action_text_sz.y ) * 0.5f;
        dl->AddText( ImVec2( ax, ay ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), action_label );
    }

    if ( is_open )
    {
        const float close_sz = FluentDpx( style.SpacingXXLarge );
        const float cx_min = bb.Max.x - close_sz - FluentDpx( style.SpacingMedium );
        const float cy_min = bb.Min.y + FluentDpx( style.SpacingMedium );
        const ImRect close_bb( ImVec2( cx_min, cy_min ),
                               ImVec2( cx_min + close_sz, cy_min + close_sz ) );
        const ImGuiID cid = w->GetID( ( const void * )is_open );
        bool hov = false, held = false;
        if ( ImGui::ItemAdd( close_bb, cid ) )
        {
            const bool pr = ImGui::ButtonBehavior( close_bb, cid, &hov, &held );
            if ( pr ) *is_open = false;
        }
        const ImU32 close_fill = ImFluent::AnimateColorU32( cid,
            held ? ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary )
                 : (hov ? ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary )
                        : ImFluent::GetColorU32( ImFluentCol_SubtleFillTransparent )) );
        dl->AddRectFilled( close_bb.Min, close_bb.Max, close_fill, FluentDpx( style.ControlCornerRadius ) );
        const ImVec2 cc( (close_bb.Min.x + close_bb.Max.x) * 0.5f, (close_bb.Min.y + close_bb.Max.y) * 0.5f );
        const float L = FluentDpx( style.ChevronGlyphSize + 1.f );
        dl->AddLine( ImVec2( cc.x - L, cc.y - L ), ImVec2( cc.x + L, cc.y + L ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), FluentDpx( style.StrokeMedium ) );
        dl->AddLine( ImVec2( cc.x - L, cc.y + L ), ImVec2( cc.x + L, cc.y - L ), ImFluent::GetColorU32( ImFluentCol_TextPrimary ), FluentDpx( style.StrokeMedium ) );
    }
    return action_clicked;
}

void ImFluent::InfoBadge( int count, const char * glyph )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const bool dot = (count < 0 && !glyph);
    if ( dot )
    {
        const float D = FluentDpx( style.SpacingMedium );
        const ImVec2 pos = w->DC.CursorPos;
        const ImRect bb( pos, ImVec2( pos.x + D, pos.y + D ) );
        ImGui::ItemSize( bb );
        if ( !ImGui::ItemAdd( bb, 0 ) ) return;
        w->DrawList->AddCircleFilled( ImVec2( pos.x + D * 0.5f, pos.y + D * 0.5f ), D * 0.5f, ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), 16 );
        return;
    }
    const char * text = glyph;
    const char * text_end = NULL;
    if ( !glyph && count >= 0 )
    {
        if ( count > 99 ) ImFormatStringToTempBuffer( &text, &text_end, "99+" );
        else              ImFormatStringToTempBuffer( &text, &text_end, "%d", count );
    }
    if ( !text ) text = "";
    const ImVec2 ts = ImGui::CalcTextSize( text, text_end );
    const float pad_x = FluentDpx( style.SpacingMedium - 2.f );
    const float h = FluentDpx( style.BadgeHeight );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + ImMax( ts.x + pad_x * 2.f, h ), pos.y + h ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, 0 ) ) return;
    w->DrawList->AddRectFilled( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ), h * 0.5f );
    w->DrawList->AddText( ImGui::GetFont(), ImGui::GetFontSize(),
                          ImVec2( bb.Min.x + (bb.GetWidth() - ts.x) * 0.5f,
                          bb.Min.y + (h - ts.y) * 0.5f ),
                          ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary ), text, text_end );
}

static const ImU32 g_AvatarTints[] = {
    IM_COL32( 99, 102, 241, 255 ),
    IM_COL32( 16, 185, 129, 255 ),
    IM_COL32( 244, 114, 182, 255 ),
    IM_COL32( 245, 158, 11, 255 ),
    IM_COL32( 6, 182, 212, 255 ),
    IM_COL32( 168, 85, 247, 255 ),
    IM_COL32( 239, 68, 68, 255 ),
    IM_COL32( 14, 165, 233, 255 ),
};

static int InitialsFromName( const char * name, char out[5] )
{
    int n = 0;
    const char * p = name;
    bool at_word = true;
    while ( *p && n < 4 )
    {
        if ( ( unsigned char )*p > ' ' )
        {
            if ( at_word )
            {
                char c = *p;
                if ( c >= 'a' && c <= 'z' ) c = ( char )(c - 'a' + 'A');
                out[n++] = c;
                at_word = false;
                if ( n >= 2 ) break;
            }
        }
        else
        {
            at_word = true;
        }
        ++p;
    }
    out[n] = 0;
    return n;
}

static ImU32 TintFromName( const char * name )
{
    unsigned int h = 5381u;
    for ( const char * p = name; p && *p; ++p ) h = (h * 33u) ^ ( unsigned char )*p;
    return g_AvatarTints[h % ( unsigned int )(sizeof( g_AvatarTints ) / sizeof( g_AvatarTints[0] ))];
}

void ImFluent::PersonPicture( const char * display_name, float diameter_dpx, const char * glyph_override )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return;
    const float D = FluentDpx( diameter_dpx );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( pos, ImVec2( pos.x + D, pos.y + D ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, 0 ) ) return;
    const ImVec2 c( pos.x + D * 0.5f, pos.y + D * 0.5f );
    const ImU32 tint = (display_name && *display_name) ? TintFromName( display_name )
        : ImFluent::GetColorU32( ImFluentCol_AccentFillDefault );
    ImDrawList * dl = w->DrawList;
    dl->AddCircleFilled( c, D * 0.5f, tint, 32 );
    if ( glyph_override )
    {
        const ImVec2 gs = ImGui::CalcTextSize( glyph_override );
        dl->AddText( ImVec2( c.x - gs.x * 0.5f, c.y - gs.y * 0.5f ),
                     ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary ), glyph_override );
    }
    else if ( display_name && *display_name )
    {
        char initials[5];
        InitialsFromName( display_name, initials );
        const ImVec2 ts = ImGui::CalcTextSize( initials );
        dl->AddText( ImVec2( c.x - ts.x * 0.5f, c.y - ts.y * 0.5f ),
                     ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary ), initials );
    }
    else
    {
        dl->AddText( ImVec2( c.x - ImGui::CalcTextSize( ImFluentIcon_Contact ).x * 0.5f,
                     c.y - ImGui::GetFontSize() * 0.5f ),
                     ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary ), ImFluentIcon_Contact );
    }
}

// [SECTION] TeachingTip

void ImFluent::OpenTeachingTip( const char * id )
{
    ImGui::OpenPopup( id );
    ImGuiContext & g = *ImGui::GetCurrentContext();
    StorePopupAnchor( ImGui::GetID( id ), g.LastItemData.Rect );
}

bool ImFluent::BeginTeachingTip( const char * id, const char * title, ImFluentTeachingTipPlacement placement )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImFluentStackGuard g;
    g.PushStyleColor( ImGuiCol_PopupBg, ImFluent::GetColorU32( ImFluentCol_SolidBgQuarternary ) );
    g.PushStyleColor( ImGuiCol_Border, ImFluent::GetColorU32( ImFluentCol_SurfaceStrokeFlyout ) );
    g.PushStyleVar( ImGuiStyleVar_PopupRounding, FluentDpx( style.OverlayCornerRadius ) );
    g.PushStyleVar( ImGuiStyleVar_WindowPadding,
                    ImVec2( FluentDpx( style.SpacingXLarge ), FluentDpx( style.SpacingLarge ) ) );

    if ( ImGui::IsPopupOpen( id, ImGuiPopupFlags_None ) )
    {
        const ImGuiID pid = ImGui::GetID( id );
        const ImRect * anchor = FindPopupAnchor( pid );
        if ( anchor )
        {
            const float gap = FluentDpx( style.SpacingMedium );
            ImVec2 pos;
            ImVec2 pivot( 0.f, 0.f );
            switch ( placement )
            {
                case ImFluentTeachingTipPlacement_Top:
                    pos = ImVec2( (anchor->Min.x + anchor->Max.x) * 0.5f, anchor->Min.y - gap );
                    pivot = ImVec2( 0.5f, 1.f );
                    break;
                case ImFluentTeachingTipPlacement_Left:
                    pos = ImVec2( anchor->Min.x - gap, (anchor->Min.y + anchor->Max.y) * 0.5f );
                    pivot = ImVec2( 1.f, 0.5f );
                    break;
                case ImFluentTeachingTipPlacement_Right:
                    pos = ImVec2( anchor->Max.x + gap, (anchor->Min.y + anchor->Max.y) * 0.5f );
                    pivot = ImVec2( 0.f, 0.5f );
                    break;
                case ImFluentTeachingTipPlacement_Bottom:
                default:
                    pos = ImVec2( (anchor->Min.x + anchor->Max.x) * 0.5f, anchor->Max.y + gap );
                    pivot = ImVec2( 0.5f, 0.f );
                    break;
            }
            ImGui::SetNextWindowPos( pos, ImGuiCond_Appearing, pivot );
        }
    }

    if ( !ImGui::BeginPopup( id ) )
        return false;
    g.Forget();
    if ( title && *title )
    {
        TextBlock( title, ImFluentTextStyle_BodyStrong );
        ImGui::Dummy( ImVec2( 0.f, FluentDpx( style.SpacingXSmall ) ) );
    }
    return true;
}

void ImFluent::EndTeachingTip()
{
    ImGui::EndPopup();
    ImGui::PopStyleVar( 2 );
    ImGui::PopStyleColor( 2 );
}

// [SECTION] TitleBar

bool ImFluent::BeginTitleBar( const char * title, float height )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float ctrl_h   = FluentDpx( style.ControlHeight );
    const float h        = ImMax( ctrl_h, ( height > 0.f ) ? height : FluentDpx( style.TitleBarHeight ) );
    const float pad_y    = ( h - ctrl_h ) * 0.5f;
    ImFluentStackGuard g;
    g.PushStyleColor( ImGuiCol_ChildBg, ImFluent::GetColorU32( ImFluentCol_SolidBgBase ) );
    g.PushStyleVar( ImGuiStyleVar_WindowPadding,
                    ImVec2( FluentDpx( style.SpacingMedium ), pad_y ) );
    g.PushStyleVar( ImGuiStyleVar_ItemSpacing,
                    ImVec2( FluentDpx( style.SpacingMedium ), 0.f ) );
    if ( !ImGui::BeginChild( "##fluent-titlebar", ImVec2( 0, h ),
                             ImGuiChildFlags_AlwaysUseWindowPadding,
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
    {
        ImGui::EndChild();
        return false;
    }
    g.Forget();
    if ( title && *title )
    {
        ImFluent::PushFont( ImFluentTextStyle_Body );
        const float shift = ImMax( 0.f, ( ctrl_h - ImGui::GetFontSize() ) * 0.5f );
        ImGui::SetCursorPosY( ImGui::GetCursorPosY() + shift );
        ImGui::TextUnformatted( title );
        ImFluent::PopFont();
        ImGui::SameLine();
    }
    return true;
}

void ImFluent::EndTitleBar()
{
    ImGui::EndChild();
    ImGui::PopStyleVar( 2 );
    ImGui::PopStyleColor();
}

static bool TitleBarChromeButton( const char * id, const char * glyph, bool enabled )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float bar_h = ImGui::GetWindowSize().y;
    const float btn_w = ImFluent::FluentDpx( style.NavItemHeight );
    const ImGuiID iid = w->GetID( id );
    const float top_y = w->Pos.y;
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( ImVec2( pos.x, top_y ), ImVec2( pos.x + btn_w, top_y + bar_h ) );
    ImGui::ItemSize( ImVec2( btn_w, ImGui::GetFontSize() ) );
    if ( !ImGui::ItemAdd( bb, iid ) ) return false;
    bool hov = false, held = false;
    const bool pressed = enabled ? ImGui::ButtonBehavior( bb, iid, &hov, &held ) : false;
    ImDrawList * dl = w->DrawList;
    const float r = ImFluent::FluentDpx( style.ControlCornerRadius );
    if ( enabled )
    {
        const ImU32 fill_target = (hov || held)
            ? (held ? ImFluent::GetColorU32( ImFluentCol_SubtleFillTertiary )
                    : ImFluent::GetColorU32( ImFluentCol_SubtleFillSecondary ))
            : ImFluent::GetColorU32( ImFluentCol_SubtleFillTransparent );
        dl->AddRectFilled( bb.Min, bb.Max, ImFluent::AnimateColorU32( iid, fill_target ), r );
    }
    const ImU32 textCol = enabled ? ImFluent::GetColorU32( ImFluentCol_TextPrimary )
                                  : ImFluent::GetColorU32( ImFluentCol_TextDisabled );
    const ImVec2 ts = ImGui::CalcTextSize( glyph );
    dl->AddText( ImVec2( bb.Min.x + ( btn_w - ts.x ) * 0.5f,
                         bb.Min.y + ( bar_h - ts.y ) * 0.5f ),
                 textCol, glyph );
    ImGui::SameLine();
    return pressed;
}

bool ImFluent::TitleBarBackButton( bool enabled, bool visible )
{
    if ( !visible ) return false;
    return TitleBarChromeButton( "##tb-back", ImFluentIcon_BackArrow, enabled );
}

bool ImFluent::TitleBarPaneToggleButton( bool enabled, bool visible )
{
    if ( !visible ) return false;
    return TitleBarChromeButton( "##tb-toggle", ImFluentIcon_GlobalNavButton, enabled );
}

static void TitleBarVerticallyCenterCursor()
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float ctrl_h = ImFluent::FluentDpx( style.ControlHeight );
    const float bar_h  = ImGui::GetWindowSize().y;
    const float pad_y  = ImMax( 0.f, ( bar_h - ctrl_h ) * 0.5f );
    const float shift  = ImMax( 0.f, ( ctrl_h - ImGui::GetFontSize() ) * 0.5f );
    const float top    = ImGui::GetWindowPos().y + pad_y + shift;
    ImGui::SetCursorScreenPos( ImVec2( ImGui::GetCursorScreenPos().x, top ) );
}

void ImFluent::TitleBarIcon( const char * glyph )
{
    if ( !glyph || !*glyph ) return;
    TitleBarVerticallyCenterCursor();
    ImGui::TextUnformatted( glyph );
    ImGui::SameLine();
}

void ImFluent::TitleBarTitle( const char * text )
{
    if ( !text || !*text ) return;
    ImFluent::PushFont( ImFluentTextStyle_Body );
    TitleBarVerticallyCenterCursor();
    ImGui::TextUnformatted( text );
    ImFluent::PopFont();
    ImGui::SameLine();
}

void ImFluent::TitleBarSubtitle( const char * subtitle )
{
    if ( !subtitle || !*subtitle ) return;
    ImGui::SameLine();
    ImFluent::PushFont( ImFluentTextStyle_Caption );
    TitleBarVerticallyCenterCursor();
    ImGui::PushStyleColor( ImGuiCol_Text, ImFluent::GetColorU32( ImFluentCol_TextSecondary ) );
    ImGui::TextUnformatted( subtitle );
    ImGui::PopStyleColor();
    ImFluent::PopFont();
}

// [SECTION] MenuBar

bool ImFluent::BeginMenuBar()
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGui::PushStyleColor( ImGuiCol_MenuBarBg, ImFluent::GetColorU32( ImFluentCol_LayerFillDefault ) );
    return ImGui::BeginMainMenuBar();
}
void ImFluent::EndMenuBar()
{
    ImGui::EndMainMenuBar();
    ImGui::PopStyleColor();
}

// [SECTION] CommandBar & AppBar

bool ImFluent::BeginCommandBar( const char * id, float height )
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float h = (height > 0.f) ? height : FluentDpx( style.AppBarButtonHeight );

    ImFluentStackGuard g;
    g.PushStyleColor( ImGuiCol_ChildBg, ImFluent::GetColorU32( ImFluentCol_LayerFillDefault ) );
    g.PushStyleVar( ImGuiStyleVar_WindowPadding,
                    ImVec2( FluentDpx( style.SpacingMedium ), 0.f ) );
    g.PushStyleVar( ImGuiStyleVar_ItemSpacing,
                    ImVec2( FluentDpx( style.SpacingXSmall ), 0.f ) );

    if ( !ImGui::BeginChild( id, ImVec2( 0, h ),
                             ImGuiChildFlags_AlwaysUseWindowPadding,
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
    {
        ImGui::EndChild();
        return false;
    }
    g.Forget();

    ImFluentCommandBarState s;
    ImStrncpy( s.Id, id, sizeof( s.Id ) );
    ImFormatString( s.PopupId, sizeof( s.PopupId ), "##cb-overflow-%s", id );
    s.Height = h;
    g_Ctx.CommandBarStack.push_back( s );
    return true;
}

bool ImFluent::BeginCommandBarOverflow()
{
    if ( g_Ctx.CommandBarStack.empty() ) return false;
    const ImFluentCommandBarState & s = g_Ctx.CommandBarStack.back();
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float chev_w = FluentDpx( style.AppBarButtonHeight );
    const float right = ImGui::GetWindowContentRegionMax().x;
    const float curX = ImGui::GetCursorPosX();
    if ( curX < right - chev_w )
        ImGui::SameLine( right - chev_w );
    SetNextAppBarLabelPosition( ImFluentAppBarLabelPosition_Collapsed );
    if ( AppBarButton( "##cb-more", ImFluentIcon_More, ImVec2( chev_w, s.Height ) ) )
        OpenMenuFlyout( s.PopupId );
    return BeginMenuFlyout( s.PopupId );
}

void ImFluent::EndCommandBarOverflow()
{
    EndMenuFlyout();
}

void ImFluent::EndCommandBar()
{
    if ( !g_Ctx.CommandBarStack.empty() ) g_Ctx.CommandBarStack.pop_back();
    ImGui::EndChild();
    ImGui::PopStyleVar( 2 );
    ImGui::PopStyleColor();
}

void ImFluent::SetNextAppBarLabelPosition( ImFluentAppBarLabelPosition pos )
{
    g_Ctx.NextAppBarLabelPos = pos;
    g_Ctx.NextAppBarLabelPosSet = true;
}

bool ImFluent::AppBarButton( const char * label, const char * glyph, const ImVec2 & size_arg )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    glyph = ConsumePendingGlyph( glyph );
    const ImFluentAppBarLabelPosition pos = ConsumeAppBarLabelPos();
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );

    float defW = FluentDpx( style.AppBarButtonWidth );
    float defH = FluentDpx( style.AppBarButtonHeight );
    if ( pos == ImFluentAppBarLabelPosition_Right )
    {
        const float ts_x = label ? ImGui::CalcTextSize( label ).x : 0.f;
        const float gs_x = glyph ? ImGui::CalcTextSize( glyph ).x : 0.f;
        defW = gs_x + (glyph && label ? FluentDpx( style.SpacingMedium ) : 0.f) + ts_x + FluentDpx( style.SpacingXLarge ) * 2.f;
        defH = FluentDpx( style.ControlHeight );
    }
    else if ( pos == ImFluentAppBarLabelPosition_Collapsed )
    {
        defW = FluentDpx( style.AppBarButtonHeight );
        defH = FluentDpx( style.AppBarButtonHeight );
    }
    const float W = (size_arg.x > 0 ? size_arg.x : defW);
    const float H = (size_arg.y > 0 ? size_arg.y : defH);

    const ImVec2 p = w->DC.CursorPos;
    const ImRect bb( p, ImVec2( p.x + W, p.y + H ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;
    bool hov = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, id, &hov, &held );
    ImDrawList * dl = w->DrawList;
    const float r = FluentDpx( style.ControlCornerRadius );
    ImU32 fill = ResolveSubtleFillState( false, held, hov );
    dl->AddRectFilled( bb.Min, bb.Max, AnimateColorU32( id, fill ), r );
    DrawAppBarContent( dl, bb, label, glyph, ImFluent::GetColorU32( ImFluentCol_TextPrimary ), pos, style );
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, r );
    return pressed;
}

bool ImFluent::AppBarToggleButton( const char * label, const char * glyph, bool * v, const ImVec2 & size_arg )
{
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return false;
    glyph = ConsumePendingGlyph( glyph );
    const ImFluentAppBarLabelPosition pos = ConsumeAppBarLabelPos();
    const ImFluentStyle & style = ImFluent::GetStyle();
    const ImGuiID id = w->GetID( label );

    float defW = FluentDpx( style.AppBarButtonWidth );
    float defH = FluentDpx( style.AppBarButtonHeight );
    if ( pos == ImFluentAppBarLabelPosition_Right )
    {
        const float ts_x = label ? ImGui::CalcTextSize( label ).x : 0.f;
        const float gs_x = glyph ? ImGui::CalcTextSize( glyph ).x : 0.f;
        defW = gs_x + (glyph && label ? FluentDpx( style.SpacingMedium ) : 0.f) + ts_x + FluentDpx( style.SpacingXLarge ) * 2.f;
        defH = FluentDpx( style.ControlHeight );
    }
    else if ( pos == ImFluentAppBarLabelPosition_Collapsed )
    {
        defW = FluentDpx( style.AppBarButtonHeight );
        defH = FluentDpx( style.AppBarButtonHeight );
    }
    const float W = (size_arg.x > 0 ? size_arg.x : defW);
    const float H = (size_arg.y > 0 ? size_arg.y : defH);

    const ImVec2 p = w->DC.CursorPos;
    const ImRect bb( p, ImVec2( p.x + W, p.y + H ) );
    ImGui::ItemSize( bb );
    if ( !ImGui::ItemAdd( bb, id ) ) return false;
    bool hov = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, id, &hov, &held );
    if ( pressed && v ) *v = !*v;
    const bool on = v && *v;

    ImDrawList * dl = w->DrawList;
    const float r = FluentDpx( style.ControlCornerRadius );
    ImU32 fill;
    if ( on )
        fill = held ? ImFluent::GetColorU32( ImFluentCol_AccentFillDisabled )
        : hov ? ImFluent::GetColorU32( ImFluentCol_AccentFillTertiary )
        : ImFluent::GetColorU32( ImFluentCol_AccentFillSecondary );
    else
        fill = ResolveSubtleFillState( false, held, hov );
    dl->AddRectFilled( bb.Min, bb.Max, AnimateColorU32( id, fill ), r );
    const ImU32 textCol = on ? ImFluent::GetColorU32( ImFluentCol_TextOnAccentPrimary )
        : ImFluent::GetColorU32( ImFluentCol_TextPrimary );
    DrawAppBarContent( dl, bb, label, glyph, textCol, pos, style );
    if ( IsItemFocused( id ) ) DrawFocusRing( dl, bb, r );
    return pressed;
}

void ImFluent::AppBarSeparator()
{
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGuiWindow * w = ImGui::GetCurrentWindow();
    if ( w->SkipItems ) return;
    const float H = FluentDpx( style.NavItemHeight );
    const ImVec2 pos = w->DC.CursorPos;
    const ImRect bb( ImVec2( pos.x + FluentDpx( style.SpacingSmall ), pos.y + FluentDpx( style.SpacingMedium ) ),
                     ImVec2( pos.x + FluentDpx( style.SpacingSmall + 1.f ), pos.y + H - FluentDpx( style.SpacingMedium ) ) );
    ImGui::ItemSize( ImVec2( FluentDpx( style.SpacingMedium + 1.f ), H ) );
    if ( !ImGui::ItemAdd( bb, 0 ) ) return;
    w->DrawList->AddRectFilled( bb.Min, bb.Max, ImFluent::GetColorU32( ImFluentCol_DividerStrokeDefault ) );
}

static int DaysInMonth( int year, int month )
{
    static const int kDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int d = kDays[month - 1];
    if ( month == 2 )
    {
        const bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        if ( leap ) d = 29;
    }
    return d;
}

// [SECTION] Date & Time pickers

bool ImFluent::DatePicker( const char * label, ImFluentDate * date )
{
    if ( !date ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGui::PushID( label );
    ImGui::BeginGroup();
    bool changed = false;
    int dim = DaysInMonth( date->Year, date->Month );
    ImGui::PushItemWidth( FluentDpx( style.ControlMinWidth ) );
    int day = date->Day, month = date->Month, year = date->Year;
    if ( ImGui::DragInt( "##day", &day, 0.1f, 1, dim, LocalizeGetMsg( ImFluentLocKey_DatePickerDayFormat ) ) ) { date->Day = day;   changed = true; }
    ImGui::SameLine();
    char month_combo[256];
    int  off = 0;
    for ( int i = 0; i < 12 && off < ( int )sizeof( month_combo ) - 2; ++i )
    {
        const char * m = LocalizeGetMsg( ( ImFluentLocKey )(ImFluentLocKey_MonthJanuary + i) );
        const int    n = ImFormatString( month_combo + off, sizeof( month_combo ) - off, "%s", m );
        off += n + 1;
    }
    month_combo[off] = 0;
    if ( ImGui::Combo( "##month", &month, month_combo ) )
    {
        date->Month = month + 1; date->Day = ImClamp( date->Day, 1, DaysInMonth( date->Year, date->Month ) ); changed = true;
    }
    ImGui::SameLine();
    if ( ImGui::DragInt( "##year", &year, 0.5f, 1900, 2100, LocalizeGetMsg( ImFluentLocKey_DatePickerYearFormat ) ) ) { date->Year = year; date->Day = ImClamp( date->Day, 1, DaysInMonth( date->Year, date->Month ) ); changed = true; }
    ImGui::PopItemWidth();
    if ( label && *label ) { ImGui::SameLine(); ImGui::TextUnformatted( label ); }
    ImGui::EndGroup();
    const ImRect group_rect = ImGui::GetCurrentContext()->LastItemData.Rect;
    ImGui::PopID();
    DrawAndConsumePendingError( group_rect );
    return changed;
}

bool ImFluent::TimePicker( const char * label, ImFluentTime * time )
{
    if ( !time ) return false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    ImGui::PushID( label );
    ImGui::BeginGroup();
    bool changed = false;
    int h = time->Hour, m = time->Minute;
    ImGui::PushItemWidth( FluentDpx( style.NavPaneCompactWidth * 2.f ) );
    if ( ImGui::DragInt( "##hour", &h, 0.1f, 0, 23, LocalizeGetMsg( ImFluentLocKey_TimePickerHourFormat ) ) ) { time->Hour = h; changed = true; }
    ImGui::SameLine();
    if ( ImGui::DragInt( "##minute", &m, 0.5f, 0, 59, LocalizeGetMsg( ImFluentLocKey_TimePickerMinuteFormat ) ) ) { time->Minute = m; changed = true; }
    ImGui::PopItemWidth();
    if ( label && *label ) { ImGui::SameLine(); ImGui::TextUnformatted( label ); }
    ImGui::EndGroup();
    const ImRect group_rect = ImGui::GetCurrentContext()->LastItemData.Rect;
    ImGui::PopID();
    DrawAndConsumePendingError( group_rect );
    return changed;
}

bool ImFluent::CalendarDatePicker( const char * label, ImFluentDate * date, const char * hint )
{
    if ( !date ) return false;
    const char * preview = NULL;
    const char * preview_end = NULL;
    if ( date->Year > 0 )
        ImFormatStringToTempBuffer( &preview, &preview_end, "%04d-%02d-%02d", date->Year, date->Month, date->Day );
    bool changed = false;
    const ImFluentStyle & style = ImFluent::GetStyle();
    const float h = FluentDpx( style.ControlHeight );
    const float w = ImGui::CalcItemWidth();
    ImGui::PushID( label );
    const char * btn_label = preview ? preview
        : (hint ? hint : LocalizeGetMsg( ImFluentLocKey_DatePickerPickADate ));
    if ( ImFluent::Button( btn_label, ImVec2( w, h ) ) )
        ImGui::OpenPopup( "##cal" );
    const ImRect trigger_rect = ImGui::GetCurrentContext()->LastItemData.Rect;
    if ( ImGui::BeginPopup( "##cal" ) )
    {

        if ( ImFluent::Button( "<" ) ) { date->Month--; if ( date->Month < 1 ) { date->Month = 12; date->Year--; } }
        ImGui::SameLine();
        const char * hdr; const char * hdr_end;
        ImFormatStringToTempBuffer( &hdr, &hdr_end, "%s %d",
                                    LocalizeGetMsg( ( ImFluentLocKey )(ImFluentLocKey_MonthJanuary + date->Month - 1) ),
                                    date->Year );
        ImGui::TextUnformatted( hdr, hdr_end );
        ImGui::SameLine();
        if ( ImFluent::Button( ">" ) ) { date->Month++; if ( date->Month > 12 ) { date->Month = 1; date->Year++; } }
        ImGui::Separator();

        const int dim = DaysInMonth( date->Year, date->Month );
        const float cell = FluentDpx( style.ControlHeight );
        for ( int d = 1; d <= dim; ++d )
        {
            const char * buf; const char * buf_end;
            ImFormatStringToTempBuffer( &buf, &buf_end, "%2d", d );
            ( void )buf_end;
            const bool sel = (d == date->Day);
            ImGui::PushID( d );
            if ( sel ) ImGui::PushStyleColor( ImGuiCol_Button, ImFluent::GetColorU32( ImFluentCol_AccentFillDefault ) );
            if ( ImGui::Button( buf, ImVec2( cell, cell ) ) )
            {
                date->Day = d;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            if ( sel ) ImGui::PopStyleColor();
            ImGui::PopID();
            if ( (d % 7) != 0 ) ImGui::SameLine();
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    if ( label && *label ) { ImGui::SameLine(); ImGui::TextUnformatted( label ); }
    DrawAndConsumePendingError( trigger_rect );
    return changed;
}
