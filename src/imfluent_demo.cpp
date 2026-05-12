#include "imfluent.h"

#if !defined(IMFLUENT_DISABLE_DEMO_WINDOWS)

#include "imfluent_icons.h"
#include "imfluent_internal.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>

namespace ImFluentGalleryApp
{

using namespace ImFluent;

// [SECTION] ControlInfo data

struct ControlInfo
{
    const char*  UniqueId;
    const char*  GroupId;
    const char*  Title;
    const char*  Subtitle;
    const char*  Glyph;
    void       (*PageFn)();
};

struct GroupInfo
{
    const char* GroupId;
    const char* Title;
    const char* Glyph;
};

static void Page_Item_Button();
static void Page_Item_HyperlinkButton();
static void Page_Item_DropDownButton();
static void Page_Item_SplitButton();
static void Page_Item_ToggleButton();
static void Page_Item_RepeatButton();
static void Page_Item_CheckBox();
static void Page_Item_RadioButton();
static void Page_Item_ToggleSwitch();
static void Page_Item_RatingControl();
static void Page_Item_Slider();
static void Page_Item_ProgressBar();
static void Page_Item_ProgressRing();
static void Page_Item_TextBox();
static void Page_Item_PasswordBox();
static void Page_Item_NumberBox();
static void Page_Item_AutoSuggestBox();
static void Page_Item_RichEditBox();
static void Page_Item_TextBlock();
static void Page_Item_ComboBox();
static void Page_Item_ListBox();
static void Page_Item_ListView();
static void Page_Item_TreeView();
static void Page_Item_GridView();
static void Page_Item_PipsPager();
static void Page_Item_PagerControl();
static void Page_Item_BreadcrumbBar();
static void Page_Item_Card();
static void Page_Item_Expander();
static void Page_Item_TabView();
static void Page_Item_NavigationView();
static void Page_Item_SelectorBar();
static void Page_Item_InfoBar();
static void Page_Item_InfoBadge();
static void Page_Item_ToolTip();
static void Page_Item_ContentDialog();
static void Page_Item_Flyout();
static void Page_Item_MenuFlyout();
static void Page_Item_AppBarButton();
static void Page_Item_DatePicker();
static void Page_Item_TimePicker();
static void Page_Item_CalendarDatePicker();
static void Page_Item_SettingsCard();
static void Page_Item_StackPanel();
static void Page_Item_SplitView();
static void Page_Item_WrapPanel();
static void Page_Item_TeachingTip();
static void Page_Item_TitleBar();
static void Page_Item_StyleStack();
static void Page_Item_CommandBar();
static void Page_Item_ColorPicker();

static const ControlInfo g_Controls[] = {
    { "Button",          "BasicInput",  "Button",            "A control that responds to user input and raises a Click event.", ImFluentIcon_Add,           &Page_Item_Button },
    { "HyperlinkButton", "BasicInput",  "HyperlinkButton",   "A button that appears as hyperlink text and can navigate to a URI.", ImFluentIcon_Share,         &Page_Item_HyperlinkButton },
    { "DropDownButton",  "BasicInput",  "DropDownButton",    "A button with a chevron that opens a flyout when clicked.",        ImFluentIcon_ChevronDown,   &Page_Item_DropDownButton },
    { "SplitButton",     "BasicInput",  "SplitButton",       "A button with two parts: an action button and a chevron drop-down.", ImFluentIcon_ChevronDown,   &Page_Item_SplitButton },
    { "ToggleButton",    "BasicInput",  "ToggleButton",      "A button that can be on, off, or indeterminate.",                  ImFluentIcon_CheckboxComposite,    &Page_Item_ToggleButton },
    { "RepeatButton",    "BasicInput",  "RepeatButton",      "A button that fires its Click event repeatedly while held.",       ImFluentIcon_Refresh,       &Page_Item_RepeatButton },
    { "CheckBox",        "BasicInput",  "CheckBox",          "A control a user can select or clear.",                            ImFluentIcon_CheckboxComposite,    &Page_Item_CheckBox },
    { "RadioButton",     "BasicInput",  "RadioButton",       "A control that allows a user to select one option from a group.",  ImFluentIcon_RadioBtnOn,    &Page_Item_RadioButton },
    { "ToggleSwitch",    "BasicInput",  "ToggleSwitch",      "A switch that can be toggled between two states.",                 ImFluentIcon_Settings,      &Page_Item_ToggleSwitch },
    { "RatingControl",   "BasicInput",  "RatingControl",     "Lets users rate something on a 5-star scale.",                      ImFluentIcon_FavoriteStarFill,    &Page_Item_RatingControl },
    { "Slider",          "BasicInput",  "Slider",            "A control that lets the user select from a range of values.",      ImFluentIcon_AlignCenter,       &Page_Item_Slider },

    { "ProgressBar",     "StatusAndInfo", "ProgressBar",     "Shows the progress of a long-running operation.",                  ImFluentIcon_Refresh,       &Page_Item_ProgressBar },
    { "ProgressRing",    "StatusAndInfo", "ProgressRing",    "Shows the progress of a long-running operation as a ring.",        ImFluentIcon_Refresh,       &Page_Item_ProgressRing },
    { "InfoBar",         "StatusAndInfo", "InfoBar",         "Inline notification for app-wide status messages.",                 ImFluentIcon_Info,          &Page_Item_InfoBar },
    { "InfoBadge",       "StatusAndInfo", "InfoBadge",       "Small contextual indicator for new content or notifications.",     ImFluentIcon_Important,     &Page_Item_InfoBadge },
    { "ToolTip",         "StatusAndInfo", "ToolTip",         "Pops up additional info about an element on hover.",                ImFluentIcon_Info,          &Page_Item_ToolTip },

    { "TextBlock",       "Text",        "TextBlock",         "Displays small amounts of read-only text.",                         ImFluentIcon_Font,    &Page_Item_TextBlock },
    { "TextBox",         "Text",        "TextBox",           "A single-line plain-text input field.",                             ImFluentIcon_Edit,          &Page_Item_TextBox },
    { "PasswordBox",     "Text",        "PasswordBox",       "A control for entering passwords.",                                  ImFluentIcon_Hide,          &Page_Item_PasswordBox },
    { "NumberBox",       "Text",        "NumberBox",         "Numeric input with up/down spin buttons.",                          ImFluentIcon_Add,           &Page_Item_NumberBox },
    { "AutoSuggestBox",  "Text",        "AutoSuggestBox",    "A text-box that gives suggestions as the user types.",              ImFluentIcon_Search,        &Page_Item_AutoSuggestBox },
    { "RichEditBox",     "Text",        "RichEditBox",       "Multi-line text input.",                                            ImFluentIcon_Document,      &Page_Item_RichEditBox },

    { "ComboBox",        "Collections", "ComboBox",          "A drop-down list of items.",                                        ImFluentIcon_ChevronDown,   &Page_Item_ComboBox },
    { "ListBox",         "Collections", "ListBox",           "A control that lets users select from a list.",                     ImFluentIcon_ViewAll,   &Page_Item_ListBox },
    { "ListView",        "Collections", "ListView",          "A vertical list of items with selection.",                          ImFluentIcon_ViewAll,   &Page_Item_ListView },
    { "TreeView",        "Collections", "TreeView",          "A hierarchical list of items.",                                     ImFluentIcon_Folder,        &Page_Item_TreeView },
    { "GridView",        "Collections", "GridView",          "A grid layout of selectable items.",                                ImFluentIcon_ViewAll,   &Page_Item_GridView },
    { "PipsPager",       "Collections", "PipsPager",         "A pager rendered as a row of dots.",                                ImFluentIcon_More,          &Page_Item_PipsPager },
    { "PagerControl",    "Collections", "PagerControl",      "Page navigator with numeric buttons, combo box, or editable number; first/last/prev/next controls.", ImFluentIcon_Page, &Page_Item_PagerControl },

    { "Card",            "Layout",      "Card",              "A surface that groups related content.",                            ImFluentIcon_Folder,        &Page_Item_Card },
    { "SettingsCard",    "Layout",      "SettingsCard",      "Card row used on settings pages: glyph + header + description + control slot.", ImFluentIcon_Settings, &Page_Item_SettingsCard },
    { "Expander",        "Layout",      "Expander",          "A control with a header that expands to reveal a body.",            ImFluentIcon_ChevronDown,   &Page_Item_Expander },
    { "StackPanel",      "Layout",      "StackPanel",        "Linear container with uniform spacing; horizontal or vertical.",    ImFluentIcon_AlignCenter,       &Page_Item_StackPanel },
    { "SplitView",       "Layout",      "SplitView",         "Master/detail layout with a side pane and content area; Inline / CompactInline / Overlay / CompactOverlay modes.", ImFluentIcon_DockLeft, &Page_Item_SplitView },
    { "WrapPanel",       "Layout",      "WrapPanel",         "Lays children left-to-right and wraps to a new row when full.",     ImFluentIcon_AlignCenter,       &Page_Item_WrapPanel },
    { "TitleBar",        "Layout",      "TitleBar",          "Custom title-bar shell hosting nav chevrons, search, and actions.", ImFluentIcon_GlobalNavButton, &Page_Item_TitleBar },

    { "TabView",         "Navigation",  "TabView",           "A control with multiple tabs the user can switch between.",         ImFluentIcon_ViewAll,   &Page_Item_TabView },
    { "NavigationView",  "Navigation",  "NavigationView",    "A side-pane navigation control.",                                   ImFluentIcon_GlobalNavButton, &Page_Item_NavigationView },
    { "SelectorBar",     "Navigation",  "SelectorBar",       "A horizontal list of pill-shaped pivot items.",                     ImFluentIcon_ViewAll,   &Page_Item_SelectorBar },
    { "BreadcrumbBar",   "Navigation",  "BreadcrumbBar",     "A trail of clickable parent items showing the current location.",   ImFluentIcon_ChevronRight,  &Page_Item_BreadcrumbBar },

    { "ContentDialog",   "DialogsAndFlyouts", "ContentDialog","A modal dialog with title, body, and action buttons.",             ImFluentIcon_Important,     &Page_Item_ContentDialog },
    { "Flyout",          "DialogsAndFlyouts", "Flyout",      "A lightweight contextual popup.",                                   ImFluentIcon_More,          &Page_Item_Flyout },
    { "MenuFlyout",      "DialogsAndFlyouts", "MenuFlyout",  "A flyout with a list of menu items.",                               ImFluentIcon_More,          &Page_Item_MenuFlyout },
    { "TeachingTip",     "DialogsAndFlyouts", "TeachingTip", "Anchored callout used to teach a feature; placement = Top/Bottom/Left/Right.", ImFluentIcon_Info, &Page_Item_TeachingTip },

    { "AppBarButton",    "MenusAndToolbars", "AppBarButton", "A toolbar button with an icon glyph above its label.",              ImFluentIcon_Add,           &Page_Item_AppBarButton },
    { "CommandBar",      "MenusAndToolbars", "CommandBar",   "Toolbar with primary commands + overflow flyout for secondary.",   ImFluentIcon_More,          &Page_Item_CommandBar },

    { "DatePicker",         "DateTime", "DatePicker",         "Lets a user pick a date.",                                          ImFluentIcon_Calendar,    &Page_Item_DatePicker },
    { "TimePicker",         "DateTime", "TimePicker",         "Lets a user pick a time.",                                          ImFluentIcon_Clock,       &Page_Item_TimePicker },
    { "CalendarDatePicker", "DateTime", "CalendarDatePicker", "Drop-down calendar for picking a date.",                            ImFluentIcon_Calendar,    &Page_Item_CalendarDatePicker },

    { "StyleStack",         "Design",   "Style stack",        "Push/pop Fluent color and sizing tokens for scoped overrides.",     ImFluentIcon_Color,       &Page_Item_StyleStack },
    { "ColorPicker",        "Design",   "ColorPicker",        "Pick a color via hue/saturation rect, sliders, hex, or palette swatch.", ImFluentIcon_Color, &Page_Item_ColorPicker },
};
static const int g_ControlsCount = (int)(sizeof(g_Controls) / sizeof(g_Controls[0]));

static const GroupInfo g_Groups[] = {
    { "BasicInput",         "Basic input",        ImFluentIcon_Add },
    { "Collections",        "Collections",        ImFluentIcon_ViewAll },
    { "DateTime",           "Date & time",        ImFluentIcon_Calendar },
    { "Design",             "Design",             ImFluentIcon_Color },
    { "DialogsAndFlyouts",  "Dialogs & flyouts",  ImFluentIcon_Important },
    { "Layout",             "Layout",             ImFluentIcon_Folder },
    { "MenusAndToolbars",   "Menus & toolbars",   ImFluentIcon_More },
    { "Navigation",         "Navigation",         ImFluentIcon_GlobalNavButton },
    { "StatusAndInfo",      "Status & info",      ImFluentIcon_Info },
    { "Text",               "Text",               ImFluentIcon_Font },
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

// [SECTION] Navigation state + page router
namespace
{
struct GalleryState
{
    std::vector<std::string> NavStack;
    int                      NavCursor = -1;
    ImFluentNavViewMode      NavMode   = ImFluentNavViewMode_LeftOpen;
    char                     SearchBuf[128] = {};
    int                      ExpandedGroup  = -1;
    bool                     ResetScroll    = false;
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

// [SECTION] PageHeader + ControlExample helpers
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

static bool BeginControlExample(const char* header)
{
    TextBlock(header, ImFluentTextStyle_BodyStrong);
    ImGui::Dummy(ImVec2(0.f, FluentDpx(4.f)));
    return BeginCard(header, ImVec2(0.f, 0.f), ImFluentCardStyle_Filled);
}

static void ControlExampleOptionsHeader()
{
    ImFluent::Separator();
    TextBlock("Options:", ImFluentTextStyle_Caption);
}

static void ControlExampleOutput(const char* fmt, ...)
{
    char buf[512];
    va_list ap; va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ImGui::Dummy(ImVec2(0.f, FluentDpx(8.f)));
    ImFluent::Separator();
    const char* line; const char* line_end;
    ImFormatStringToTempBuffer(&line, &line_end, "Output: %s", buf);
    (void)line_end;
    TextBlock(line, ImFluentTextStyle_Caption);
}

static void EndControlExample()
{
    EndCard();
    ImGui::Dummy(ImVec2(0.f, FluentDpx(16.f)));
}

// [SECTION] Per-control demo pages

// [SECTION] Buttons family

static void Page_Item_Button()
{
    PageHeader("Button", "A control that responds to user input and raises a Click event.");

    if (BeginControlExample("A simple Button"))
    {
    static int s_clicks = 0;
    if (Button("Standard button")) ++s_clicks;
    ControlExampleOutput("Click count: %d", s_clicks);
    }
    EndControlExample();

    if (BeginControlExample("Accent button"))
    {
    static int s_aclicks = 0;
    if (AccentButton("Accent button")) ++s_aclicks;
    ControlExampleOutput("Click count: %d", s_aclicks);
    }
    EndControlExample();

    if (BeginControlExample("States"))
    {
    Button("Rest");           ImGui::SameLine();
    AccentButton("Accent");   ImGui::SameLine();
    ImGui::BeginDisabled();
    Button("Disabled");       ImGui::SameLine();
    AccentButton("Accent disabled");
    ImGui::EndDisabled();
    }
    EndControlExample();
}

static void Page_Item_HyperlinkButton()
{
    PageHeader("HyperlinkButton", "A button that appears as hyperlink text.");
    if (BeginControlExample("A hyperlink button"))
    {
    static int s = 0;
    if (HyperlinkButton("Open the Fluent 2 design system")) ++s;
    ControlExampleOutput("Clicks: %d", s);
    }
    EndControlExample();
}

static void Page_Item_DropDownButton()
{
    PageHeader("DropDownButton", "A button with a chevron that opens a flyout when clicked.");
    if (BeginControlExample("DropDown"))
    {
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
    }
    EndControlExample();
}

static void Page_Item_SplitButton()
{
    PageHeader("SplitButton", "A primary action plus a chevron that opens a flyout. Click the label to invoke the action; click the chevron to open the menu.");

    if (BeginControlExample("Send with options"))
    {
    static const char* send_modes[] = { "Send now", "Send later", "Save as draft", "Discard" };
    static int  picked_mode = 0;
    static int  send_count  = 0;
    bool dd1 = false;
    if (SplitButton(send_modes[picked_mode], &dd1)) send_count++;
    if (dd1) OpenMenuFlyout("##sb-menu1");
    if (BeginMenuFlyout("##sb-menu1"))
    {
        SetNextItemGlyph(ImFluentIcon_Mail);     if (MenuFlyoutItem("Send now"))      picked_mode = 0;
        SetNextItemGlyph(ImFluentIcon_Clock);    if (MenuFlyoutItem("Send later"))    picked_mode = 1;
        SetNextItemGlyph(ImFluentIcon_Save);     if (MenuFlyoutItem("Save as draft")) picked_mode = 2;
        MenuFlyoutSeparator();
        SetNextItemGlyph(ImFluentIcon_Delete);   if (MenuFlyoutItem("Discard"))       picked_mode = 3;
        EndMenuFlyout();
    }
    ControlExampleOutput("Mode = %s   Sends = %d", send_modes[picked_mode], send_count);
    }
    EndControlExample();

    if (BeginControlExample("ToggleSplitButton (paragraph alignment)"))
    {
    static bool   align_on    = true;
    static int    align_pick  = 0;
    static const char* aligns[] = { "Align left", "Align center", "Align right", "Justify" };
    bool dd2 = false;
    if (ToggleSplitButton(aligns[align_pick], &align_on, &dd2)) {}
    if (dd2) OpenMenuFlyout("##sb-menu2");
    if (BeginMenuFlyout("##sb-menu2"))
    {
        for (int i = 0; i < IM_ARRAYSIZE(aligns); ++i)
            if (RadioMenuFlyoutItem(aligns[i], &align_pick, i)) align_on = true;
        EndMenuFlyout();
    }
    ControlExampleOutput("On = %s   Pick = %s", align_on ? "true" : "false", aligns[align_pick]);
    }
    EndControlExample();
}

static void Page_Item_ToggleButton()
{
    PageHeader("ToggleButton", "A button that can be toggled on or off.");
    if (BeginControlExample("Toggle"))
    {
    static bool s = false;
    ToggleButton("Bold", &s);
    ControlExampleOutput("Bold = %s", s ? "On" : "Off");
    }
    EndControlExample();
}

static void Page_Item_RepeatButton()
{
    PageHeader("RepeatButton", "Fires Click repeatedly while held.");
    if (BeginControlExample("Counter"))
    {
    static int s = 0;
    if (RepeatButton("+ Increment")) ++s;
    ControlExampleOutput("Count: %d", s);
    }
    EndControlExample();
}

// [SECTION] Selection

static void Page_Item_CheckBox()
{
    PageHeader("CheckBox", "A control a user can select or clear.");
    if (BeginControlExample("Two-state"))
    {
    static bool s_a = true;
    Checkbox("Sync favorites", &s_a);
    ControlExampleOutput("Sync favorites = %s", s_a ? "true" : "false");
    }
    EndControlExample();

    if (BeginControlExample("Three-state"))
    {
    static int s_t = -1;
    CheckboxTristate("Select all", &s_t);
    ControlExampleOutput("State = %d", s_t);
    }
    EndControlExample();
}

static void Page_Item_RadioButton()
{
    PageHeader("RadioButton", "Choose one of a small set of options.");
    if (BeginControlExample("Group"))
    {
    static int s = 1;
    RadioButton("Option 1", &s, 1);
    RadioButton("Option 2", &s, 2);
    RadioButton("Option 3", &s, 3);
    ControlExampleOutput("Selected = %d", s);
    }
    EndControlExample();

    if (BeginControlExample("RadioButtons group (multi-column)"))
    {
    static int picked = 2;
    static const char* sizes[] = { "Small", "Medium", "Large", "Extra large", "Huge", "Gigantic" };
    RadioButtons("Pick a size", &picked, sizes, IM_ARRAYSIZE(sizes), 3);
    ControlExampleOutput("Picked = %s", sizes[picked]);
    }
    EndControlExample();
}

static void Page_Item_ToggleSwitch()
{
    PageHeader("ToggleSwitch", "Two-state switch.");
    if (BeginControlExample("Wi-Fi"))
    {
    static bool s = true;
    ToggleSwitch("Wi-Fi", &s, "On", "Off");
    ControlExampleOutput("Wi-Fi = %s", s ? "On" : "Off");
    }
    EndControlExample();
}

static void Page_Item_RatingControl()
{
    PageHeader("RatingControl", "Star-based rating input.");
    if (BeginControlExample("Rate"))
    {
    static float r = 3.5f;
    RatingControl("##rate", &r, 5);
    ControlExampleOutput("Rating: %.1f", r);
    }
    EndControlExample();
}

static void Page_Item_Slider()
{
    PageHeader("Slider", "Pick a value from a range.");
    if (BeginControlExample("Float"))
    {
    static float f = 0.5f;
    ImGui::PushItemWidth(280.f);
    Slider("##s1", &f, 0.f, 1.f, "%.2f");
    ImGui::PopItemWidth();
    ControlExampleOutput("Value: %.2f", f);
    }
    EndControlExample();

    if (BeginControlExample("Int"))
    {
    static int i = 50;
    ImGui::PushItemWidth(280.f);
    SliderInt("##s2", &i, 0, 100);
    ImGui::PopItemWidth();
    ControlExampleOutput("Value: %d", i);
    }
    EndControlExample();

    if (BeginControlExample("Header + Description (SetNextItem* pattern)"))
    {
    static float vol = 0.65f;
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Volume");
    SetNextItemDescription("Drag the thumb to adjust system volume.");
    Slider("##s-vol", &vol, 0.f, 1.f, "%.0f%%");
    ImGui::PopItemWidth();
    }
    EndControlExample();

    if (BeginControlExample("RangeSlider (two thumbs)"))
    {
    static float range_lo = 25.f, range_hi = 75.f;
    ImGui::PushItemWidth(280.f);
    RangeSlider("##rs", &range_lo, &range_hi, 0.f, 100.f, "%.0f");
    ImGui::PopItemWidth();
    ControlExampleOutput("Range: [%.0f, %.0f]", range_lo, range_hi);
    }
    EndControlExample();
}

static void Page_Item_ProgressBar()
{
    PageHeader("ProgressBar", "Linear progress indicator. State controls the fill color: Running (accent), Paused (caution), Error (critical).");

    if (BeginControlExample("Determinate"))
    {
    static float p = 0.0f;
    p += ImGui::GetIO().DeltaTime * 0.1f;
    if (p > 1.f) p = 0.f;
    ProgressBar(p, ImVec2(280.f, 0.f), nullptr);
    ControlExampleOutput("Fraction: %.0f%%", p * 100.f);
    }
    EndControlExample();

    if (BeginControlExample("State (Running / Paused / Error)"))
    {
    static int state = 0;
    static const char* state_names[] = { "Running", "Paused", "Error" };
    if (BeginSelectorBar("##pb-state"))
    {
    for (int i = 0; i < 3; ++i)
        if (SelectorBarItem(state_names[i], state == i)) state = i;
        EndSelectorBar();
    }
    ProgressBar(0.6f, ImVec2(280.f, 0.f), nullptr, (ImFluentProgressBarState)state);
    }
    EndControlExample();
}

static void Page_Item_ProgressRing()
{
    PageHeader("ProgressRing", "Circular progress indicator with ease-in-out indeterminate animation.");
    if (BeginControlExample("Indeterminate"))
    {
    ProgressRing(20.f, -1.f);  ImGui::SameLine(); ImGui::Dummy(ImVec2(FluentDpx(16.f), 0)); ImGui::SameLine();
    ProgressRing(32.f, -1.f);  ImGui::SameLine(); ImGui::Dummy(ImVec2(FluentDpx(16.f), 0)); ImGui::SameLine();
    ProgressRing(48.f, -1.f);  ImGui::SameLine(); ImGui::Dummy(ImVec2(FluentDpx(16.f), 0)); ImGui::SameLine();
    ProgressRing(72.f, -1.f);
    }
    EndControlExample();
    if (BeginControlExample("Determinate"))
    {
    static float p = 0.f;
    p += ImGui::GetIO().DeltaTime * 0.2f;
    if (p > 1.f) p = 0.f;
    ProgressRing(40.f, p);
    ControlExampleOutput("Fraction: %.0f%%", p * 100.f);
    }
    EndControlExample();
}

// [SECTION] Text

static void Page_Item_TextBlock()
{
    PageHeader("TextBlock", "Read-only text in the Fluent type ramp.");
    if (BeginControlExample("Type ramp"))
    {
    TextBlock("Caption — 12 dp",     ImFluentTextStyle_Caption);
    TextBlock("Body — 14 dp",        ImFluentTextStyle_Body);
    TextBlock("BodyStrong — 14 dp",  ImFluentTextStyle_BodyStrong);
    TextBlock("Subtitle — 20 dp",    ImFluentTextStyle_Subtitle);
    TextBlock("Title — 28 dp",       ImFluentTextStyle_Title);
    TextBlock("TitleLarge — 40 dp",  ImFluentTextStyle_TitleLarge);
    }
    EndControlExample();
}

static void Page_Item_TextBox()
{
    PageHeader("TextBox", "Single-line plain text input.");
    if (BeginControlExample("Text input"))
    {
    static char buf[128] = "";
    ImGui::PushItemWidth(280.f);
    TextBox("##tb", buf, sizeof(buf), "Type something...");
    ImGui::PopItemWidth();
    ControlExampleOutput("Value: %s", buf);
    }
    EndControlExample();

    if (BeginControlExample("With Header + Description (SetNextItem* pattern)"))
    {
    static char buf2[128] = "";
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Display name");
    SetNextItemDescription("Shown next to your avatar in conversations.");
    TextBox("##tb-named", buf2, sizeof(buf2), "Enter your name");
    ImGui::PopItemWidth();
    }
    EndControlExample();

    if (BeginControlExample("Validation error (SetNextItemError)"))
    {
    static char buf_email[128] = "";
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Email");
    const bool email_invalid = buf_email[0] && !std::strchr(buf_email, '@');
    if (email_invalid) SetNextItemError("Enter a valid email address (must contain '@').");
    TextBox("##tb-email", buf_email, sizeof(buf_email), "name@example.com");
    ImGui::PopItemWidth();
    }
    EndControlExample();

    if (BeginControlExample("Clear button + max length + counter"))
    {
    static char buf_bio[256] = "";
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Short bio");
    TextBox("##tb-bio", buf_bio, sizeof(buf_bio), "Tell us about yourself", 0,
            ImFluentTextBoxFlags_ClearButton | ImFluentTextBoxFlags_ShowCounter, 64);
    ImGui::PopItemWidth();
    }
    EndControlExample();
}

static void Page_Item_PasswordBox()
{
    PageHeader("PasswordBox", "Hidden text input. Press and hold the eye button on the right to reveal.");
    if (BeginControlExample("Password"))
    {
    static char buf[64] = "";
    ImGui::PushItemWidth(280.f);
    PasswordBox("##pb", buf, sizeof(buf), "Enter password");
    ImGui::PopItemWidth();
    ControlExampleOutput("Length: %d", (int)std::strlen(buf));
    }
    EndControlExample();

    if (BeginControlExample("With Header + Description"))
    {
    static char buf2[64] = "";
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Account password");
    SetNextItemDescription("8+ characters, mix of letters and numbers.");
    PasswordBox("##pb2", buf2, sizeof(buf2), "••••••••");
    ImGui::PopItemWidth();
    }
    EndControlExample();
}

static void Page_Item_NumberBox()
{
    PageHeader("NumberBox", "Numeric input with inline up/down spin buttons. Hold a button for repeat. Ctrl+click steps by step_fast.");
    if (BeginControlExample("Number"))
    {
    static double v = 1.5;
    ImGui::PushItemWidth(280.f);
    NumberBox("##nb", &v, 0.5, 5.0);
    ImGui::PopItemWidth();
    ControlExampleOutput("Value: %.3f", v);
    }
    EndControlExample();

    if (BeginControlExample("With Header + Description"))
    {
    static double qty = 1.0;
    ImGui::PushItemWidth(280.f);
    SetNextItemHeader("Quantity");
    SetNextItemDescription("Step = 1, Ctrl-step = 10.");
    NumberBox("##nb2", &qty, 1.0, 10.0, "%.0f");
    ImGui::PopItemWidth();
    }
    EndControlExample();
}

static void Page_Item_AutoSuggestBox()
{
    PageHeader("AutoSuggestBox", "Text input with a click-to-pick suggestion list. List filters on what you type and stays open while focused.");
    if (BeginControlExample("Suggest"))
    {
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
    }
    EndControlExample();
}

static void Page_Item_RichEditBox()
{
    PageHeader("RichEditBox", "Multi-line text input.");
    if (BeginControlExample("Multi-line"))
    {
    static char buf[1024] = "Type multiple lines here...";
    RichEditBox("##rb", buf, sizeof(buf), ImVec2(380.f, 120.f));
    }
    EndControlExample();
}

// [SECTION] Collections

static void Page_Item_ComboBox()
{
    PageHeader("ComboBox", "A drop-down list.");
    if (BeginControlExample("Combo"))
    {
    static int s = 0;
    static const char* items[] = { "Red", "Green", "Blue", "Yellow", "Magenta", "Cyan" };
    ImGui::PushItemWidth(280.f);
    ComboBox("##cb", &s, items, IM_ARRAYSIZE(items));
    ImGui::PopItemWidth();
    ControlExampleOutput("Selected: %s", items[s]);
    }
    EndControlExample();

    if (BeginControlExample("With Header + Description"))
    {
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
    }
    EndControlExample();

}

static void Page_Item_ListBox()
{
    PageHeader("ListBox", "Pick from a list.");
    if (BeginControlExample("List"))
    {
    static int s = 0;
    static const char* items[] = { "Item 1", "Item 2", "Item 3", "Item 4", "Item 5" };
    ImGui::PushItemWidth(280.f);
    ListBox("##lb", &s, items, IM_ARRAYSIZE(items));
    ImGui::PopItemWidth();
    ControlExampleOutput("Selected: %s", items[s]);
    }
    EndControlExample();
}

static void Page_Item_ListView()
{
    PageHeader("ListView", "Selectable rows with optional icon.");
    if (BeginControlExample("List"))
    {
    static int s = 1;
    static const char* names[]  = { "Inbox", "Sent items", "Drafts", "Junk" };
    static const char* glyphs[] = { ImFluentIcon_Mail, ImFluentIcon_Share, ImFluentIcon_Edit, ImFluentIcon_Delete };
    for (int i = 0; i < 4; ++i)
    {
        if (ListViewItem(names[i], s == i, glyphs[i])) s = i;
    }
    }
    EndControlExample();
}

static void Page_Item_TreeView()
{
    PageHeader("TreeView", "Hierarchical list.");

    if (BeginControlExample("Files"))
    {
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
    }
    EndControlExample();

    if (BeginControlExample("Multi-select (checkbox variant)"))
    {
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
    }
    EndControlExample();
}

static void Page_Item_GridView()
{
    PageHeader("GridView", "A flow grid of selectable tiles.");
    if (BeginControlExample("Tiles"))
    {
    static int s = 0;
    for (int i = 0; i < 8; ++i)
    {
        const char* buf; const char* buf_end;
        ImFormatStringToTempBuffer(&buf, &buf_end, "Tile %d", i + 1);
        (void)buf_end;
        if (GridViewItem(buf, s == i, ImVec2(140.f, 100.f))) s = i;
        if ((i % 4) != 3) ImGui::SameLine();
    }
    }
    EndControlExample();
}

static void Page_Item_PipsPager()
{
    PageHeader("PipsPager", "A row of dots indicating page position.");
    if (BeginControlExample("Pips"))
    {
    static int s = 2;
    PipsPager("##pp", &s, 7);
    ControlExampleOutput("Page %d / 7", s + 1);
    }
    EndControlExample();
}

static void Page_Item_PagerControl()
{
    PageHeader("PagerControl", "Page navigator with three display modes: numeric buttons, combo box, or editable number. Each direction supports Visible / HiddenOnEdge / Hidden.");

    if (BeginControlExample("Button panel"))
    {
    static int page = 0;
    PagerControl("##pc-btn", &page, 12);
    ControlExampleOutput("Page %d / 12", page + 1);
    }
    EndControlExample();

    if (BeginControlExample("Combo box"))
    {
    static int page = 4;
    PagerControl("##pc-combo", &page, 25, ImFluentPagerDisplayMode_ComboBox);
    ControlExampleOutput("Page %d / 25", page + 1);
    }
    EndControlExample();

    if (BeginControlExample("Number box"))
    {
    static int page = 7;
    PagerControl("##pc-num", &page, 100, ImFluentPagerDisplayMode_NumberBox);
    ControlExampleOutput("Page %d / 100", page + 1);
    }
    EndControlExample();

    if (BeginControlExample("First/last buttons"))
    {
    static int page = 2;
    PagerControl("##pc-fl", &page, 20,
                 ImFluentPagerDisplayMode_ButtonPanel,
                 ImFluentPagerButtonVisibility_Visible,
                 ImFluentPagerButtonVisibility_Visible);
    ControlExampleOutput("Page %d / 20", page + 1);
    }
    EndControlExample();

    if (BeginControlExample("Hidden on edge"))
    {
    static int page = 0;
    PagerControl("##pc-hide", &page, 10,
                 ImFluentPagerDisplayMode_ButtonPanel,
                 ImFluentPagerButtonVisibility_HiddenOnEdge,
                 ImFluentPagerButtonVisibility_HiddenOnEdge);
    ControlExampleOutput("Page %d / 10 - first/last/prev/next hide at edges", page + 1);
    }
    EndControlExample();
}

static void Page_Item_BreadcrumbBar()
{
    PageHeader("BreadcrumbBar", "Trail of clickable parent items.");
    if (BeginControlExample("Breadcrumb"))
    {
    static const char* items[] = { "Home", "Documents", "Work", "Project Phoenix" };
    int clicked = BreadcrumbBar("##br", items, IM_ARRAYSIZE(items));
    if (clicked >= 0) ControlExampleOutput("Clicked index: %d (%s)", clicked, items[clicked]);
    }
    EndControlExample();
}

// [SECTION] Layout / Containers

static void Page_Item_Card()
{
    PageHeader("Card", "A grouped surface.");
    if (BeginControlExample("Filled card"))
    {
    if (BeginCard("##card1", ImVec2(380.f, 0.f), ImFluentCardStyle_Filled))
    {
        TextBlock("Card title", ImFluentTextStyle_BodyStrong);
        TextBlock("Description text inside the card surface.", ImFluentTextStyle_Body);
        Button("Action");
    }
    EndCard();
    }
    EndControlExample();
}

static void Page_Item_Expander()
{
    PageHeader("Expander", "A header that expands to reveal a body.");
    if (BeginControlExample("Settings"))
    {
    static bool s_open = false;
    if (BeginExpander("Display options", &s_open))
    {
        static bool s_dark = true;
        Checkbox("Dark theme",  &s_dark);
        static bool s_acrylic = false;
        ToggleSwitch("Acrylic background", &s_acrylic);
        EndExpander();
    }
    }
    EndControlExample();

    if (BeginControlExample("Up direction (body opens above)"))
    {
    static bool s_open = false;
    static bool s_notify_email = true;
    static bool s_notify_push  = false;
    static int  s_priority = 1;
    if (BeginExpander("Notification preferences", &s_open, ImFluentExpandDirection_Up))
    {
        Checkbox("Email notifications", &s_notify_email);
        Checkbox("Push notifications",  &s_notify_push);
        TextBlock("Priority:", ImFluentTextStyle_Body);
        RadioButton("Low",    &s_priority, 0); ImGui::SameLine();
        RadioButton("Normal", &s_priority, 1); ImGui::SameLine();
        RadioButton("High",   &s_priority, 2);
        EndExpander();
    }
    }
    EndControlExample();

    if (BeginControlExample("Expand / collapse events"))
    {
    static bool s_open = false;
    static int  expanded_count  = 0;
    static int  collapsed_count = 0;
    bool just_expanded = false, just_collapsed = false;
    if (BeginExpander("Notifications", &s_open, ImFluentExpandDirection_Down,
                      &just_expanded, &just_collapsed))
    {
        TextBlock("Body content shown when open.", ImFluentTextStyle_Caption);
        EndExpander();
    }
    if (just_expanded)  ++expanded_count;
    if (just_collapsed) ++collapsed_count;
    ControlExampleOutput("expanded: %d   collapsed: %d", expanded_count, collapsed_count);
    }
    EndControlExample();
}

static void Page_Item_TabView()
{
    PageHeader("TabView", "Switch between content areas with tabs.");
    if (BeginControlExample("Tabs"))
    {
    if (BeginTabView("##tv"))
    {
        if (BeginTabItem("Overview")) { TextBlock("Overview content", ImFluentTextStyle_Body); EndTabItem(); }
        if (BeginTabItem("Details"))  { TextBlock("Details content",  ImFluentTextStyle_Body); EndTabItem(); }
        if (BeginTabItem("Settings")) { TextBlock("Settings content", ImFluentTextStyle_Body); EndTabItem(); }
        EndTabView();
    }
    }
    EndControlExample();

    if (BeginControlExample("Closeable tabs + Add button (middle-click also closes)"))
    {
    static struct Tab { char title[32]; bool open; } tabs[8] = {
        {"Document 1", true}, {"Document 2", true}, {"Document 3", true}
    };
    static int next_doc = 4;
    if (BeginTabView("##tv-add"))
    {
        for (int i = 0; i < 8; ++i)
        {
            if (!tabs[i].open || tabs[i].title[0] == 0) continue;
            if (BeginTabItem(tabs[i].title, &tabs[i].open))
            {
                TextBlock(tabs[i].title, ImFluentTextStyle_BodyStrong);
                EndTabItem();
            }
        }
        if (TabAddButton())
        {
            for (int i = 0; i < 8; ++i)
            {
                if (tabs[i].title[0] == 0 || !tabs[i].open)
                {
                    std::snprintf(tabs[i].title, sizeof(tabs[i].title), "Document %d", next_doc++);
                    tabs[i].open = true;
                    break;
                }
            }
        }
        EndTabView();
    }
    }
    EndControlExample();
}

static void Page_Item_NavigationView()
{
    PageHeader("NavigationView", "Side-pane navigation.");
    const float pane_demo_h = FluentDpx(360.f);
    if (BeginControlExample("Side pane with title, back button, search and settings"))
    {
    static ImFluentNavViewMode mode = ImFluentNavViewMode_LeftCompact;
    static int sel = 0;
    static int back_clicks = 0;
    static char search_buf[64] = "";
    static const char* search_items[] = { "Home", "Inbox", "Folders", "Contacts", "Settings" };
    ImGui::BeginChild("##nav-host-1", ImVec2(0, pane_demo_h), ImGuiChildFlags_None);
    BeginNavigationView("##demoNav", &mode);
    NavPaneTitle("My App");
    NavPaneAutoSuggestBox("##nav-search", search_buf, sizeof(search_buf),
                          search_items, IM_ARRAYSIZE(search_items), NULL, "Search");
    if (NavBackButton(sel != 0, true)) { back_clicks++; sel = 0; }
    if (NavItem("Home",     sel == 0, ImFluentIcon_Home)) sel = 0;
    if (NavItem("Inbox",    sel == 1, ImFluentIcon_Mail)) sel = 1;
    NavSubHeader("More");
    if (NavItem("Folders",  sel == 2, ImFluentIcon_Folder)) sel = 2;
    if (NavItem("Contacts", sel == 3, ImFluentIcon_Contact)) sel = 3;
    if (NavSettingsItem(sel == 4)) sel = 4;
    EndNavigationView();
    ImGui::SameLine();
    ImGui::BeginGroup();
    TextBlock("(toggle the pane with the menu icon)", ImFluentTextStyle_Caption);
    const char* line; const char* line_end;
    ImFormatStringToTempBuffer(&line, &line_end, "Back clicks: %d", back_clicks);
    (void)line_end;
    TextBlock(line, ImFluentTextStyle_Caption);
    ImGui::EndGroup();
    ImGui::EndChild();
    }
    EndControlExample();

    if (BeginControlExample("Top pane mode"))
    {
    static ImFluentNavViewMode mode = ImFluentNavViewMode_Top;
    static int sel = 0;
    BeginNavigationView("##demoNavTop", &mode);
    if (NavItem("Home",     sel == 0, ImFluentIcon_Home))    sel = 0;
    if (NavItem("Inbox",    sel == 1, ImFluentIcon_Mail))    sel = 1;
    if (NavItem("Folders",  sel == 2, ImFluentIcon_Folder))  sel = 2;
    if (NavItem("Contacts", sel == 3, ImFluentIcon_Contact)) sel = 3;
    EndNavigationView();
    }
    EndControlExample();

    if (BeginControlExample("Pane toggle hidden"))
    {
    static ImFluentNavViewMode mode = ImFluentNavViewMode_LeftCompact;
    static int sel = 0;
    ImGui::BeginChild("##nav-host-3", ImVec2(0, pane_demo_h), ImGuiChildFlags_None);
    SetNextNavPaneToggleButtonVisible(false);
    BeginNavigationView("##demoNavNoToggle", &mode);
    if (NavItem("Home",     sel == 0, ImFluentIcon_Home))    sel = 0;
    if (NavItem("Inbox",    sel == 1, ImFluentIcon_Mail))    sel = 1;
    EndNavigationView();
    ImGui::EndChild();
    }
    EndControlExample();
}

static void Page_Item_SelectorBar()
{
    PageHeader("SelectorBar", "Pivot-style horizontal selector.");
    if (BeginControlExample("Pivot"))
    {
    static int s = 0;
    if (BeginSelectorBar("##sb"))
    {
    if (SelectorBarItem("Day",    s == 0)) s = 0;
    if (SelectorBarItem("Week",   s == 1)) s = 1;
    if (SelectorBarItem("Month",  s == 2)) s = 2;
    if (SelectorBarItem("Year",   s == 3)) s = 3;
        EndSelectorBar();
    }
    ControlExampleOutput("Selected: %d", s);
    }
    EndControlExample();
}

// [SECTION] Status & info

static void Page_Item_InfoBar()
{
    PageHeader("InfoBar", "Inline notification.");
    if (BeginControlExample("All severities"))
    {
    static bool open[4] = { true, true, true, true };
    InfoBar(ImFluentInfoSeverity_Informational, "Informational", "Update is available.", &open[0]);
    InfoBar(ImFluentInfoSeverity_Success,       "Success",       "Sign-in succeeded.",   &open[1]);
    InfoBar(ImFluentInfoSeverity_Warning,       "Warning",       "Battery is low.",      &open[2]);
    InfoBar(ImFluentInfoSeverity_Critical,      "Error",         "Could not save file.", &open[3]);
    }
    EndControlExample();

    if (BeginControlExample("Action button + close"))
    {
    static bool open = true;
    static int  install_clicks = 0;
    if (InfoBar(ImFluentInfoSeverity_Informational, "Update available",
                "A new version is ready to install. Restart to apply.",
                &open, NULL, true, "Install"))
        install_clicks++;
    ControlExampleOutput("Install clicks: %d   open: %s", install_clicks, open ? "yes" : "no");
    }
    EndControlExample();

    if (BeginControlExample("Icon hidden"))
    {
    static bool open = true;
    InfoBar(ImFluentInfoSeverity_Success, "Saved", "Changes have been written to disk.",
            &open, NULL, false);
    }
    EndControlExample();
}

static void Page_Item_InfoBadge()
{
    PageHeader("InfoBadge", "Compact contextual indicator.");
    if (BeginControlExample("Variants"))
    {
    TextBlock("Dot:", ImFluentTextStyle_Body); ImGui::SameLine(); InfoBadge();
    ImGui::Spacing();
    TextBlock("Count 3:",   ImFluentTextStyle_Body); ImGui::SameLine(); InfoBadge(3);
    ImGui::Spacing();
    TextBlock("Count 99+:", ImFluentTextStyle_Body); ImGui::SameLine(); InfoBadge(101);
    }
    EndControlExample();
}

static void Page_Item_ToolTip()
{
    PageHeader("ToolTip", "Hover-info popup.");
    if (BeginControlExample("Hover"))
    {
    if (Button("Hover me")) {}
    SetItemTooltip("This is a Fluent-styled tooltip.");
    }
    EndControlExample();
}

// [SECTION] Dialogs

static void Page_Item_ContentDialog()
{
    PageHeader("ContentDialog", "Modal dialog.");
    if (BeginControlExample("Open dialog"))
    {
    if (Button("Show dialog")) OpenContentDialog("##cd");
    if (BeginContentDialog("##cd", "Save changes?"))
    {
        TextBlock("Your changes have not been saved. Do you want to save them now?",
                  ImFluentTextStyle_Body);
        const int r = EndContentDialog("Save", "Don't save", "Cancel",
                                       ImFluentContentDialogButton_Primary);
        if (r == 1)      ControlExampleOutput("Result: Save");
        else if (r == 2) ControlExampleOutput("Result: Don't save");
        else if (r == 3) ControlExampleOutput("Result: Cancel");
    }
    }
    EndControlExample();

    if (BeginControlExample("DefaultButton = Close (destructive close highlighted)"))
    {
    if (Button("Show destructive dialog")) OpenContentDialog("##cd-destructive");
    if (BeginContentDialog("##cd-destructive", "Delete file?"))
    {
        TextBlock("This file will be permanently deleted from your device.",
                  ImFluentTextStyle_Body);
        const int r = EndContentDialog("Delete", NULL, "Keep",
                                       ImFluentContentDialogButton_Close);
        if (r == 1)      ControlExampleOutput("Result: Delete");
        else if (r == 3) ControlExampleOutput("Result: Keep");
    }
    }
    EndControlExample();
}

static void Page_Item_Flyout()
{
    PageHeader("Flyout", "Lightweight contextual popup.");
    if (BeginControlExample("Open flyout"))
    {
    if (Button("Show flyout")) OpenFlyout("##fl");
    if (BeginFlyout("##fl"))
    {
        TextBlock("Flyout content",   ImFluentTextStyle_BodyStrong);
        TextBlock("With Fluent body.", ImFluentTextStyle_Body);
        if (Button("Close")) ImGui::CloseCurrentPopup();
        EndFlyout();
    }
    }
    EndControlExample();
}

static void Page_Item_MenuFlyout()
{
    PageHeader("MenuFlyout", "Context menu flyout. Anchored under the trigger item; closes on click-outside.");
    if (BeginControlExample("Open menu"))
    {
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
    }
    EndControlExample();

    if (BeginControlExample("Glyph via SetNextItemGlyph"))
    {
    if (Button("Open share menu")) OpenMenuFlyout("##mf2");
    if (BeginMenuFlyout("##mf2"))
    {
        SetNextItemGlyph(ImFluentIcon_Mail);   MenuFlyoutItem("Email a copy");
        SetNextItemGlyph(ImFluentIcon_Print);  MenuFlyoutItem("Print");
        SetNextItemGlyph(ImFluentIcon_Share);  MenuFlyoutItem("Share to…",        "Ctrl+S");
        MenuFlyoutSeparator();
        SetNextItemGlyph(ImFluentIcon_Folder); MenuFlyoutItem("Save to folder…");
        EndMenuFlyout();
    }
    }
    EndControlExample();

    if (BeginControlExample("Toggle, Radio, and cascading SubItem"))
    {
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
    }
    EndControlExample();
}

// [SECTION] Menus & toolbars

static void Page_Item_AppBarButton()
{
    PageHeader("AppBarButton", "Toolbar button with icon-above-label.");
    if (BeginControlExample("Toolbar"))
    {
    AppBarButton("Add",    ImFluentIcon_Add);    ImGui::SameLine();
    AppBarButton("Edit",   ImFluentIcon_Edit);   ImGui::SameLine();
    AppBarButton("Delete", ImFluentIcon_Delete); ImGui::SameLine();
    AppBarSeparator();                            ImGui::SameLine();
    AppBarButton("Share",  ImFluentIcon_Share);
    }
    EndControlExample();

    if (BeginControlExample("AppBarToggleButton"))
    {
    static bool bold = true, italic = false, under = false;
    AppBarToggleButton("Bold",      "B", &bold);   ImGui::SameLine();
    AppBarToggleButton("Italic",    "I", &italic); ImGui::SameLine();
    AppBarToggleButton("Underline", "U", &under);
    ControlExampleOutput("B=%d I=%d U=%d", bold ? 1 : 0, italic ? 1 : 0, under ? 1 : 0);
    }
    EndControlExample();

    if (BeginControlExample("LabelPosition: Bottom (default), Right, Collapsed"))
    {
    SetNextAppBarLabelPosition(ImFluentAppBarLabelPosition_Bottom);
    AppBarButton("Add",     ImFluentIcon_Add);     ImGui::SameLine();
    SetNextAppBarLabelPosition(ImFluentAppBarLabelPosition_Right);
    AppBarButton("Refresh", ImFluentIcon_Refresh); ImGui::SameLine();
    SetNextAppBarLabelPosition(ImFluentAppBarLabelPosition_Collapsed);
    AppBarButton("Share",   ImFluentIcon_Share);
    }
    EndControlExample();
}

// [SECTION] Date & time

static void Page_Item_DatePicker()
{
    PageHeader("DatePicker", "Pick a date.");
    if (BeginControlExample("Date"))
    {
    static ImFluentDate d = { 2026, 5, 9 };
    DatePicker("##dp", &d);
    ControlExampleOutput("Date: %04d-%02d-%02d", d.Year, d.Month, d.Day);
    }
    EndControlExample();
}

static void Page_Item_TimePicker()
{
    PageHeader("TimePicker", "Pick a time.");
    if (BeginControlExample("24-hour"))
    {
    static ImFluentTime t = { 9, 30 };
    TimePicker("##tp24", &t);
    ControlExampleOutput("Time: %02d:%02d", t.Hour, t.Minute);
    }
    EndControlExample();

    if (BeginControlExample("12-hour with AM/PM"))
    {
    static ImFluentTime t = { 14, 15 };
    TimePicker("##tp12", &t, ImFluentTimePickerFlags_Hours12);
    ControlExampleOutput("Time: %02d:%02d (%s)", t.Hour, t.Minute, t.Hour >= 12 ? "PM" : "AM");
    }
    EndControlExample();

    if (BeginControlExample("15-minute increment"))
    {
    static ImFluentTime t = { 8, 0 };
    TimePicker("##tp15", &t, ImFluentTimePickerFlags_None, 15);
    ControlExampleOutput("Time: %02d:%02d", t.Hour, t.Minute);
    }
    EndControlExample();
}

static void Page_Item_CalendarDatePicker()
{
    PageHeader("CalendarDatePicker", "Drop-down calendar.");
    if (BeginControlExample("Calendar"))
    {
    static ImFluentDate d = { 2026, 5, 9 };
    ImGui::PushItemWidth(220.f);
    CalendarDatePicker("##cdp", &d);
    ImGui::PopItemWidth();
    ControlExampleOutput("Date: %04d-%02d-%02d", d.Year, d.Month, d.Day);
    }
    EndControlExample();
}

// [SECTION] Menus & toolbars (additions)

static void Page_Item_CommandBar()
{
    PageHeader("CommandBar", "Toolbar with primary commands on the left and secondary commands collapsed into an overflow flyout on the right.");

    if (BeginControlExample("Primary + overflow"))
    {
    static int format_clicks = 0;
    if (BeginCommandBar("##cmdbar"))
    {
        if (AppBarButton("Save",   ImFluentIcon_Save))   format_clicks++;
        if (AppBarButton("Copy",   ImFluentIcon_Copy))   format_clicks++;
        if (AppBarButton("Cut",    ImFluentIcon_Cut))    format_clicks++;
        if (AppBarButton("Paste",  ImFluentIcon_Paste))  format_clicks++;
        AppBarSeparator();
        if (AppBarButton("Share",  ImFluentIcon_Share))  format_clicks++;

        if (BeginCommandBarOverflow())
        {
            MenuFlyoutItem("Settings",  NULL, ImFluentIcon_Settings);
            MenuFlyoutItem("Help",      NULL, ImFluentIcon_Info);
            MenuFlyoutSeparator();
            MenuFlyoutItem("About",     NULL, ImFluentIcon_Important);
            EndCommandBarOverflow();
        }
        EndCommandBar();
    }
    ControlExampleOutput("Primary clicks: %d", format_clicks);
    }
    EndControlExample();
}

// [SECTION] Layout / Containers (additions)

static void Page_Item_SettingsCard()
{
    PageHeader("SettingsCard", "Card row with glyph + header + description on the left and a control on the right. The standard layout for settings pages.");

    if (BeginControlExample("Toggle"))
    {
    static bool wifi = true;
    if (BeginSettingsCard("##sc-wifi", "Wi-Fi", "Connect to wireless networks.", ImFluentIcon_GlobalNavButton))
    {
        ToggleSwitch("##wifi", &wifi);
        EndSettingsCard();
    }
    }
    EndControlExample();

    if (BeginControlExample("ComboBox"))
    {
    static int theme = 0;
    if (BeginSettingsCard("##sc-theme", "App theme", "Choose Light, Dark, or High Contrast.", ImFluentIcon_Color))
    {
        static const char* themes[] = { "Light", "Dark", "High Contrast" };
        ComboBox("##theme", &theme, themes, IM_ARRAYSIZE(themes));
        EndSettingsCard();
    }
    }
    EndControlExample();

    if (BeginControlExample("Button (action card)"))
    {
    static int clear_count = 0;
    if (BeginSettingsCard("##sc-clear", "Clear cache", "Free disk space used by cached assets.", ImFluentIcon_Delete))
    {
        if (Button("Clear")) clear_count++;
        EndSettingsCard();
    }
    ControlExampleOutput("Clicks: %d", clear_count);
    }
    EndControlExample();
}

static void Page_Item_StackPanel()
{
    PageHeader("StackPanel", "Linear container that stacks children with a uniform spacing. Use horizontal for toolbars/inline groups, vertical for forms.");

    if (BeginControlExample("Horizontal"))
    {
    BeginStackPanelHorizontal();
    Button("Save");
    Button("Discard");
    AccentButton("Continue");
    EndStackPanel();
    }
    EndControlExample();

    if (BeginControlExample("Vertical (16 dpx spacing)"))
    {
    BeginStackPanelVertical(FluentDpx(16.f));
    static bool a = true, b = false, c = true;
    Checkbox("Enable telemetry", &a);
    Checkbox("Send crash reports", &b);
    Checkbox("Receive newsletters", &c);
    EndStackPanel();
    }
    EndControlExample();
}

static void Page_Item_SplitView()
{
    PageHeader("SplitView", "Master/detail layout with a side pane and content area. Display modes: Inline (pane pushes content), CompactInline (narrow strip always visible, expands inline), Overlay (pane floats over content), CompactOverlay (narrow strip always, expands as overlay).");

    static const char* mode_labels[] = { "Inline", "CompactInline", "Overlay", "CompactOverlay" };
    static const char* place_labels[] = { "Left", "Right" };

    static int mode_pick  = ImFluentSplitViewDisplayMode_CompactInline;
    static int place_pick = ImFluentSplitViewPanePlacement_Left;
    static bool pane_open = true;

    if (BeginControlExample("Configurable"))
    {
    ImGui::PushItemWidth(FluentDpx(200.f));
    ComboBox("Display mode", &mode_pick, mode_labels, IM_ARRAYSIZE(mode_labels));
    ImGui::SameLine();
    ComboBox("Placement", &place_pick, place_labels, IM_ARRAYSIZE(place_labels));
    ImGui::PopItemWidth();
    ToggleSwitch("Pane open", &pane_open);

    ImGui::Dummy(ImVec2(0, FluentDpx(8.f)));
    ImGui::BeginChild("##sv-host", ImVec2(FluentDpx(560.f), FluentDpx(280.f)), ImGuiChildFlags_Borders);
    if (BeginSplitView("##sv1", &pane_open,
                       (ImFluentSplitViewDisplayMode)mode_pick,
                       (ImFluentSplitViewPanePlacement)place_pick))
    {
        if (BeginSplitViewContent())
        {
            TextBlock("Main content", ImFluentTextStyle_Subtitle);
            TextBlock("Toggle the pane via the switch above. Try each display mode to compare push vs overlay behaviors.", ImFluentTextStyle_Body);
            EndSplitViewContent();
        }
        if (BeginSplitViewPane())
        {
            static int sel = 0;
            if (NavItem("Home",     sel == 0, ImFluentIcon_Home))     sel = 0;
            if (NavItem("Inbox",    sel == 1, ImFluentIcon_Mail))     sel = 1;
            if (NavItem("Folders",  sel == 2, ImFluentIcon_Folder))   sel = 2;
            if (NavItem("Contacts", sel == 3, ImFluentIcon_Contact))  sel = 3;
            EndSplitViewPane();
        }
        EndSplitView();
    }
    ImGui::EndChild();
    }
    EndControlExample();
}

static void Page_Item_WrapPanel()
{
    PageHeader("WrapPanel", "Lays children out left-to-right and wraps onto a new row when the content region is exhausted.");

    if (BeginControlExample("Pill row"))
    {
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
    }
    EndControlExample();
}

// [SECTION] Dialogs & Flyouts (additions)

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

    if (BeginControlExample("All four placements"))
    {
    tip_block("Top",    "##tt-top",    ImFluentTeachingTipPlacement_Top,    "I appear above the trigger.");
    ImGui::SameLine();
    tip_block("Bottom", "##tt-bot",    ImFluentTeachingTipPlacement_Bottom, "I appear below the trigger.");
    ImGui::SameLine();
    tip_block("Left",   "##tt-left",   ImFluentTeachingTipPlacement_Left,   "I appear to the left of the trigger.");
    ImGui::SameLine();
    tip_block("Right",  "##tt-right",  ImFluentTeachingTipPlacement_Right,  "I appear to the right of the trigger.");
    }
    EndControlExample();
}

// [SECTION] Chrome (TitleBar)

static void Page_Item_TitleBar()
{
    PageHeader("TitleBar", "Custom title-bar shell that hosts navigation chevrons, search, and inline actions. Drop into the top of your application window.");

    if (BeginControlExample("Chevrons + title + search"))
    {
    if (BeginTitleBar("ImFluent App"))
    {
        TitleBarBackButton();
        TitleBarPaneToggleButton();
        static char q[64] = "";
        ImGui::PushItemWidth(FluentDpx(220.f));
        TextBox("##search", q, sizeof(q), "Search");
        ImGui::PopItemWidth();
        EndTitleBar();
    }
    }
    EndControlExample();

    if (BeginControlExample("Back + pane toggle + icon + subtitle"))
    {
    static int back_clicks = 0;
    static int toggle_clicks = 0;
    if (BeginTitleBar(NULL))
    {
        if (TitleBarBackButton(back_clicks > 0 ? true : false)) --back_clicks;
        if (TitleBarPaneToggleButton())                          ++toggle_clicks;
        TitleBarIcon(ImFluentIcon_Home);
        TitleBarTitle("My App");
        TitleBarSubtitle("v1.0 \xe2\x80\x94 preview");
        EndTitleBar();
    }
    if (Button("Push back-stack item")) ++back_clicks;
    ControlExampleOutput("back stack: %d   pane toggles: %d", back_clicks, toggle_clicks);
    }
    EndControlExample();
}

// [SECTION] Style stack

static void Page_Item_StyleStack()
{
    PageHeader("Style stack", "Push/pop Fluent tokens (colors and sizing vars) to scope visual overrides without mutating the global ImFluentStyle.");

    if (BeginControlExample("PushStyleColor"))
    {
    Button("Default button");
    ImGui::SameLine();
    PushStyleColor(ImFluentCol_ControlFillDefault, IM_COL32(255, 80, 90, 64));
    PushStyleColor(ImFluentCol_ControlFillSecondary, IM_COL32(255, 80, 90, 96));
    Button("Custom-tinted");
    PopStyleColor(2);
    }
    EndControlExample();

    if (BeginControlExample("PushStyleVar"))
    {
    Button("Default radius", ImVec2(180, 36));
    ImGui::SameLine();
    PushStyleVar(ImFluentStyleVar_ControlCornerRadius, 18.f);
    Button("Pill button", ImVec2(180, 36));
    PopStyleVar();
    }
    EndControlExample();

    if (BeginControlExample("BeginDisabled scope"))
    {
    static bool disable_ui = true;
    Checkbox("Disable controls below", &disable_ui);
    BeginDisabled(disable_ui);
    static bool b = false;
    Checkbox("I'm disabled", &b);
    static char text[64] = "";
    TextBox("##d-tb", text, sizeof(text), "Disabled");
    EndDisabled();
    }
    EndControlExample();
}

// [SECTION] ColorPicker

static void Page_Item_ColorPicker()
{
    PageHeader("ColorPicker", "Pick a color via the saturation/value rect, hue slider, alpha slider, and hex/RGB inputs.");

    if (BeginControlExample("Inline picker (RGB)"))
    {
    static float col[4] = { 0.30f, 0.60f, 0.95f, 1.00f };
    ImGui::PushItemWidth(FluentDpx(320.f));
    ColorPicker("##cp-rgb", col, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
    ImGui::PopItemWidth();
    ControlExampleOutput("RGBA: (%.2f, %.2f, %.2f, %.2f)", col[0], col[1], col[2], col[3]);
    }
    EndControlExample();

    if (BeginControlExample("Compact edit + popup"))
    {
    static float c1[4] = { 0.96f, 0.45f, 0.25f, 1.00f };
    static float c2[4] = { 0.45f, 0.85f, 0.60f, 1.00f };
    static float c3[4] = { 0.55f, 0.40f, 0.90f, 1.00f };
    ColorEdit("Primary",   c1);
    ColorEdit("Success",   c2);
    ColorEdit("Highlight", c3);
    }
    EndControlExample();

    if (BeginControlExample("Color swatches"))
    {
    static const ImU32 swatches[] = {
        IM_COL32( 99, 102, 241, 255), IM_COL32( 16, 185, 129, 255), IM_COL32(244, 114, 182, 255),
        IM_COL32(245, 158,  11, 255), IM_COL32(  6, 182, 212, 255), IM_COL32(168,  85, 247, 255),
        IM_COL32(239,  68,  68, 255), IM_COL32( 14, 165, 233, 255),
    };
    static int picked = 0;
    for (int i = 0; i < IM_ARRAYSIZE(swatches); ++i)
    {
        ImGui::PushID(i);
        if (ColorButton("##swatch", ImColor(swatches[i]).Value,
                         ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip,
                         ImVec2(FluentDpx(28.f), FluentDpx(28.f))))
            picked = i;
        ImGui::PopID();
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ControlExampleOutput("Picked swatch #%d", picked);
    }
    EndControlExample();
}

// [SECTION] Top-level pages
static void Page_Home()
{
    PageHeader("ImFluent Gallery", "A WinUI 3 Gallery clone built on Dear ImGui + the Fluent 2 design system.");

    TextBlock("Get started", ImFluentTextStyle_Subtitle);
    ImGui::Dummy(ImVec2(0.f, FluentDpx(8.f)));

    struct Tile { const char* page; const char* title; const char* subtitle; const char* glyph; };
    const Tile tiles[] = {
        { "Section_BasicInput",       "Basic input",      "Buttons, switches, sliders",  ImFluentIcon_Add },
        { "Section_Collections",      "Collections",      "Lists, trees, grids",         ImFluentIcon_ViewAll },
        { "Section_Layout",           "Layout",           "Cards, stacks, settings",     ImFluentIcon_Folder },
        { "Section_Navigation",       "Navigation",       "Tabs, nav pane, breadcrumbs", ImFluentIcon_GlobalNavButton },
        { "Section_DialogsAndFlyouts","Dialogs & flyouts","Dialogs, menus, tips",        ImFluentIcon_Important },
        { "Section_Text",             "Text",             "Text input variants",         ImFluentIcon_Font },
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
    PageHeader("All controls", "Every widget shipped with ImFluent, rendered inline.");
    static char filter[64] = {};
    ImGui::PushItemWidth(FluentDpx(280.f));
    TextBox("##filter", filter, sizeof(filter), "Filter controls...");
    ImGui::PopItemWidth();
    ImGui::Dummy(ImVec2(0, FluentDpx(16.f)));

    int shown = 0;
    for (int i = 0; i < g_ControlsCount; ++i)
    {
        const ControlInfo& c = g_Controls[i];
        if (!c.PageFn) continue;
        if (filter[0] && !std::strstr(c.Title, filter)) continue;
        if (shown > 0)
        {
            ImGui::Dummy(ImVec2(0, FluentDpx(24.f)));
            ImFluent::Separator();
            ImGui::Dummy(ImVec2(0, FluentDpx(16.f)));
        }
        ImGui::PushID(i);
        ImGui::BeginChild(c.UniqueId, ImVec2(-FLT_MIN, 0),
                          ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_NavFlattened,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        c.PageFn();
        ImGui::EndChild();
        ImGui::PopID();
        ++shown;
    }
    if (shown == 0) TextBlock("(no matches)", ImFluentTextStyle_Caption);
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
            const char* p; const char* p_end;
            ImFormatStringToTempBuffer(&p, &p_end, "Item_%s", c.UniqueId);
            (void)p_end;
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
    if (BeginSelectorBar("##theme"))
    {
    if (SelectorBarItem("Light",         cur == ImFluentThemePreset_Light))         ImFluent::SetThemePreset(ImFluentThemePreset_Light);
    if (SelectorBarItem("Dark",          cur == ImFluentThemePreset_Dark))          ImFluent::SetThemePreset(ImFluentThemePreset_Dark);
    if (SelectorBarItem("High contrast", cur == ImFluentThemePreset_HighContrast))  ImFluent::SetThemePreset(ImFluentThemePreset_HighContrast);
        EndSelectorBar();
    }

    ImGui::Dummy(ImVec2(0, FluentDpx(16.f)));
    TextBlock("Accent color", ImFluentTextStyle_Subtitle);
    ImGui::Dummy(ImVec2(0, FluentDpx(8.f)));
    static ImVec4 accent = ImFluent::GetAccentColor().Value;
    if (ImGui::ColorEdit3("##accent", &accent.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
        ImFluent::SetAccentColor(ImColor(accent));
    ImGui::SameLine();
    static const struct { const char* Name; ImU32 Col; } presets[] = {
        { "Default Blue", IM_COL32( 76, 194, 255, 255) },
        { "Teal",         IM_COL32(  0, 183, 195, 255) },
        { "Emerald",      IM_COL32( 16, 185, 129, 255) },
        { "Amber",        IM_COL32(245, 158,  11, 255) },
        { "Pink",         IM_COL32(244, 114, 182, 255) },
        { "Purple",       IM_COL32(168,  85, 247, 255) },
        { "Crimson",      IM_COL32(239,  68,  68, 255) },
    };
    for (int i = 0; i < IM_ARRAYSIZE(presets); ++i)
    {
        ImGui::PushID(i);
        if (ImGui::ColorButton(presets[i].Name, ImColor(presets[i].Col).Value, ImGuiColorEditFlags_NoTooltip, ImVec2(FluentDpx(24.f), FluentDpx(24.f))))
        {
            accent = ImColor(presets[i].Col).Value;
            ImFluent::SetAccentColor(ImColor(presets[i].Col));
        }
        ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::Dummy(ImVec2(0, FluentDpx(16.f)));
    TextBlock("About", ImFluentTextStyle_Subtitle);
    TextBlock("ImFluent — Dear ImGui port of the Fluent 2 design system.", ImFluentTextStyle_Body);
    const char* ver; const char* ver_end;
    ImFormatStringToTempBuffer(&ver, &ver_end, "DPI scale: %.0f%%", ImGui::GetStyle().FontScaleDpi * 100.f);
    (void)ver_end;
    TextBlockColored(ver, ImFluent::GetColorU32(ImFluentCol_TextSecondary), ImFluentTextStyle_Caption);
}

// [SECTION] Page router
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
    if (AppBarButton(ImFluentIcon_Back, nullptr, ImVec2(FluentDpx(40.f), FluentDpx(32.f))))
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

// [SECTION] Navigation pane
static void DrawNavigationPane()
{
    BeginNavigationView("##gallery-nav", &g_State.NavMode);
    if (NavItem("Home", std::strcmp(CurrentPageId(), "Home") == 0, ImFluentIcon_Home))
        Navigate("Home");
    if (NavItem("All controls", std::strcmp(CurrentPageId(), "AllControls") == 0, ImFluentIcon_ViewAll))
        Navigate("AllControls");

    NavSubHeader("Categories");
    for (int g = 0; g < g_GroupsCount; ++g)
    {
        const GroupInfo& gi = g_Groups[g];
        const char* pid; const char* pid_end;
        ImFormatStringToTempBuffer(&pid, &pid_end, "Section_%s", gi.GroupId);
        (void)pid_end;
        const bool sel = (std::strcmp(CurrentPageId(), pid) == 0);
        const bool open = BeginNavItem(gi.Title, sel, gi.Glyph);
        if (ImGui::IsItemActivated()) Navigate(pid);
        if (open)
        {
            for (int i = 0; i < g_ControlsCount; ++i)
            {
                const ControlInfo& c = g_Controls[i];
                if (std::strcmp(c.GroupId, gi.GroupId) != 0) continue;
                const char* ipid; const char* ipid_end;
                ImFormatStringToTempBuffer(&ipid, &ipid_end, "Item_%s", c.UniqueId);
                (void)ipid_end;
                if (NavItem(c.Title, std::strcmp(CurrentPageId(), ipid) == 0, c.Glyph))
                    Navigate(ipid);
            }
            EndNavItem();
        }
    }

    NavSubHeader("App");
    if (NavItem("Settings", std::strcmp(CurrentPageId(), "Settings") == 0, ImFluentIcon_Settings))
        Navigate("Settings");
    EndNavigationView();
}

// [SECTION] Shell entry
static void ShowDemoWindowImpl(bool* p_open)
{
    if (g_State.NavStack.empty())
        Navigate("Home");

    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) ImFluent::SetThemePreset(ImFluentThemePreset_Light);
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) ImFluent::SetThemePreset(ImFluentThemePreset_Dark);
    if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) ImFluent::SetThemePreset(ImFluentThemePreset_HighContrast);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
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
    if (g_State.ResetScroll)
    {
        ImGui::SetScrollY(0.f);
        g_State.ResetScroll = false;
    }
    RenderCurrentPage();
    NavigationViewEndContent();

    ImGui::End();
}

}

namespace ImFluent
{
void ShowDemoWindow(bool* p_open)
{
    if (p_open && !*p_open) return;
    ImFluentGalleryApp::ShowDemoWindowImpl(p_open);
}
}

#else

namespace ImFluent
{
void ShowDemoWindow(bool*) {}
}

#endif
