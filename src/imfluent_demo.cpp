// imfluent_demo.cpp
// Built-in demo window — WinUI 3 Gallery viewer. Defines
// ImFluent::ShowDemoWindow (declared in imfluent.h).
//
// When IMFLUENT_DISABLE_DEMO_WINDOWS is defined, the entire body compiles out
// and ShowDemoWindow becomes an empty stub (mirrors ImGui's
// IMGUI_DISABLE_DEMO_WINDOWS convention).

#include "imfluent.h"

#if !defined(IMFLUENT_DISABLE_DEMO_WINDOWS)

#include "imfluent_icons.h"

#include <imgui.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>

namespace ImFluentGalleryApp
{

using namespace ImFluent;

// ============================================================================
// ControlInfo data
// ============================================================================

struct ControlInfo
{
    const char*  UniqueId;
    const char*  GroupId;
    const char*  Title;
    const char*  Subtitle;
    const char*  Glyph;       // points at one of ImFluentIcon_* in imfluent_icons.h
    void       (*PageFn)();   // demo function for the ItemPage
};

struct GroupInfo
{
    const char* GroupId;
    const char* Title;
    const char* Glyph;
};

// Forward declarations of every Page_Item_* — defined later in this TU.
static void Page_Item_Button         ();
static void Page_Item_HyperlinkButton();
static void Page_Item_DropDownButton ();
static void Page_Item_SplitButton    ();
static void Page_Item_ToggleButton   ();
static void Page_Item_RepeatButton   ();
static void Page_Item_CheckBox       ();
static void Page_Item_RadioButton    ();
static void Page_Item_ToggleSwitch   ();
static void Page_Item_RatingControl  ();
static void Page_Item_Slider         ();
static void Page_Item_ProgressBar    ();
static void Page_Item_ProgressRing   ();
static void Page_Item_TextBox        ();
static void Page_Item_PasswordBox    ();
static void Page_Item_NumberBox      ();
static void Page_Item_AutoSuggestBox ();
static void Page_Item_RichEditBox    ();
static void Page_Item_TextBlock      ();
static void Page_Item_ComboBox       ();
static void Page_Item_ListBox        ();
static void Page_Item_ListView       ();
static void Page_Item_TreeView       ();
static void Page_Item_GridView       ();
static void Page_Item_PipsPager      ();
static void Page_Item_BreadcrumbBar  ();
static void Page_Item_Card           ();
static void Page_Item_Expander       ();
static void Page_Item_TabView        ();
static void Page_Item_NavigationView ();
static void Page_Item_SelectorBar    ();
static void Page_Item_InfoBar        ();
static void Page_Item_InfoBadge      ();
static void Page_Item_ToolTip        ();
static void Page_Item_ContentDialog  ();
static void Page_Item_Flyout         ();
static void Page_Item_MenuFlyout     ();
static void Page_Item_AppBarButton   ();
static void Page_Item_DatePicker     ();
static void Page_Item_TimePicker     ();
static void Page_Item_CalendarDatePicker();
static void Page_Item_SettingsCard   ();
static void Page_Item_StackPanel     ();
static void Page_Item_WrapPanel      ();
static void Page_Item_TeachingTip    ();
static void Page_Item_TitleBar       ();
static void Page_Item_StyleStack     ();

static const ControlInfo g_Controls[] = {
    // ---- Basic Input -------------------------------------------------------
    { "Button",          "BasicInput",  "Button",            "A control that responds to user input and raises a Click event.", ImFluentIcon_Add,           &Page_Item_Button },
    { "HyperlinkButton", "BasicInput",  "HyperlinkButton",   "A button that appears as hyperlink text and can navigate to a URI.", ImFluentIcon_Share,         &Page_Item_HyperlinkButton },
    { "DropDownButton",  "BasicInput",  "DropDownButton",    "A button with a chevron that opens a flyout when clicked.",        ImFluentIcon_ChevronDown,   &Page_Item_DropDownButton },
    { "SplitButton",     "BasicInput",  "SplitButton",       "A button with two parts: an action button and a chevron drop-down.", ImFluentIcon_ChevronDown,   &Page_Item_SplitButton },
    { "ToggleButton",    "BasicInput",  "ToggleButton",      "A button that can be on, off, or indeterminate.",                  ImFluentIcon_CheckboxOn,    &Page_Item_ToggleButton },
    { "RepeatButton",    "BasicInput",  "RepeatButton",      "A button that fires its Click event repeatedly while held.",       ImFluentIcon_Refresh,       &Page_Item_RepeatButton },
    { "CheckBox",        "BasicInput",  "CheckBox",          "A control a user can select or clear.",                            ImFluentIcon_CheckboxOn,    &Page_Item_CheckBox },
    { "RadioButton",     "BasicInput",  "RadioButton",       "A control that allows a user to select one option from a group.",  ImFluentIcon_RadioBtnOn,    &Page_Item_RadioButton },
    { "ToggleSwitch",    "BasicInput",  "ToggleSwitch",      "A switch that can be toggled between two states.",                 ImFluentIcon_Settings,      &Page_Item_ToggleSwitch },
    { "RatingControl",   "BasicInput",  "RatingControl",     "Lets users rate something on a 5-star scale.",                      ImFluentIcon_StarFilled,    &Page_Item_RatingControl },
    { "Slider",          "BasicInput",  "Slider",            "A control that lets the user select from a range of values.",      ImFluentIcon_Spacing,       &Page_Item_Slider },

    // ---- Status & Info -----------------------------------------------------
    { "ProgressBar",     "StatusAndInfo", "ProgressBar",     "Shows the progress of a long-running operation.",                  ImFluentIcon_Refresh,       &Page_Item_ProgressBar },
    { "ProgressRing",    "StatusAndInfo", "ProgressRing",    "Shows the progress of a long-running operation as a ring.",        ImFluentIcon_Refresh,       &Page_Item_ProgressRing },
    { "InfoBar",         "StatusAndInfo", "InfoBar",         "Inline notification for app-wide status messages.",                 ImFluentIcon_Info,          &Page_Item_InfoBar },
    { "InfoBadge",       "StatusAndInfo", "InfoBadge",       "Small contextual indicator for new content or notifications.",     ImFluentIcon_Important,     &Page_Item_InfoBadge },
    { "ToolTip",         "StatusAndInfo", "ToolTip",         "Pops up additional info about an element on hover.",                ImFluentIcon_Info,          &Page_Item_ToolTip },

    // ---- Text --------------------------------------------------------------
    { "TextBlock",       "Text",        "TextBlock",         "Displays small amounts of read-only text.",                         ImFluentIcon_Typography,    &Page_Item_TextBlock },
    { "TextBox",         "Text",        "TextBox",           "A single-line plain-text input field.",                             ImFluentIcon_Edit,          &Page_Item_TextBox },
    { "PasswordBox",     "Text",        "PasswordBox",       "A control for entering passwords.",                                  ImFluentIcon_Hide,          &Page_Item_PasswordBox },
    { "NumberBox",       "Text",        "NumberBox",         "Numeric input with up/down spin buttons.",                          ImFluentIcon_Add,           &Page_Item_NumberBox },
    { "AutoSuggestBox",  "Text",        "AutoSuggestBox",    "A text-box that gives suggestions as the user types.",              ImFluentIcon_Search,        &Page_Item_AutoSuggestBox },
    { "RichEditBox",     "Text",        "RichEditBox",       "Multi-line text input.",                                            ImFluentIcon_Document,      &Page_Item_RichEditBox },

    // ---- Collections -------------------------------------------------------
    { "ComboBox",        "Collections", "ComboBox",          "A drop-down list of items.",                                        ImFluentIcon_ChevronDown,   &Page_Item_ComboBox },
    { "ListBox",         "Collections", "ListBox",           "A control that lets users select from a list.",                     ImFluentIcon_AllControls,   &Page_Item_ListBox },
    { "ListView",        "Collections", "ListView",          "A vertical list of items with selection.",                          ImFluentIcon_AllControls,   &Page_Item_ListView },
    { "TreeView",        "Collections", "TreeView",          "A hierarchical list of items.",                                     ImFluentIcon_Folder,        &Page_Item_TreeView },
    { "GridView",        "Collections", "GridView",          "A grid layout of selectable items.",                                ImFluentIcon_AllControls,   &Page_Item_GridView },
    { "PipsPager",       "Collections", "PipsPager",         "A pager rendered as a row of dots.",                                ImFluentIcon_More,          &Page_Item_PipsPager },

    // ---- Layout / Containers -----------------------------------------------
    { "Card",            "Layout",      "Card",              "A surface that groups related content.",                            ImFluentIcon_Folder,        &Page_Item_Card },
    { "SettingsCard",    "Layout",      "SettingsCard",      "Card row used on settings pages: glyph + header + description + control slot.", ImFluentIcon_Settings, &Page_Item_SettingsCard },
    { "Expander",        "Layout",      "Expander",          "A control with a header that expands to reveal a body.",            ImFluentIcon_ChevronDown,   &Page_Item_Expander },
    { "StackPanel",      "Layout",      "StackPanel",        "Linear container with uniform spacing; horizontal or vertical.",    ImFluentIcon_Spacing,       &Page_Item_StackPanel },
    { "WrapPanel",       "Layout",      "WrapPanel",         "Lays children left-to-right and wraps to a new row when full.",     ImFluentIcon_Spacing,       &Page_Item_WrapPanel },
    { "TitleBar",        "Layout",      "TitleBar",          "Custom title-bar shell hosting nav chevrons, search, and actions.", ImFluentIcon_GlobalNavButton, &Page_Item_TitleBar },

    // ---- Navigation --------------------------------------------------------
    { "TabView",         "Navigation",  "TabView",           "A control with multiple tabs the user can switch between.",         ImFluentIcon_AllControls,   &Page_Item_TabView },
    { "NavigationView",  "Navigation",  "NavigationView",    "A side-pane navigation control.",                                   ImFluentIcon_GlobalNavButton, &Page_Item_NavigationView },
    { "SelectorBar",     "Navigation",  "SelectorBar",       "A horizontal list of pill-shaped pivot items.",                     ImFluentIcon_AllControls,   &Page_Item_SelectorBar },
    { "BreadcrumbBar",   "Navigation",  "BreadcrumbBar",     "A trail of clickable parent items showing the current location.",   ImFluentIcon_ChevronRight,  &Page_Item_BreadcrumbBar },

    // ---- Dialogs & Flyouts -------------------------------------------------
    { "ContentDialog",   "DialogsAndFlyouts", "ContentDialog","A modal dialog with title, body, and action buttons.",             ImFluentIcon_Important,     &Page_Item_ContentDialog },
    { "Flyout",          "DialogsAndFlyouts", "Flyout",      "A lightweight contextual popup.",                                   ImFluentIcon_More,          &Page_Item_Flyout },
    { "MenuFlyout",      "DialogsAndFlyouts", "MenuFlyout",  "A flyout with a list of menu items.",                               ImFluentIcon_More,          &Page_Item_MenuFlyout },
    { "TeachingTip",     "DialogsAndFlyouts", "TeachingTip", "Anchored callout used to teach a feature; placement = Top/Bottom/Left/Right.", ImFluentIcon_Info, &Page_Item_TeachingTip },

    // ---- Menus & toolbars --------------------------------------------------
    { "AppBarButton",    "MenusAndToolbars", "AppBarButton", "A toolbar button with an icon glyph above its label.",              ImFluentIcon_Add,           &Page_Item_AppBarButton },

    // ---- Date & Time -------------------------------------------------------
    { "DatePicker",         "DateTime", "DatePicker",         "Lets a user pick a date.",                                          ImFluentIcon_Calendar,    &Page_Item_DatePicker },
    { "TimePicker",         "DateTime", "TimePicker",         "Lets a user pick a time.",                                          ImFluentIcon_Clock,       &Page_Item_TimePicker },
    { "CalendarDatePicker", "DateTime", "CalendarDatePicker", "Drop-down calendar for picking a date.",                            ImFluentIcon_Calendar,    &Page_Item_CalendarDatePicker },

    // ---- Design / Theming --------------------------------------------------
    { "StyleStack",         "Design",   "Style stack",        "Push/pop Fluent color and sizing tokens for scoped overrides.",     ImFluentIcon_Color,       &Page_Item_StyleStack },
};
static const int g_ControlsCount = (int)(sizeof(g_Controls) / sizeof(g_Controls[0]));

static const GroupInfo g_Groups[] = {
    { "BasicInput",         "Basic input",        ImFluentIcon_Add },
    { "Collections",        "Collections",        ImFluentIcon_AllControls },
    { "DateTime",           "Date & time",        ImFluentIcon_Calendar },
    { "Design",             "Design",             ImFluentIcon_Color },
    { "DialogsAndFlyouts",  "Dialogs & flyouts",  ImFluentIcon_Important },
    { "Layout",             "Layout",             ImFluentIcon_Folder },
    { "MenusAndToolbars",   "Menus & toolbars",   ImFluentIcon_More },
    { "Navigation",         "Navigation",         ImFluentIcon_GlobalNavButton },
    { "StatusAndInfo",      "Status & info",      ImFluentIcon_Info },
    { "Text",               "Text",               ImFluentIcon_Typography },
};
static const int g_GroupsCount = (int)(sizeof(g_Groups) / sizeof(g_Groups[0]));

static const ControlInfo* FindControl(const char* uniqueId)
{
    if (!uniqueId) return nullptr;
    for (int i = 0; i < g_ControlsCount; ++i)
        if (std::strcmp(g_Controls[i].UniqueId, uniqueId) == 0)
            return &g_Controls[i];
    return nullptr;
}

// ============================================================================
// Navigation state + page router
// ============================================================================
namespace
{
struct GalleryState
{
    std::vector<std::string> NavStack;
    int                      NavCursor = -1;
    ImFluentNavViewMode      NavMode   = ImFluentNavViewMode_LeftOpen;
    char                     SearchBuf[128] = {};
    int                      ExpandedGroup  = -1; // -1 = none, otherwise index into g_Groups
    bool                     ResetScroll    = false; // set after each navigation
};
}
static GalleryState g_State;

static void Navigate(const char* page_id)
{
    if (!page_id) return;
    if (g_State.NavCursor + 1 < (int)g_State.NavStack.size())
        g_State.NavStack.resize(g_State.NavCursor + 1);
    g_State.NavStack.push_back(page_id);
    g_State.NavCursor = (int)g_State.NavStack.size() - 1;
    g_State.ResetScroll = true;
}
static void NavigateBack()    { if (g_State.NavCursor > 0)                                 { g_State.NavCursor--; g_State.ResetScroll = true; } }
static void NavigateForward() { if (g_State.NavCursor + 1 < (int)g_State.NavStack.size())  { g_State.NavCursor++; g_State.ResetScroll = true; } }
static const char* CurrentPageId()
{
    if (g_State.NavCursor < 0 || g_State.NavCursor >= (int)g_State.NavStack.size())
        return "Home";
    return g_State.NavStack[g_State.NavCursor].c_str();
}

// ============================================================================
// PageHeader + ControlExample helpers
// ============================================================================
static void PageHeader(const char* title, const char* subtitle = nullptr)
{
    TextBlock(title, ImFluentTextStyle_TitleLarge);
    if (subtitle && *subtitle)
    {
        const ImFluentStyle& style = ImFluent::GetStyle();
        TextBlockColored(subtitle, GetColorU32(ImFluentCol_TextSecondary), ImFluentTextStyle_Body);
    }
    ImGui::Dummy(ImVec2(0.f, FluentDpx(8.f)));
    ImFluent::Separator();
    ImGui::Dummy(ImVec2(0.f, FluentDpx(8.f)));
}

namespace
{
struct ControlExampleState
{
    bool        SourceVisible = false;
    int         SourceTab     = 0; // 0=XAML, 1=C#
    const char* XamlCode      = nullptr;
    const char* CSharpCode    = nullptr;
    bool        OutputUsed    = false;
    bool        OptionsUsed   = false;
    char        Output[512]   = {};
};
}
static ControlExampleState g_CE;

static void BeginControlExample(const char* header)
{
    g_CE.OutputUsed  = false;
    g_CE.OptionsUsed = false;
    g_CE.XamlCode    = nullptr;
    g_CE.CSharpCode  = nullptr;
    g_CE.Output[0]   = 0;
    TextBlock(header, ImFluentTextStyle_BodyStrong);
    ImGui::Dummy(ImVec2(0.f, FluentDpx(4.f)));
    BeginCard(header, ImVec2(0.f, 0.f), ImFluentCardStyle_Filled);
}

static void ControlExampleOptionsHeader()
{
    g_CE.OptionsUsed = true;
    ImFluent::Separator();
    TextBlock("Options:", ImFluentTextStyle_Caption);
}

static void ControlExampleOutput(const char* fmt, ...)
{
    g_CE.OutputUsed = true;
    va_list ap; va_start(ap, fmt);
    std::vsnprintf(g_CE.Output, sizeof(g_CE.Output), fmt, ap);
    va_end(ap);
    ImGui::Dummy(ImVec2(0.f, FluentDpx(8.f)));
    ImFluent::Separator();
    char line[576];
    std::snprintf(line, sizeof(line), "Output: %s", g_CE.Output);
    TextBlock(line, ImFluentTextStyle_Caption);
}

static void ControlExampleSourceHeader(const char* xaml_code, const char* csharp_code)
{
    g_CE.XamlCode   = xaml_code;
    g_CE.CSharpCode = csharp_code;
}

static void EndControlExample()
{
    if (g_CE.XamlCode || g_CE.CSharpCode)
    {
        ImGui::Dummy(ImVec2(0.f, FluentDpx(8.f)));
        ImFluent::Separator();
        ImGui::Dummy(ImVec2(0.f, FluentDpx(4.f)));
        if (BeginExpander("Source code", &g_CE.SourceVisible))
        {
            BeginSelectorBar("##src");
            if (g_CE.XamlCode)   { if (SelectorBarItem("XAML", g_CE.SourceTab == 0)) g_CE.SourceTab = 0; }
            if (g_CE.CSharpCode) { if (SelectorBarItem("C#",   g_CE.SourceTab == 1)) g_CE.SourceTab = 1; }
            EndSelectorBar();
            const char* code = (g_CE.SourceTab == 0 && g_CE.XamlCode) ? g_CE.XamlCode :
                               (g_CE.SourceTab == 1 && g_CE.CSharpCode) ? g_CE.CSharpCode :
                               (g_CE.XamlCode ? g_CE.XamlCode : g_CE.CSharpCode);
            if (code)
                ImGui::TextUnformatted(code);
            EndExpander();
        }
    }
    EndCard();
    ImGui::Dummy(ImVec2(0.f, FluentDpx(16.f)));
}

// ============================================================================
// Per-control demo pages
// ============================================================================

// ---- Buttons family --------------------------------------------------------

static void Page_Item_Button()
{
    PageHeader("Button", "A control that responds to user input and raises a Click event.");

    BeginControlExample("A simple Button");
    static int s_clicks = 0;
    if (Button("Standard button")) ++s_clicks;
    ControlExampleOutput("Click count: %d", s_clicks);
    ControlExampleSourceHeader("<Button Content=\"Standard button\" Click=\"OnClick\" />",
                               "private void OnClick(object sender, RoutedEventArgs e) { ++clicks; }");
    EndControlExample();

    BeginControlExample("Accent button");
    static int s_aclicks = 0;
    if (AccentButton("Accent button")) ++s_aclicks;
    ControlExampleOutput("Click count: %d", s_aclicks);
    ControlExampleSourceHeader("<Button Style=\"{StaticResource AccentButtonStyle}\" Content=\"Accent button\" />",
                               "// AccentButtonStyle is provided by WinUI.");
    EndControlExample();

    BeginControlExample("States");
    Button("Rest");           ImGui::SameLine();
    AccentButton("Accent");   ImGui::SameLine();
    ImGui::BeginDisabled();
    Button("Disabled");       ImGui::SameLine();
    AccentButton("Accent disabled");
    ImGui::EndDisabled();
    EndControlExample();
}

static void Page_Item_HyperlinkButton()
{
    PageHeader("HyperlinkButton", "A button that appears as hyperlink text.");
    BeginControlExample("A hyperlink button");
    static int s = 0;
    if (HyperlinkButton("Open the Fluent 2 design system")) ++s;
    ControlExampleOutput("Clicks: %d", s);
    ControlExampleSourceHeader("<HyperlinkButton Content=\"Open the Fluent 2 design system\" NavigateUri=\"https://fluent2.microsoft.design/\" />",
                               nullptr);
    EndControlExample();
}

static void Page_Item_DropDownButton()
{
    PageHeader("DropDownButton", "A button with a chevron that opens a flyout when clicked.");
    BeginControlExample("DropDown");
    if (DropDownButton("Choose action"))
        OpenMenuFlyout("##dd-menu");
    if (BeginMenuFlyout("##dd-menu"))
    {
        MenuFlyoutItem("New",  "Ctrl+N", ImFluentIcon_Add);
        MenuFlyoutItem("Open", "Ctrl+O", ImFluentIcon_OpenFile);
        MenuFlyoutSeparator();
        MenuFlyoutItem("Exit", "Alt+F4", ImFluentIcon_Cancel);
        EndMenuFlyout();
    }
    EndControlExample();
}

static void Page_Item_SplitButton()
{
    PageHeader("SplitButton", "A button with two parts.");
    BeginControlExample("Split button");
    bool ddClicked = false;
    if (SplitButton("Send", &ddClicked)) {}
    if (ddClicked) OpenMenuFlyout("##sb-menu");
    if (BeginMenuFlyout("##sb-menu"))
    {
        MenuFlyoutItem("Send now");
        MenuFlyoutItem("Send later");
        EndMenuFlyout();
    }
    EndControlExample();
}

static void Page_Item_ToggleButton()
{
    PageHeader("ToggleButton", "A button that can be toggled on or off.");
    BeginControlExample("Toggle");
    static bool s = false;
    ToggleButton("Bold", &s);
    ControlExampleOutput("Bold = %s", s ? "On" : "Off");
    EndControlExample();
}

static void Page_Item_RepeatButton()
{
    PageHeader("RepeatButton", "Fires Click repeatedly while held.");
    BeginControlExample("Counter");
    static int s = 0;
    if (RepeatButton("+ Increment")) ++s;
    ControlExampleOutput("Count: %d", s);
    EndControlExample();
}

// ---- Selection -------------------------------------------------------------

static void Page_Item_CheckBox()
{
    PageHeader("CheckBox", "A control a user can select or clear.");
    BeginControlExample("Two-state");
    static bool s_a = true;
    Checkbox("Sync favorites", &s_a);
    ControlExampleOutput("Sync favorites = %s", s_a ? "true" : "false");
    EndControlExample();

    BeginControlExample("Three-state");
    static int s_t = -1;
    CheckboxTristate("Select all", &s_t);
    ControlExampleOutput("State = %d", s_t);
    EndControlExample();
}

static void Page_Item_RadioButton()
{
    PageHeader("RadioButton", "Choose one of a small set of options.");
    BeginControlExample("Group");
    static int s = 1;
    RadioButton("Option 1", &s, 1);
    RadioButton("Option 2", &s, 2);
    RadioButton("Option 3", &s, 3);
    ControlExampleOutput("Selected = %d", s);
    EndControlExample();

    BeginControlExample("RadioButtons group (multi-column)");
    static int picked = 2;
    static const char* sizes[] = { "Small", "Medium", "Large", "Extra large", "Huge", "Gigantic" };
    RadioButtons("Pick a size", &picked, sizes, IM_ARRAYSIZE(sizes), 3);
    ControlExampleOutput("Picked = %s", sizes[picked]);
    EndControlExample();
}

static void Page_Item_ToggleSwitch()
{
    PageHeader("ToggleSwitch", "Two-state switch.");
    BeginControlExample("Wi-Fi");
    static bool s = true;
    ToggleSwitch("Wi-Fi", &s, "On", "Off");
    ControlExampleOutput("Wi-Fi = %s", s ? "On" : "Off");
    EndControlExample();
}

static void Page_Item_RatingControl()
{
    PageHeader("RatingControl", "Star-based rating input.");
    BeginControlExample("Rate");
    static float r = 3.5f;
    RatingControl("##rate", &r, 5);
    ControlExampleOutput("Rating: %.1f", r);
    EndControlExample();
}

static void Page_Item_Slider()
{
    PageHeader("Slider", "Pick a value from a range.");
    BeginControlExample("Float");
    static float f = 0.5f;
    ImGui::PushItemWidth(280.f);
    Slider("##s1", &f, 0.f, 1.f, "%.2f");
    ImGui::PopItemWidth();
    ControlExampleOutput("Value: %.2f", f);
    EndControlExample();

    BeginControlExample("Int");
    static int i = 50;
    ImGui::PushItemWidth(280.f);
    SliderInt("##s2", &i, 0, 100);
    ImGui::PopItemWidth();
    ControlExampleOutput("Value: %d", i);
    EndControlExample();

    BeginControlExample("Header + Description (SetNextItem* pattern)");
    static float vol = 0.65f;
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Volume");
    SetNextItemDescription("Drag the thumb to adjust system volume.");
    Slider("##s-vol", &vol, 0.f, 1.f, "%.0f%%");
    ImGui::PopItemWidth();
    EndControlExample();

    BeginControlExample("RangeSlider (two thumbs)");
    static float range_lo = 25.f, range_hi = 75.f;
    ImGui::PushItemWidth(280.f);
    RangeSlider("##rs", &range_lo, &range_hi, 0.f, 100.f, "%.0f");
    ImGui::PopItemWidth();
    ControlExampleOutput("Range: [%.0f, %.0f]", range_lo, range_hi);
    EndControlExample();
}

static void Page_Item_ProgressBar()
{
    PageHeader("ProgressBar", "Linear progress indicator. State controls the fill color: Running (accent), Paused (caution), Error (critical).");

    BeginControlExample("Determinate");
    static float p = 0.0f;
    p += ImGui::GetIO().DeltaTime * 0.1f;
    if (p > 1.f) p = 0.f;
    ProgressBar(p, ImVec2(280.f, 0.f), nullptr);
    ControlExampleOutput("Fraction: %.0f%%", p * 100.f);
    EndControlExample();

    BeginControlExample("State (Running / Paused / Error)");
    static int state = 0;
    static const char* state_names[] = { "Running", "Paused", "Error" };
    BeginSelectorBar("##pb-state");
    for (int i = 0; i < 3; ++i)
        if (SelectorBarItem(state_names[i], state == i)) state = i;
    EndSelectorBar();
    ProgressBar(0.6f, ImVec2(280.f, 0.f), nullptr, (ImFluentProgressBarState)state);
    EndControlExample();
}

static void Page_Item_ProgressRing()
{
    PageHeader("ProgressRing", "Circular progress indicator.");
    BeginControlExample("Indeterminate");
    ProgressRing(40.f, -1.f);
    EndControlExample();
    BeginControlExample("Determinate");
    static float p = 0.f;
    p += ImGui::GetIO().DeltaTime * 0.2f;
    if (p > 1.f) p = 0.f;
    ProgressRing(40.f, p);
    ControlExampleOutput("Fraction: %.0f%%", p * 100.f);
    EndControlExample();
}

// ---- Text ------------------------------------------------------------------

static void Page_Item_TextBlock()
{
    PageHeader("TextBlock", "Read-only text in the Fluent type ramp.");
    BeginControlExample("Type ramp");
    TextBlock("Caption — 12 dp",     ImFluentTextStyle_Caption);
    TextBlock("Body — 14 dp",        ImFluentTextStyle_Body);
    TextBlock("BodyStrong — 14 dp",  ImFluentTextStyle_BodyStrong);
    TextBlock("Subtitle — 20 dp",    ImFluentTextStyle_Subtitle);
    TextBlock("Title — 28 dp",       ImFluentTextStyle_Title);
    TextBlock("TitleLarge — 40 dp",  ImFluentTextStyle_TitleLarge);
    EndControlExample();
}

static void Page_Item_TextBox()
{
    PageHeader("TextBox", "Single-line plain text input.");
    BeginControlExample("Text input");
    static char buf[128] = "";
    ImGui::PushItemWidth(280.f);
    TextBox("##tb", buf, sizeof(buf), "Type something...");
    ImGui::PopItemWidth();
    ControlExampleOutput("Value: %s", buf);
    EndControlExample();

    BeginControlExample("With Header + Description (SetNextItem* pattern)");
    static char buf2[128] = "";
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Display name");
    SetNextItemDescription("Shown next to your avatar in conversations.");
    TextBox("##tb-named", buf2, sizeof(buf2), "Enter your name");
    ImGui::PopItemWidth();
    EndControlExample();
}

static void Page_Item_PasswordBox()
{
    PageHeader("PasswordBox", "Hidden text input. Press and hold the eye button on the right to reveal.");
    BeginControlExample("Password");
    static char buf[64] = "";
    ImGui::PushItemWidth(280.f);
    PasswordBox("##pb", buf, sizeof(buf), "Enter password");
    ImGui::PopItemWidth();
    ControlExampleOutput("Length: %d", (int)std::strlen(buf));
    EndControlExample();

    BeginControlExample("With Header + Description");
    static char buf2[64] = "";
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Account password");
    SetNextItemDescription("8+ characters, mix of letters and numbers.");
    PasswordBox("##pb2", buf2, sizeof(buf2), "••••••••");
    ImGui::PopItemWidth();
    EndControlExample();
}

static void Page_Item_NumberBox()
{
    PageHeader("NumberBox", "Numeric input with inline up/down spin buttons. Hold a button for repeat. Ctrl+click steps by step_fast.");
    BeginControlExample("Number");
    static double v = 1.5;
    ImGui::PushItemWidth(280.f);
    NumberBox("##nb", &v, 0.5, 5.0);
    ImGui::PopItemWidth();
    ControlExampleOutput("Value: %.3f", v);
    EndControlExample();

    BeginControlExample("With Header + Description");
    static double qty = 1.0;
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Quantity");
    SetNextItemDescription("Step = 1, Ctrl-step = 10.");
    NumberBox("##nb2", &qty, 1.0, 10.0, "%.0f");
    ImGui::PopItemWidth();
    EndControlExample();
}

static void Page_Item_AutoSuggestBox()
{
    PageHeader("AutoSuggestBox", "Text input with a click-to-pick suggestion list. List filters on what you type and stays open while focused.");
    BeginControlExample("Suggest");
    static const char* items[] = { "Apple", "Banana", "Cherry", "Date", "Elderberry", "Fig", "Grapefruit", "Honeydew", "Kiwi" };
    static char buf[32] = "";
    static int  selected = -1;
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Pick a fruit");
    AutoSuggestBox("##sgb", buf, sizeof(buf), items, IM_ARRAYSIZE(items), &selected, "Type to filter...");
    ImGui::PopItemWidth();
    if (selected >= 0)
    {
        ControlExampleOutput( "Selected #%d: %s", selected, items[selected] );

        int size = std::min( ( int )strlen( items[selected] ), 30 );
        memcpy(buf, items[selected], size );

        buf[size + 1] = '\0';
    }
    else
        ControlExampleOutput("Selected: (none)");

    selected = -1;
    EndControlExample();
}

static void Page_Item_RichEditBox()
{
    PageHeader("RichEditBox", "Multi-line text input.");
    BeginControlExample("Multi-line");
    static char buf[1024] = "Type multiple lines here...";
    RichEditBox("##rb", buf, sizeof(buf), ImVec2(380.f, 120.f));
    EndControlExample();
}

// ---- Collections -----------------------------------------------------------

static void Page_Item_ComboBox()
{
    PageHeader("ComboBox", "A drop-down list.");
    BeginControlExample("Combo");
    static int s = 0;
    static const char* items[] = { "Red", "Green", "Blue", "Yellow", "Magenta", "Cyan" };
    ImGui::PushItemWidth(280.f);
    ComboBox("##cb", &s, items, IM_ARRAYSIZE(items));
    ImGui::PopItemWidth();
    ControlExampleOutput("Selected: %s", items[s]);
    EndControlExample();

    BeginControlExample("With Header + Description");
    static int  tz_sel = 0;
    static const char* tz_items[] = { "(UTC-08:00) Pacific Time",
                                      "(UTC-05:00) Eastern Time",
                                      "(UTC+00:00) UTC",
                                      "(UTC+01:00) Central Europe",
                                      "(UTC+09:00) Tokyo" };
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Time zone");
    SetNextItemDescription("Used for scheduling and notifications.");
    ComboBox("##cb-tz", &tz_sel, tz_items, IM_ARRAYSIZE(tz_items));
    ImGui::PopItemWidth();
    EndControlExample();
}

static void Page_Item_ListBox()
{
    PageHeader("ListBox", "Pick from a list.");
    BeginControlExample("List");
    static int s = 0;
    static const char* items[] = { "Item 1", "Item 2", "Item 3", "Item 4", "Item 5" };
    ImGui::PushItemWidth(280.f);
    ListBox("##lb", &s, items, IM_ARRAYSIZE(items));
    ImGui::PopItemWidth();
    ControlExampleOutput("Selected: %s", items[s]);
    EndControlExample();
}

static void Page_Item_ListView()
{
    PageHeader("ListView", "Selectable rows with optional icon.");
    BeginControlExample("List");
    static int s = 1;
    static const char* names[]  = { "Inbox", "Sent items", "Drafts", "Junk" };
    static const char* glyphs[] = { ImFluentIcon_Mail, ImFluentIcon_Share, ImFluentIcon_Edit, ImFluentIcon_Delete };
    for (int i = 0; i < 4; ++i)
    {
        if (ListViewItem(names[i], s == i, glyphs[i])) s = i;
    }
    EndControlExample();
}

static void Page_Item_TreeView()
{
    PageHeader("TreeView", "Hierarchical list.");

    BeginControlExample("Files");
    static bool a = true, b = false;
    SetNextItemGlyph(ImFluentIcon_Folder);
    if (TreeNode("Documents", &a))
    {
        SetNextItemGlyph(ImFluentIcon_Folder);
        if (TreeNode("Work", &b))
        {
            SetNextItemGlyph(ImFluentIcon_Document);
            ListViewItem("report.docx", false);
            TreePop();
        }
        SetNextItemGlyph(ImFluentIcon_Document);
        ListViewItem("notes.txt", false);
        TreePop();
    }
    EndControlExample();

    BeginControlExample("Multi-select (checkbox variant)");
    static bool   ms_open[3] = { true, false, false };
    static bool   ms_chk [3] = { false, false, false };
    SetNextItemGlyph(ImFluentIcon_Folder);
    if (TreeNode("Inbox", &ms_open[0], &ms_chk[0]))
    {
        SetNextItemGlyph(ImFluentIcon_Mail);
        TreeNode("Welcome — read me", &ms_open[1], &ms_chk[1]);
        SetNextItemGlyph(ImFluentIcon_Mail);
        TreeNode("Project status update", &ms_open[2], &ms_chk[2]);
        TreePop();
    }
    ControlExampleOutput("Checked: %s%s%s",
        ms_chk[0] ? "Inbox " : "",
        ms_chk[1] ? "Welcome " : "",
        ms_chk[2] ? "Project " : "");
    EndControlExample();
}

static void Page_Item_GridView()
{
    PageHeader("GridView", "A flow grid of selectable tiles.");
    BeginControlExample("Tiles");
    static int s = 0;
    for (int i = 0; i < 8; ++i)
    {
        char buf[16]; std::snprintf(buf, sizeof(buf), "Tile %d", i + 1);
        if (GridViewItem(buf, s == i, ImVec2(140.f, 100.f))) s = i;
        if ((i % 4) != 3) ImGui::SameLine();
    }
    EndControlExample();
}

static void Page_Item_PipsPager()
{
    PageHeader("PipsPager", "A row of dots indicating page position.");
    BeginControlExample("Pips");
    static int s = 2;
    PipsPager("##pp", &s, 7);
    ControlExampleOutput("Page %d / 7", s + 1);
    EndControlExample();
}

static void Page_Item_BreadcrumbBar()
{
    PageHeader("BreadcrumbBar", "Trail of clickable parent items.");
    BeginControlExample("Breadcrumb");
    static const char* items[] = { "Home", "Documents", "Work", "Project Phoenix" };
    int clicked = BreadcrumbBar("##br", items, IM_ARRAYSIZE(items));
    if (clicked >= 0) ControlExampleOutput("Clicked index: %d (%s)", clicked, items[clicked]);
    EndControlExample();
}

// ---- Layout / Containers ---------------------------------------------------

static void Page_Item_Card()
{
    PageHeader("Card", "A grouped surface.");
    BeginControlExample("Filled card");
    if (BeginCard("##card1", ImVec2(380.f, 0.f), ImFluentCardStyle_Filled))
    {
        TextBlock("Card title", ImFluentTextStyle_BodyStrong);
        TextBlock("Description text inside the card surface.", ImFluentTextStyle_Body);
        Button("Action");
    }
    EndCard();
    EndControlExample();
}

static void Page_Item_Expander()
{
    PageHeader("Expander", "A header that expands to reveal a body.");
    BeginControlExample("Settings");
    static bool s_open = false;
    if (BeginExpander("Display options", &s_open))
    {
        static bool s_dark = true;
        Checkbox("Dark theme",  &s_dark);
        static bool s_acrylic = false;
        ToggleSwitch("Acrylic background", &s_acrylic);
        EndExpander();
    }
    EndControlExample();
}

static void Page_Item_TabView()
{
    PageHeader("TabView", "Switch between content areas with tabs.");
    BeginControlExample("Tabs");
    if (BeginTabView("##tv"))
    {
        if (BeginTabItem("Overview")) { TextBlock("Overview content", ImFluentTextStyle_Body); EndTabItem(); }
        if (BeginTabItem("Details"))  { TextBlock("Details content",  ImFluentTextStyle_Body); EndTabItem(); }
        if (BeginTabItem("Settings")) { TextBlock("Settings content", ImFluentTextStyle_Body); EndTabItem(); }
        EndTabView();
    }
    EndControlExample();
}

static void Page_Item_NavigationView()
{
    PageHeader("NavigationView", "Side-pane navigation.");
    BeginControlExample("Side pane");
    static ImFluentNavViewMode mode = ImFluentNavViewMode_LeftCompact;
    static int sel = 0;
    BeginNavigationView("##demoNav", &mode);
    if (NavItem("Home",     sel == 0, ImFluentIcon_Home))       sel = 0;
    if (NavItem("Inbox",    sel == 1, ImFluentIcon_Mail))       sel = 1;
    if (NavItem("Settings", sel == 2, ImFluentIcon_Settings))   sel = 2;
    EndNavigationView();
    ImGui::SameLine();
    ImGui::BeginGroup();
    TextBlock("(toggle the pane with the menu icon)", ImFluentTextStyle_Caption);
    ImGui::EndGroup();
    EndControlExample();
}

static void Page_Item_SelectorBar()
{
    PageHeader("SelectorBar", "Pivot-style horizontal selector.");
    BeginControlExample("Pivot");
    static int s = 0;
    BeginSelectorBar("##sb");
    if (SelectorBarItem("Day",    s == 0)) s = 0;
    if (SelectorBarItem("Week",   s == 1)) s = 1;
    if (SelectorBarItem("Month",  s == 2)) s = 2;
    if (SelectorBarItem("Year",   s == 3)) s = 3;
    EndSelectorBar();
    ControlExampleOutput("Selected: %d", s);
    EndControlExample();
}

// ---- Status & info ---------------------------------------------------------

static void Page_Item_InfoBar()
{
    PageHeader("InfoBar", "Inline notification.");
    BeginControlExample("All severities");
    static bool open[4] = { true, true, true, true };
    InfoBar(ImFluentInfoSeverity_Informational, "Informational", "Update is available.", &open[0]);
    InfoBar(ImFluentInfoSeverity_Success,       "Success",       "Sign-in succeeded.",   &open[1]);
    InfoBar(ImFluentInfoSeverity_Warning,       "Warning",       "Battery is low.",      &open[2]);
    InfoBar(ImFluentInfoSeverity_Critical,      "Error",         "Could not save file.", &open[3]);
    EndControlExample();
}

static void Page_Item_InfoBadge()
{
    PageHeader("InfoBadge", "Compact contextual indicator.");
    BeginControlExample("Variants");
    TextBlock("Dot:", ImFluentTextStyle_Body); ImGui::SameLine(); InfoBadge();
    ImGui::Spacing();
    TextBlock("Count 3:",   ImFluentTextStyle_Body); ImGui::SameLine(); InfoBadge(3);
    ImGui::Spacing();
    TextBlock("Count 99+:", ImFluentTextStyle_Body); ImGui::SameLine(); InfoBadge(101);
    EndControlExample();
}

static void Page_Item_ToolTip()
{
    PageHeader("ToolTip", "Hover-info popup.");
    BeginControlExample("Hover");
    if (Button("Hover me")) {}
    SetItemTooltip("This is a Fluent-styled tooltip.");
    EndControlExample();
}

// ---- Dialogs ---------------------------------------------------------------

static void Page_Item_ContentDialog()
{
    PageHeader("ContentDialog", "Modal dialog.");
    BeginControlExample("Open dialog");
    if (Button("Show dialog")) OpenContentDialog("##cd");
    if (BeginContentDialog("##cd", "Save changes?"))
    {
        TextBlock("Your changes have not been saved. Do you want to save them now?",
                  ImFluentTextStyle_Body);
        const int r = EndContentDialog("Save", "Don't save", "Cancel");
        if (r == 1)      ControlExampleOutput("Result: Save");
        else if (r == 2) ControlExampleOutput("Result: Don't save");
        else if (r == 3) ControlExampleOutput("Result: Cancel");
    }
    EndControlExample();
}

static void Page_Item_Flyout()
{
    PageHeader("Flyout", "Lightweight contextual popup.");
    BeginControlExample("Open flyout");
    if (Button("Show flyout")) OpenFlyout("##fl");
    if (BeginFlyout("##fl"))
    {
        TextBlock("Flyout content",   ImFluentTextStyle_BodyStrong);
        TextBlock("With Fluent body.", ImFluentTextStyle_Body);
        if (Button("Close")) ImGui::CloseCurrentPopup();
        EndFlyout();
    }
    EndControlExample();
}

static void Page_Item_MenuFlyout()
{
    PageHeader("MenuFlyout", "Context menu flyout. Anchored under the trigger item; closes on click-outside.");
    BeginControlExample("Open menu");
    if (Button("Open menu")) OpenMenuFlyout("##mf");
    if (BeginMenuFlyout("##mf"))
    {
        MenuFlyoutItem("Cut",    "Ctrl+X", ImFluentIcon_Cut);
        MenuFlyoutItem("Copy",   "Ctrl+C", ImFluentIcon_Copy);
        MenuFlyoutItem("Paste",  "Ctrl+V", ImFluentIcon_Paste);
        MenuFlyoutSeparator();
        MenuFlyoutItem("Delete", "Del",    ImFluentIcon_Delete);
        EndMenuFlyout();
    }
    EndControlExample();

    BeginControlExample("Glyph via SetNextItemGlyph");
    if (Button("Open share menu")) OpenMenuFlyout("##mf2");
    if (BeginMenuFlyout("##mf2"))
    {
        // SetNextItemGlyph drains exactly one item; equivalent to passing the
        // glyph as the third arg, but reads cleaner when the icon list is
        // computed elsewhere.
        SetNextItemGlyph(ImFluentIcon_Mail);   MenuFlyoutItem("Email a copy");
        SetNextItemGlyph(ImFluentIcon_Print);  MenuFlyoutItem("Print");
        SetNextItemGlyph(ImFluentIcon_Share);  MenuFlyoutItem("Share to…",        "Ctrl+S");
        MenuFlyoutSeparator();
        SetNextItemGlyph(ImFluentIcon_Folder); MenuFlyoutItem("Save to folder…");
        EndMenuFlyout();
    }
    EndControlExample();

    BeginControlExample("Toggle, Radio, and cascading SubItem");
    static bool word_wrap   = true;
    static bool show_ruler  = false;
    static int  zoom_pct    = 100;
    if (Button("Open view menu")) OpenMenuFlyout("##mf3");
    if (BeginMenuFlyout("##mf3"))
    {
        ToggleMenuFlyoutItem("Word wrap",   &word_wrap,  "Alt+Z");
        ToggleMenuFlyoutItem("Show ruler",  &show_ruler);
        MenuFlyoutSeparator();
        if (BeginMenuFlyoutSubItem("Zoom", ImFluentIcon_View))
        {
            RadioMenuFlyoutItem("50%",   &zoom_pct,  50);
            RadioMenuFlyoutItem("75%",   &zoom_pct,  75);
            RadioMenuFlyoutItem("100%",  &zoom_pct, 100);
            RadioMenuFlyoutItem("150%",  &zoom_pct, 150);
            RadioMenuFlyoutItem("200%",  &zoom_pct, 200);
            EndMenuFlyoutSubItem();
        }
        EndMenuFlyout();
    }
    ControlExampleOutput("wrap=%s  ruler=%s  zoom=%d%%",
                         word_wrap ? "on" : "off",
                         show_ruler ? "on" : "off",
                         zoom_pct);
    EndControlExample();
}

// ---- Menus & toolbars ------------------------------------------------------

static void Page_Item_AppBarButton()
{
    PageHeader("AppBarButton", "Toolbar button with icon-above-label.");
    BeginControlExample("Toolbar");
    AppBarButton("Add",    ImFluentIcon_Add);    ImGui::SameLine();
    AppBarButton("Edit",   ImFluentIcon_Edit);   ImGui::SameLine();
    AppBarButton("Delete", ImFluentIcon_Delete); ImGui::SameLine();
    AppBarSeparator();                            ImGui::SameLine();
    AppBarButton("Share",  ImFluentIcon_Share);
    EndControlExample();

    BeginControlExample("AppBarToggleButton");
    static bool bold = true, italic = false, under = false;
    AppBarToggleButton("Bold",      "B", &bold);   ImGui::SameLine();
    AppBarToggleButton("Italic",    "I", &italic); ImGui::SameLine();
    AppBarToggleButton("Underline", "U", &under);
    ControlExampleOutput("B=%d I=%d U=%d", bold ? 1 : 0, italic ? 1 : 0, under ? 1 : 0);
    EndControlExample();
}

// ---- Date & time -----------------------------------------------------------

static void Page_Item_DatePicker()
{
    PageHeader("DatePicker", "Pick a date.");
    BeginControlExample("Date");
    static ImFluentDate d = { 2026, 5, 9 };
    DatePicker("##dp", &d);
    ControlExampleOutput("Date: %04d-%02d-%02d", d.Year, d.Month, d.Day);
    EndControlExample();
}

static void Page_Item_TimePicker()
{
    PageHeader("TimePicker", "Pick a time.");
    BeginControlExample("Time");
    static ImFluentTime t = { 9, 30 };
    TimePicker("##tp", &t);
    ControlExampleOutput("Time: %02d:%02d", t.Hour, t.Minute);
    EndControlExample();
}

static void Page_Item_CalendarDatePicker()
{
    PageHeader("CalendarDatePicker", "Drop-down calendar.");
    BeginControlExample("Calendar");
    static ImFluentDate d = { 2026, 5, 9 };
    ImGui::PushItemWidth(220.f);
    CalendarDatePicker("##cdp", &d);
    ImGui::PopItemWidth();
    ControlExampleOutput("Date: %04d-%02d-%02d", d.Year, d.Month, d.Day);
    EndControlExample();
}

// ---- Layout / Containers (additions) ---------------------------------------

static void Page_Item_SettingsCard()
{
    PageHeader("SettingsCard", "Card row with glyph + header + description on the left and a control on the right. The standard layout for settings pages.");

    BeginControlExample("Toggle");
    static bool wifi = true;
    if (BeginSettingsCard("##sc-wifi", "Wi-Fi", "Connect to wireless networks.", ImFluentIcon_GlobalNavButton))
    {
        ToggleSwitch("##wifi", &wifi);
        EndSettingsCard();
    }
    EndControlExample();

    BeginControlExample("ComboBox");
    static int theme = 0;
    if (BeginSettingsCard("##sc-theme", "App theme", "Choose Light, Dark, or High Contrast.", ImFluentIcon_Color))
    {
        static const char* themes[] = { "Light", "Dark", "High Contrast" };
        ComboBox("##theme", &theme, themes, IM_ARRAYSIZE(themes));
        EndSettingsCard();
    }
    EndControlExample();

    BeginControlExample("Button (action card)");
    static int clear_count = 0;
    if (BeginSettingsCard("##sc-clear", "Clear cache", "Free disk space used by cached assets.", ImFluentIcon_Delete))
    {
        if (Button("Clear")) clear_count++;
        EndSettingsCard();
    }
    ControlExampleOutput("Clicks: %d", clear_count);
    EndControlExample();
}

static void Page_Item_StackPanel()
{
    PageHeader("StackPanel", "Linear container that stacks children with a uniform spacing. Use horizontal for toolbars/inline groups, vertical for forms.");

    BeginControlExample("Horizontal");
    BeginStackPanelHorizontal();
    Button("Save");
    Button("Discard");
    AccentButton("Continue");
    EndStackPanel();
    EndControlExample();

    BeginControlExample("Vertical (16 dpx spacing)");
    BeginStackPanelVertical(FluentDpx(16.f));
    static bool a = true, b = false, c = true;
    Checkbox("Enable telemetry", &a);
    Checkbox("Send crash reports", &b);
    Checkbox("Receive newsletters", &c);
    EndStackPanel();
    EndControlExample();
}

static void Page_Item_WrapPanel()
{
    PageHeader("WrapPanel", "Lays children out left-to-right and wraps onto a new row when the content region is exhausted.");

    BeginControlExample("Pill row");
    static const char* tags[] = {
        "Productivity", "Design", "Networking", "Storage", "Security",
        "Accessibility", "Performance", "Privacy", "Sync", "Themes"
    };
    BeginWrapPanel();
    for (int i = 0; i < IM_ARRAYSIZE(tags); ++i)
    {
        const float item_w = ImGui::CalcTextSize(tags[i]).x + FluentDpx(24.f);
        WrapPanelNextItem(item_w);
        Button(tags[i], ImVec2(item_w, 0));
    }
    EndWrapPanel();
    EndControlExample();
}

// ---- Dialogs & Flyouts (additions) -----------------------------------------

static void Page_Item_TeachingTip()
{
    PageHeader("TeachingTip", "Anchored callout used to teach the user about a feature. Anchored to the trigger; placement chooses which edge to attach to.");

    auto tip_block = [](const char* label, const char* id, ImFluentTeachingTipPlacement placement, const char* hint)
    {
        if (Button(label)) OpenTeachingTip(id);
        if (BeginTeachingTip(id, "Pro tip", placement))
        {
            TextBlockColored(hint, GetColorU32(ImFluentCol_TextSecondary), ImFluentTextStyle_Body);
            ImGui::Dummy(ImVec2(0, FluentDpx(8.f)));
            if (Button("Got it")) ImGui::CloseCurrentPopup();
            EndTeachingTip();
        }
    };

    BeginControlExample("All four placements");
    tip_block("Top",    "##tt-top",    ImFluentTeachingTipPlacement_Top,    "I appear above the trigger.");
    ImGui::SameLine();
    tip_block("Bottom", "##tt-bot",    ImFluentTeachingTipPlacement_Bottom, "I appear below the trigger.");
    ImGui::SameLine();
    tip_block("Left",   "##tt-left",   ImFluentTeachingTipPlacement_Left,   "I appear to the left of the trigger.");
    ImGui::SameLine();
    tip_block("Right",  "##tt-right",  ImFluentTeachingTipPlacement_Right,  "I appear to the right of the trigger.");
    EndControlExample();
}

// ---- Chrome (TitleBar) -----------------------------------------------------

static void Page_Item_TitleBar()
{
    PageHeader("TitleBar", "Custom title-bar shell that hosts navigation chevrons, search, and inline actions. Drop into the top of your application window.");

    BeginControlExample("Chevrons + title + search");
    if (BeginTitleBar("ImFluent App"))
    {
        AppBarButton("Back",    ImFluentIcon_BackArrow); ImGui::SameLine();
        AppBarButton("Forward", ImFluentIcon_Forward);   ImGui::SameLine();
        AppBarSeparator();                               ImGui::SameLine();
        static char q[64] = "";
        ImGui::PushItemWidth(FluentDpx(220.f));
        TextBox("##search", q, sizeof(q), "Search");
        ImGui::PopItemWidth();
        EndTitleBar();
    }
    EndControlExample();
}

// ---- Style stack -----------------------------------------------------------

static void Page_Item_StyleStack()
{
    PageHeader("Style stack", "Push/pop Fluent tokens (colors and sizing vars) to scope visual overrides without mutating the global ImFluentStyle.");

    BeginControlExample("PushStyleColor");
    Button("Default button");
    ImGui::SameLine();
    PushStyleColor(ImFluentCol_ControlFillDefault, IM_COL32(255, 80, 90, 64));
    PushStyleColor(ImFluentCol_ControlFillSecondary, IM_COL32(255, 80, 90, 96));
    Button("Custom-tinted");
    PopStyleColor(2);
    EndControlExample();

    BeginControlExample("PushStyleVar");
    Button("Default radius", ImVec2(180, 36));
    ImGui::SameLine();
    PushStyleVar(ImFluentStyleVar_ControlCornerRadius, 18.f);
    Button("Pill button", ImVec2(180, 36));
    PopStyleVar();
    EndControlExample();

    BeginControlExample("BeginDisabled scope");
    static bool disable_ui = true;
    Checkbox("Disable controls below", &disable_ui);
    BeginDisabled(disable_ui);
    static bool b = false;
    Checkbox("I'm disabled", &b);
    static char text[64] = "";
    TextBox("##d-tb", text, sizeof(text), "Disabled");
    EndDisabled();
    EndControlExample();
}

// ============================================================================
// Top-level pages: Home, AllControls, Section_, Settings
// ============================================================================
static void Page_Home()
{
    PageHeader("ImFluent Gallery", "A WinUI 3 Gallery clone built on Dear ImGui + the Fluent 2 design system.");

    TextBlock("Get started", ImFluentTextStyle_Subtitle);
    ImGui::Dummy(ImVec2(0.f, FluentDpx(8.f)));

    struct Tile { const char* page; const char* title; const char* subtitle; const char* glyph; };
    const Tile tiles[] = {
        { "Section_BasicInput",       "Basic input",      "Buttons, switches, sliders",  ImFluentIcon_Add },
        { "Section_Collections",      "Collections",      "Lists, trees, grids",         ImFluentIcon_AllControls },
        { "Section_Layout",           "Layout",           "Cards, stacks, settings",     ImFluentIcon_Folder },
        { "Section_Navigation",       "Navigation",       "Tabs, nav pane, breadcrumbs", ImFluentIcon_GlobalNavButton },
        { "Section_DialogsAndFlyouts","Dialogs & flyouts","Dialogs, menus, tips",        ImFluentIcon_Important },
        { "Section_Text",             "Text",             "Text input variants",         ImFluentIcon_Typography },
        { "Section_StatusAndInfo",    "Status & info",    "Progress, badges, info bars", ImFluentIcon_Info },
        { "Section_Design",           "Design",           "Style stack, theming",        ImFluentIcon_Color },
    };
    for (size_t i = 0; i < sizeof(tiles) / sizeof(tiles[0]); ++i)
    {
        const Tile& t = tiles[i];
        ImGui::PushID((int)i);
        if (GridViewItem(t.title, false, ImVec2(FluentDpx(220.f), FluentDpx(120.f))))
            Navigate(t.page);
        ImGui::PopID();
        if ((i % 3) != 2) ImGui::SameLine();
    }
}

static void Page_AllControls()
{
    PageHeader("All controls", "Every widget shipped with ImFluent.");
    static char filter[64] = {};
    ImGui::PushItemWidth(FluentDpx(280.f));
    TextBox("##filter", filter, sizeof(filter), "Filter controls...");
    ImGui::PopItemWidth();
    ImGui::Dummy(ImVec2(0, FluentDpx(8.f)));

    int idx = 0;
    for (int i = 0; i < g_ControlsCount; ++i)
    {
        const ControlInfo& c = g_Controls[i];
        if (filter[0] && !std::strstr(c.Title, filter)) continue;
        ImGui::PushID(i);
        if (GridViewItem(c.Title, false, ImVec2(FluentDpx(220.f), FluentDpx(96.f))))
        {
            char p[128]; std::snprintf(p, sizeof(p), "Item_%s", c.UniqueId);
            Navigate(p);
        }
        ImGui::PopID();
        if ((idx % 3) != 2) ImGui::SameLine();
        ++idx;
    }
    if (idx == 0) TextBlock("(no matches)", ImFluentTextStyle_Caption);
}

static void Page_Section(const char* groupId)
{
    const GroupInfo* gi = nullptr;
    for (int i = 0; i < g_GroupsCount; ++i)
        if (std::strcmp(g_Groups[i].GroupId, groupId) == 0) { gi = &g_Groups[i]; break; }
    PageHeader(gi ? gi->Title : groupId, "Click a tile to open the demo page.");
    int idx = 0;
    for (int i = 0; i < g_ControlsCount; ++i)
    {
        const ControlInfo& c = g_Controls[i];
        if (std::strcmp(c.GroupId, groupId) != 0) continue;
        ImGui::PushID(i);
        if (GridViewItem(c.Title, false, ImVec2(FluentDpx(220.f), FluentDpx(96.f))))
        {
            char p[128]; std::snprintf(p, sizeof(p), "Item_%s", c.UniqueId);
            Navigate(p);
        }
        ImGui::PopID();
        if ((idx % 3) != 2) ImGui::SameLine();
        ++idx;
    }
}

static void Page_Settings()
{
    PageHeader("Settings", "Theme + about.");
    TextBlock("App theme", ImFluentTextStyle_Subtitle);
    ImGui::Dummy(ImVec2(0, FluentDpx(8.f)));
    ImFluentThemePreset cur = ImFluent::GetThemePreset();
    BeginSelectorBar("##theme");
    if (SelectorBarItem("Light",         cur == ImFluentThemePreset_Light))         ImFluent::SetThemePreset(ImFluentThemePreset_Light);
    if (SelectorBarItem("Dark",          cur == ImFluentThemePreset_Dark))          ImFluent::SetThemePreset(ImFluentThemePreset_Dark);
    if (SelectorBarItem("High contrast", cur == ImFluentThemePreset_HighContrast))  ImFluent::SetThemePreset(ImFluentThemePreset_HighContrast);
    EndSelectorBar();

    ImGui::Dummy(ImVec2(0, FluentDpx(16.f)));
    TextBlock("About", ImFluentTextStyle_Subtitle);
    TextBlock("ImFluent — Dear ImGui port of the Fluent 2 design system.", ImFluentTextStyle_Body);
    char ver[64];
    std::snprintf(ver, sizeof(ver), "DPI scale: %.0f%%", ImGui::GetStyle().FontScaleDpi * 100.f);
    TextBlockColored(ver, ImFluent::GetColorU32(ImFluentCol_TextSecondary), ImFluentTextStyle_Caption);
}

// ============================================================================
// Page router
// ============================================================================
static void RenderCurrentPage()
{
    const char* id = CurrentPageId();
    if (std::strcmp(id, "Home")        == 0) { Page_Home();        return; }
    if (std::strcmp(id, "AllControls") == 0) { Page_AllControls(); return; }
    if (std::strcmp(id, "Settings")    == 0) { Page_Settings();    return; }
    if (std::strncmp(id, "Section_", 8) == 0) { Page_Section(id + 8); return; }
    if (std::strncmp(id, "Item_", 5)    == 0)
    {
        const ControlInfo* c = FindControl(id + 5);
        if (c && c->PageFn) { c->PageFn(); return; }
    }
    PageHeader("Not found", id);
}

// ============================================================================
// Title bar (back/forward/title/search/close)
// ============================================================================
static void DrawTitleBar(bool* p_open)
{
    const ImFluentStyle& style = ImFluent::GetStyle();
    const float h = FluentDpx(40.f);
    const float closeW = p_open ? FluentDpx(40.f) : 0.f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, GetColorU32(ImFluentCol_SolidBgBase));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(FluentDpx(8.f), FluentDpx(4.f)));
    ImGui::BeginChild("##titlebar", ImVec2(0, h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::BeginDisabled(g_State.NavCursor <= 0);
    if (AppBarButton(ImFluentIcon_BackArrow, nullptr, ImVec2(FluentDpx(40.f), FluentDpx(32.f))))
        NavigateBack();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(g_State.NavCursor + 1 >= (int)g_State.NavStack.size());
    if (AppBarButton(ImFluentIcon_Forward, nullptr, ImVec2(FluentDpx(40.f), FluentDpx(32.f))))
        NavigateForward();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(FluentDpx(12.f), 0)); ImGui::SameLine();
    ImFluent::TextBlock(ImFluentIcon_Home "  WinUI 3 Gallery (ImFluent)", ImFluentTextStyle_BodyStrong);

    // Right-aligned: search box + optional close X.
    const float searchW = FluentDpx(280.f);
    const float rightW  = searchW + (closeW > 0.f ? FluentDpx(8.f) + closeW : 0.f);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - rightW + ImGui::GetCursorStartPos().x);
    ImGui::PushItemWidth(searchW);
    if (ImFluent::TextBox("##search", g_State.SearchBuf, sizeof(g_State.SearchBuf), "Search controls..."))
    {
        if (g_State.SearchBuf[0] != 0) Navigate("AllControls");
    }
    ImGui::PopItemWidth();
    if (p_open)
    {
        ImGui::SameLine(0.f, FluentDpx(8.f));
        if (AppBarButton(ImFluentIcon_Cancel, nullptr, ImVec2(closeW, FluentDpx(32.f))))
            *p_open = false;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Navigation pane
// ============================================================================
static void DrawNavigationPane()
{
    BeginNavigationView("##gallery-nav", &g_State.NavMode);
    if (NavItem("Home", std::strcmp(CurrentPageId(), "Home") == 0, ImFluentIcon_Home))
        Navigate("Home");
    if (NavItem("All controls", std::strcmp(CurrentPageId(), "AllControls") == 0, ImFluentIcon_AllControls))
        Navigate("AllControls");

    NavSubHeader("Categories");
    for (int g = 0; g < g_GroupsCount; ++g)
    {
        const GroupInfo& gi = g_Groups[g];
        char pid[64]; std::snprintf(pid, sizeof(pid), "Section_%s", gi.GroupId);
        const bool sel = (std::strcmp(CurrentPageId(), pid) == 0) || (g_State.ExpandedGroup == g);
        if (NavItem(gi.Title, sel, gi.Glyph))
        {
            g_State.ExpandedGroup = (g_State.ExpandedGroup == g ? -1 : g);
            Navigate(pid);
        }
        if (g_State.ExpandedGroup == g && g_State.NavMode == ImFluentNavViewMode_LeftOpen)
        {
            for (int i = 0; i < g_ControlsCount; ++i)
            {
                const ControlInfo& c = g_Controls[i];
                if (std::strcmp(c.GroupId, gi.GroupId) != 0) continue;
                char ipid[128]; std::snprintf(ipid, sizeof(ipid), "Item_%s", c.UniqueId);
                ImGui::Indent(ImFluent::FluentDpx(12.f));
                if (NavItem(c.Title, std::strcmp(CurrentPageId(), ipid) == 0, c.Glyph))
                    Navigate(ipid);
                ImGui::Unindent(ImFluent::FluentDpx(12.f));
            }
        }
    }

    NavSubHeader("App");
    if (NavItem("Settings", std::strcmp(CurrentPageId(), "Settings") == 0, ImFluentIcon_Settings))
        Navigate("Settings");
    EndNavigationView();
}

// ============================================================================
// Shell entry — body of ImFluent::ShowDemoWindow
// ============================================================================
static void ShowDemoWindowImpl(bool* p_open)
{
    if (g_State.NavStack.empty())
        Navigate("Home");

    // Theme hotkeys (debug).
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) ImFluent::SetThemePreset(ImFluentThemePreset_Light);
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) ImFluent::SetThemePreset(ImFluentThemePreset_Dark);
    if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) ImFluent::SetThemePreset(ImFluentThemePreset_HighContrast);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos (vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##imfluent-demo-root", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);

    DrawTitleBar(p_open);
    DrawNavigationPane();
    NavigationViewBeginContent();
    // After a navigation, reset the content child's scroll position so a long
    // page's scroll doesn't carry into a short page (and clip new content out
    // of view).
    if (g_State.ResetScroll)
    {
        ImGui::SetScrollY(0.f);
        g_State.ResetScroll = false;
    }
    RenderCurrentPage();
    NavigationViewEndContent();

    ImGui::End();
}

} // namespace ImFluentGalleryApp

namespace ImFluent
{
void ShowDemoWindow(bool* p_open)
{
    if (p_open && !*p_open) return;
    ImFluentGalleryApp::ShowDemoWindowImpl(p_open);
}
}

#else // IMFLUENT_DISABLE_DEMO_WINDOWS

namespace ImFluent
{
void ShowDemoWindow(bool*) {}
}

#endif // IMFLUENT_DISABLE_DEMO_WINDOWS
