#pragma once

#include "imgui.h"

enum ImFluentThemePreset_
{
    ImFluentThemePreset_Light = 0,
    ImFluentThemePreset_Dark,
    ImFluentThemePreset_HighContrast,
    ImFluentThemePreset_COUNT
};
typedef int ImFluentThemePreset;

enum ImFluentTextStyle_
{
    ImFluentTextStyle_Caption = 0,
    ImFluentTextStyle_Body,
    ImFluentTextStyle_BodyStrong,
    ImFluentTextStyle_Subtitle,
    ImFluentTextStyle_Title,
    ImFluentTextStyle_TitleLarge,
    ImFluentTextStyle_Display,
    ImFluentTextStyle_COUNT
};
typedef int ImFluentTextStyle;

enum ImFluentCol_
{

    ImFluentCol_TextPrimary = 0,
    ImFluentCol_TextSecondary,
    ImFluentCol_TextTertiary,
    ImFluentCol_TextDisabled,
    ImFluentCol_TextOnAccentPrimary,
    ImFluentCol_TextOnAccentSecondary,
    ImFluentCol_TextOnAccentDisabled,
    ImFluentCol_TextOnAccentSelected,
    ImFluentCol_AccentTextPrimary,
    ImFluentCol_AccentTextSecondary,
    ImFluentCol_AccentTextTertiary,
    ImFluentCol_AccentTextDisabled,

    ImFluentCol_ControlFillDefault,
    ImFluentCol_ControlFillSecondary,
    ImFluentCol_ControlFillTertiary,
    ImFluentCol_ControlFillQuarternary,
    ImFluentCol_ControlFillDisabled,
    ImFluentCol_ControlFillTransparent,
    ImFluentCol_ControlFillInputActive,
    ImFluentCol_ControlAltFillTransparent,
    ImFluentCol_ControlAltFillSecondary,
    ImFluentCol_ControlAltFillTertiary,
    ImFluentCol_ControlAltFillQuarternary,
    ImFluentCol_ControlAltFillDisabled,
    ImFluentCol_ControlSolidFillDefault,
    ImFluentCol_ControlStrongFillDefault,
    ImFluentCol_ControlStrongFillDisabled,

    ImFluentCol_SubtleFillTransparent,
    ImFluentCol_SubtleFillSecondary,
    ImFluentCol_SubtleFillTertiary,
    ImFluentCol_SubtleFillDisabled,

    ImFluentCol_AccentFillDefault,
    ImFluentCol_AccentFillSecondary,
    ImFluentCol_AccentFillTertiary,
    ImFluentCol_AccentFillDisabled,
    ImFluentCol_AccentFillSelectedTextBg,

    ImFluentCol_CardBgDefault,
    ImFluentCol_CardBgSecondary,
    ImFluentCol_LayerFillDefault,
    ImFluentCol_LayerFillAlt,
    ImFluentCol_SmokeFill,
    ImFluentCol_AcrylicFill,
    ImFluentCol_SolidBgBase,
    ImFluentCol_SolidBgQuarternary,

    ImFluentCol_CardStrokeDefault,
    ImFluentCol_CardStrokeSolid,
    ImFluentCol_ControlStrokeDefault,
    ImFluentCol_ControlStrokeSecondary,
    ImFluentCol_ControlStrokeOnAccentDefault,
    ImFluentCol_ControlStrokeOnAccentSecondary,
    ImFluentCol_ControlStrongStrokeDefault,
    ImFluentCol_ControlStrongStrokeDisabled,
    ImFluentCol_SurfaceStrokeDefault,
    ImFluentCol_SurfaceStrokeFlyout,
    ImFluentCol_DividerStrokeDefault,

    ImFluentCol_FocusStrokeOuter,
    ImFluentCol_FocusStrokeInner,

    ImFluentCol_ElevationControlTop,
    ImFluentCol_ElevationControlBottom,
    ImFluentCol_ElevationTextControlTop,
    ImFluentCol_ElevationTextControlBottom,
    ImFluentCol_ElevationTextControlFocusedBottom,
    ImFluentCol_ElevationAccentTop,
    ImFluentCol_ElevationAccentBottom,

    ImFluentCol_SystemFillSuccess,
    ImFluentCol_SystemFillCaution,
    ImFluentCol_SystemFillCritical,
    ImFluentCol_SystemFillNeutral,
    ImFluentCol_SystemFillAttention,

    ImFluentCol_COUNT
};
typedef int ImFluentCol;

enum ImFluentCardStyle_
{
    ImFluentCardStyle_Filled = 0,
    ImFluentCardStyle_Outlined,
};
typedef int ImFluentCardStyle;

enum ImFluentNavViewMode_
{
    ImFluentNavViewMode_LeftCompact = 0,
    ImFluentNavViewMode_LeftOpen,
    ImFluentNavViewMode_LeftAuto,
};
typedef int ImFluentNavViewMode;

enum ImFluentInfoSeverity_
{
    ImFluentInfoSeverity_Informational = 0,
    ImFluentInfoSeverity_Success,
    ImFluentInfoSeverity_Warning,
    ImFluentInfoSeverity_Critical,
};
typedef int ImFluentInfoSeverity;

enum ImFluentTeachingTipPlacement_
{
    ImFluentTeachingTipPlacement_Top = 0,
    ImFluentTeachingTipPlacement_Bottom,
    ImFluentTeachingTipPlacement_Left,
    ImFluentTeachingTipPlacement_Right,
};
typedef int ImFluentTeachingTipPlacement;

enum ImFluentProgressBarState_
{
    ImFluentProgressBarState_Running = 0,
    ImFluentProgressBarState_Paused,
    ImFluentProgressBarState_Error,
};
typedef int ImFluentProgressBarState;

struct ImFluentStyle
{
    float       ControlCornerRadius;
    float       OverlayCornerRadius;
    float       ControlHeight;
    float       ControlMinWidth;
    float       FocusStrokeThicknessOuter;
    float       FocusStrokeThicknessInner;
    float       CardPadding;
    float       NavPaneCompactWidth;
    float       NavPaneOpenWidth;
    ImVec2      ControlContentPadding;

    float       SpacingXSmall;
    float       SpacingSmall;
    float       SpacingMedium;
    float       SpacingLarge;
    float       SpacingXLarge;
    float       SpacingXXLarge;

    float       StrokeThin;
    float       StrokeMedium;
    float       StrokeThick;

    float       ChevronGlyphSize;
    float       StandardIconSize;

    float       CheckboxSize;
    float       RadioButtonDiameter;
    float       ToggleSwitchWidth;
    float       ToggleSwitchHeight;
    float       ToggleSwitchThumbRadiusOff;
    float       ToggleSwitchThumbRadiusOn;
    float       SliderTrackHeight;
    float       SliderThumbRadius;
    float       SliderThumbInnerRadius;
    float       ProgressBarHeight;
    float       ProgressRingThickness;
    float       RatingStarSize;

    float       NavItemHeight;
    float       MenuItemHeight;
    float       ListItemHeight;
    float       TitleBarHeight;
    float       AppBarButtonWidth;
    float       AppBarButtonHeight;
    float       SpinButtonWidth;
    float       RevealButtonWidth;
    float       BadgeHeight;
    float       PipDotSize;

    float       SeverityBarThickness;
    float       SelectionIndicatorThickness;
    float       SelectionIndicatorInset;
    float       TextInputAccentLineThickness;

    IMGUI_API   ImFluentStyle();

    ImVec4      Colors[ImFluentCol_COUNT];
};

namespace ImFluent
{
    IMGUI_API ImFluentStyle &       GetStyle();

    IMGUI_API void                  SetThemePreset( ImFluentThemePreset preset );
    IMGUI_API ImFluentThemePreset   GetThemePreset();

    IMGUI_API ImU32                 GetColorU32( ImFluentCol idx, float alpha_mul = 1.0f );
    IMGUI_API const ImVec4 &        GetStyleColorVec4( ImFluentCol idx );

    IMGUI_API void      PushFluentStyle();
    IMGUI_API void      PopFluentStyle();

    IMGUI_API float     FluentDpx( float v );
    IMGUI_API ImVec2    FluentDpx( const ImVec2 & v );

    IMGUI_API void      SetFont( ImFluentTextStyle style, ImFont * font );
    IMGUI_API ImFont *  GetFont( ImFluentTextStyle style );

    IMGUI_API void      LoadFluentFonts();
    IMGUI_API void      PushFont( ImFluentTextStyle style );
    IMGUI_API void      PopFont();

    IMGUI_API void      SetNextItemHeader( const char * text );
    IMGUI_API void      SetNextItemDescription( const char * text );
    IMGUI_API void      SetNextItemGlyph( const char * glyph );

    IMGUI_API bool      Button( const char * label, const ImVec2 & size = ImVec2( 0, 0 ) );
    IMGUI_API bool      AccentButton( const char * label, const ImVec2 & size = ImVec2( 0, 0 ) );
    IMGUI_API bool      HyperlinkButton( const char * label );
    IMGUI_API bool      ToggleButton( const char * label, bool * v, const ImVec2 & size = ImVec2( 0, 0 ) );
    IMGUI_API bool      RepeatButton( const char * label, const ImVec2 & size = ImVec2( 0, 0 ) );
    IMGUI_API bool      DropDownButton( const char * label, const ImVec2 & size = ImVec2( 0, 0 ) );
    IMGUI_API bool      SplitButton( const char * label, bool * dropdown_clicked, const ImVec2 & size = ImVec2( 0, 0 ) );
    IMGUI_API bool      ToggleSplitButton( const char * label, bool * v, bool * dropdown_clicked, const ImVec2 & size = ImVec2( 0, 0 ) );

    IMGUI_API bool      Checkbox( const char * label, bool * v );
    IMGUI_API bool      CheckboxTristate( const char * label, int * v_state );
    IMGUI_API bool      RadioButton( const char * label, bool active );
    IMGUI_API bool      RadioButton( const char * label, int * v, int v_button );
    IMGUI_API bool      ToggleSwitch( const char * label, bool * v, const char * on_text = "On", const char * off_text = "Off" );
    IMGUI_API bool      RatingControl( const char * label, float * value, int max_stars = 5 );

    IMGUI_API bool      Slider( const char * label, float * v, float v_min, float v_max, const char * format = "%.2f", ImGuiSliderFlags flags = 0 );
    IMGUI_API bool      SliderInt( const char * label, int * v, int v_min, int v_max, const char * format = "%d", ImGuiSliderFlags flags = 0 );
    IMGUI_API void      ProgressBar( float fraction, const ImVec2 & size_arg = ImVec2( -1.f, 0 ), const char * overlay = NULL, ImFluentProgressBarState state = ImFluentProgressBarState_Running );
    IMGUI_API void      ProgressRing( float diameter_dpx = 32.f, float fraction = -1.f );

    IMGUI_API bool      TextBox( const char * label, char * buf, size_t buf_size, const char * hint = NULL, ImGuiInputTextFlags flags = 0 );
    IMGUI_API bool      PasswordBox( const char * label, char * buf, size_t buf_size, const char * hint = NULL, ImGuiInputTextFlags flags = 0 );
    IMGUI_API bool      NumberBox( const char * label, double * v, double step = 1.0, double step_fast = 10.0, const char * format = "%.3f", ImGuiInputTextFlags flags = 0 );
    IMGUI_API bool      RichEditBox( const char * label, char * buf, size_t buf_size, const ImVec2 & size = ImVec2( 0, 0 ), ImGuiInputTextFlags flags = 0 );
    IMGUI_API bool      AutoSuggestBox( const char * label, char * buf, size_t buf_size, const char * const items[], int items_count, int * selected_index = NULL, const char * hint = NULL, ImGuiInputTextFlags flags = 0 );
    IMGUI_API void      TextBlock( const char * text, ImFluentTextStyle style = ImFluentTextStyle_Body );
    IMGUI_API void      TextBlockColored( const char * text, ImU32 color_u32, ImFluentTextStyle style = ImFluentTextStyle_Body );

    IMGUI_API void      Separator();
    IMGUI_API void      SetItemTooltip( const char * fmt, ... ) IM_FMTARGS( 1 );

    IMGUI_API bool      BeginCard( const char * id, const ImVec2 & size = ImVec2( 0, 0 ), ImFluentCardStyle style = ImFluentCardStyle_Filled );
    IMGUI_API void      EndCard();

    IMGUI_API bool      BeginExpander( const char * label, bool * open );
    IMGUI_API void      EndExpander();

    IMGUI_API bool      BeginScrollView( const char * id, const ImVec2 & size = ImVec2( 0, 0 ) );
    IMGUI_API void      EndScrollView();

    IMGUI_API bool      BeginTabView( const char * id );
    IMGUI_API void      EndTabView();
    IMGUI_API bool      BeginTabItem( const char * label, bool * p_open = NULL, ImGuiTabItemFlags flags = 0 );
    IMGUI_API void      EndTabItem();

    IMGUI_API bool      BeginSelectorBar( const char * id );
    IMGUI_API bool      SelectorBarItem( const char * label, bool selected, const char * glyph = NULL );
    IMGUI_API void      EndSelectorBar();

    IMGUI_API bool      BeginNavigationView( const char * id, ImFluentNavViewMode * mode );
    IMGUI_API bool      NavItem( const char * label, bool selected, const char * glyph = NULL );
    IMGUI_API void      NavSubHeader( const char * text );
    IMGUI_API void      EndNavigationView();
    IMGUI_API void      NavigationViewBeginContent();
    IMGUI_API void      NavigationViewEndContent();

    IMGUI_API bool       ComboBox( const char * label, int * current_item, const char * const items[], int items_count );
    IMGUI_API bool       ListBox( const char * label, int * current_item, const char * const items[], int items_count, int height_in_items = 7 );
    IMGUI_API bool       ListViewItem( const char * label, bool selected = false, const char * glyph = NULL );
    IMGUI_API bool       TreeNode( const char * label, bool * p_open );

    IMGUI_API bool       TreeNode( const char * label, bool * p_open, bool * p_checked );
    IMGUI_API void       TreePop();
    IMGUI_API bool       GridViewItem( const char * label, bool selected, const ImVec2 & size );
    IMGUI_API bool       PipsPager( const char * id, int * current_item, int total_pages );
    IMGUI_API int        BreadcrumbBar( const char * id, const char * const items[], int items_count );

    IMGUI_API void      OpenFlyout( const char * id );
    IMGUI_API bool      BeginFlyout( const char * id );
    IMGUI_API void      EndFlyout();

    IMGUI_API void      OpenMenuFlyout( const char * id );
    IMGUI_API bool      BeginMenuFlyout( const char * id );
    IMGUI_API bool      MenuFlyoutItem( const char * label, const char * shortcut = NULL, const char * glyph = NULL, bool selected = false, bool enabled = true );
    IMGUI_API void      MenuFlyoutSeparator();
    IMGUI_API void      EndMenuFlyout();

    IMGUI_API bool      BeginCommandBarFlyout( const char * id );
    IMGUI_API void      EndCommandBarFlyout();

    IMGUI_API void      OpenContentDialog( const char * id );
    IMGUI_API bool      BeginContentDialog( const char * id, const char * title );
    IMGUI_API int       EndContentDialog( const char * primary = "OK", const char * secondary = NULL, const char * close_text = "Cancel" );

    IMGUI_API void      InfoBar( ImFluentInfoSeverity severity, const char * title, const char * message, bool * is_open = NULL, const char * glyph_override = NULL );
    IMGUI_API void      InfoBadge( int count = -1, const char * glyph = NULL );

    IMGUI_API bool      BeginMenuBar();
    IMGUI_API void      EndMenuBar();
    IMGUI_API bool      AppBarButton( const char * label, const char * glyph, const ImVec2 & size = ImVec2( 0, 0 ) );
    IMGUI_API void      AppBarSeparator();

    struct ImFluentDate { int Year; int Month; int Day; };
    struct ImFluentTime { int Hour; int Minute; };

    IMGUI_API bool      DatePicker( const char * label, ImFluentDate * date );
    IMGUI_API bool      TimePicker( const char * label, ImFluentTime * time );
    IMGUI_API bool      CalendarDatePicker( const char * label, ImFluentDate * date, const char * hint = "Pick a date" );

    IMGUI_API void      ShowDemoWindow( bool * p_open = NULL );

}
