// ImFluent regression tests for the Dear ImGui Test Engine.
// Run them with `ImFluentGallery --run-tests [filter]` or the MCP tool `run_tests`.
//
// - "smoke/..."    drive the gallery (ImFluent::ShowDemoWindow) the way a person does.
// - "keyboard/..." put widgets in their own "Test Window" (GuiFunc) and drive them with keys only,
//                  checking WinUI keyboard behaviour: arrows, Home/End, popups that open from the keyboard,
//                  take focus, close with Escape, give focus back to their owner and open again.

#include "imfluent_automation.h"

#include "imfluent.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "imgui_te_context.h"
#include "imgui_te_engine.h"

static const char * kRoot = "//##imfluent-demo-root";

// Opens a gallery page and waits until it is laid out.
static void GotoPage( ImGuiTestContext * ctx, const char * page )
{
    ImFluent::SetDemoWindowPage( page );
    ctx->Yield( 3 );
    ctx->SetRef( kRoot );
}

// Label of the item that has keyboard focus ("" when nothing has focus).
static const char * FocusLabel( ImGuiTestContext * ctx )
{
    static ImGuiTestItemInfo info;
    const ImGuiID id = ctx->UiContext->NavId;
    if ( id == 0 )
        return "";
    info = ctx->ItemInfo( id, ImGuiTestOpFlags_NoError );
    return info.DebugLabel;
}

static int PopupCount( ImGuiTestContext * ctx )
{
    return ctx->UiContext->OpenPopupStack.Size;
}

static bool NearlyEqual( double a, double b )
{
    return ImAbs( a - b ) < 1e-4;
}

static void BeginTestWindow()
{
    ImGui::SetNextWindowPos( ImVec2( 80.f, 80.f ), ImGuiCond_Appearing );
    ImGui::Begin( "Test Window", NULL, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize );
    ImGui::PushItemWidth( 240.f );
}

static void EndTestWindow()
{
    ImGui::PopItemWidth();
    ImGui::End();
}

// Keyboard-only start: no popup left over, focus ring visible, item references relative to the test window.
static void StartKeyboardTest( ImGuiTestContext * ctx )
{
    ctx->PopupCloseAll();
    ctx->SetRef( "Test Window" );
    ctx->SetInputMode( ImGuiInputSource_Keyboard );
}

struct KeyboardVars
{
    float SliderF    = 0.5f;
    int SliderI      = 50;
    float RangeLo    = 25.f;
    float RangeHi    = 75.f;
    float Rating     = 2.f;
    int Combo        = 0;
    int Zoom         = 100;
    int MenuClicks   = 0;
    int DialogResult = 0;
    double Number    = 5.0;
    bool TreeOpen    = false;
    bool TreeChild   = false;

    ImFluent::ImFluentDate Date = { 2026, 5, 9 };
    ImFluent::ImFluentTime Time = { 14, 30 };

    int SplitPrimary   = 0;
    int SplitFlyouts   = 0;
    bool TreeOpen2     = true;
    bool TreeSub       = false;
    bool TabOpen[ 3 ]  = { true, true, true };
    int TabSelectedIdx = -1;

    int Radio   = 1;
    bool Check  = false;
    bool Switch = false;
    int List    = 1;
    int Sel     = 0;
    int Pip     = 2;
    int Crumb   = -1;
    int TabSel  = 0;
};

static bool DateIs( const ImFluent::ImFluentDate & d, int y, int m, int day )
{
    return d.Year == y && d.Month == m && d.Day == day;
}

void ImFluentTests_RegisterAll( ImGuiTestEngine * engine )
{
    ImGuiTest * t = NULL;

    // [SECTION] Smoke: the gallery shell

    t           = IM_REGISTER_TEST( engine, "smoke", "button_click" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        GotoPage( ctx, "Item_Button" );
        ctx->ItemClick( "**/Standard button" );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Standard button" );
    };

    // Tab walks from the navigation pane into the page: layout containers share one navigation scope.
    t           = IM_REGISTER_TEST( engine, "smoke", "tab_from_nav_pane_into_page" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        GotoPage( ctx, "Item_Button" );
        ctx->SetInputMode( ImGuiInputSource_Keyboard );
        ctx->NavMoveTo( "**/Settings" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Standard button" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Settings" );
    };

    t           = IM_REGISTER_TEST( engine, "smoke", "nav_pane_toggle_reachable" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        GotoPage( ctx, "Item_Button" );
        ctx->SetInputMode( ImGuiInputSource_Keyboard );
        ctx->NavMoveTo( "**/Home" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Toggle pane" );
    };

    // [SECTION] Keyboard: value controls

    t = IM_REGISTER_TEST( engine, "keyboard", "slider_arrows_home_end_page" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::Slider( "slider", &v.SliderF, 0.f, 1.f );
        ImFluent::SliderInt( "islider", &v.SliderI, 0, 100 );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "slider" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK( NearlyEqual( v.SliderF, 0.51 ) );
        ctx->KeyPress( ImGuiKey_LeftArrow, 2 );
        IM_CHECK( NearlyEqual( v.SliderF, 0.49 ) );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK( NearlyEqual( v.SliderF, 1.0 ) );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK( NearlyEqual( v.SliderF, 0.0 ) );
        ctx->KeyPress( ImGuiKey_PageUp );
        IM_CHECK( NearlyEqual( v.SliderF, 0.1 ) );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "slider" ); // Arrows change the value, they do not move focus

        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "islider" );
        ctx->KeyPress( ImGuiKey_UpArrow );
        IM_CHECK_EQ( v.SliderI, 51 );
        ctx->KeyPress( ImGuiKey_PageDown );
        IM_CHECK_EQ( v.SliderI, 41 );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "range_slider_each_thumb" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::RangeSlider( "range", &v.RangeLo, &v.RangeHi, 0.f, 100.f, "%.0f" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "range" );
        ctx->KeyPress( ImGuiKey_RightArrow ); // 1% of the full 0..100 range, not of the space left to the other thumb
        IM_CHECK( NearlyEqual( v.RangeLo, 26.0 ) );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "range (upper)" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK( NearlyEqual( v.RangeHi, 74.0 ) );
        IM_CHECK( NearlyEqual( v.RangeLo, 26.0 ) );
        ctx->KeyPress( ImGuiKey_Home ); // The upper thumb stops at the lower one
        IM_CHECK( NearlyEqual( v.RangeHi, 26.0 ) );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "rating_arrows_home_end" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::RatingControl( "rating", &v.Rating, 5 );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "rating" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK( NearlyEqual( v.Rating, 3.0 ) );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK( NearlyEqual( v.Rating, 2.0 ) );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK( NearlyEqual( v.Rating, 5.0 ) );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK( NearlyEqual( v.Rating, 0.0 ) );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "number_box_arrows_while_editing" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::NumberBox( "number", &v.Number, 1.0, 10.0, "%.0f" );
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "number" );
        ctx->KeyPress( ImGuiKey_Enter ); // Start editing
        IM_CHECK( ctx->UiContext->ActiveId != 0 );
        ctx->KeyPress( ImGuiKey_UpArrow );
        IM_CHECK( NearlyEqual( v.Number, 6.0 ) );
        ctx->KeyPress( ImGuiKey_PageUp );
        IM_CHECK( NearlyEqual( v.Number, 16.0 ) );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK( NearlyEqual( v.Number, 15.0 ) );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK( NearlyEqual( v.Number, 15.0 ) );
        // The spin buttons are not tab stops: Tab goes from the field to the next control.
        ctx->KeyPress( ImGuiKey_Escape );
        ctx->NavMoveTo( "number" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
    };

    // [SECTION] Keyboard: popups open from the keyboard, take focus, close, give focus back, open again

    t = IM_REGISTER_TEST( engine, "keyboard", "combo_box_open_select_reopen" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "Red", "Green", "Blue" };
        BeginTestWindow();
        ImFluent::ComboBox( "combo", &v.Combo, items, IM_ARRAYSIZE( items ) );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "combo" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Red" ); // Focus starts on the selected item
        ctx->KeyPress( ImGuiKey_DownArrow );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( v.Combo, 1 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "combo" );

        ctx->KeyPress( ImGuiKey_Space ); // Open again
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Green" );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "combo" );
        IM_CHECK_EQ( v.Combo, 1 );

        ctx->KeyPress( ImGuiMod_Alt | ImGuiKey_DownArrow );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_F4 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "combo" );
        ctx->KeyPress( ImGuiKey_F4 );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "menu_flyout_submenu_and_reentry" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        if ( ImFluent::Button( "menu" ) )
            ImFluent::OpenMenuFlyout( "##menu" );
        if ( ImFluent::BeginMenuFlyout( "##menu" ) )
        {
            if ( ImFluent::MenuFlyoutItem( "Cut" ) )
                v.MenuClicks++;
            ImFluent::MenuFlyoutItem( "Disabled", NULL, NULL, false, false );
            if ( ImFluent::BeginMenuFlyoutSubItem( "Zoom" ) )
            {
                ImFluent::RadioMenuFlyoutItem( "50%", &v.Zoom, 50 );
                ImFluent::RadioMenuFlyoutItem( "100%", &v.Zoom, 100 );
                ImFluent::EndMenuFlyoutSubItem();
            }
            ImFluent::EndMenuFlyout();
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "menu" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Cut" );
        ctx->KeyPress( ImGuiKey_DownArrow ); // The disabled item is skipped
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zoom" );
        ctx->KeyPress( ImGuiKey_RightArrow ); // Right opens the submenu and moves focus into it
        IM_CHECK_EQ( PopupCount( ctx ), 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "50%" );
        ctx->KeyPress( ImGuiKey_LeftArrow ); // Left closes it and returns to the parent item
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zoom" );
        ctx->KeyPress( ImGuiKey_Enter ); // Enter opens it too
        IM_CHECK_EQ( PopupCount( ctx ), 2 );
        ctx->KeyPress( ImGuiKey_Escape ); // Escape closes one level
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zoom" );
        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_Space ); // Invoke "50%": the whole menu closes
        IM_CHECK_EQ( v.Zoom, 50 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "menu" );

        ctx->KeyPress( ImGuiKey_Space ); // Open again from the keyboard
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Enter ); // Invoke "Cut"
        IM_CHECK_EQ( v.MenuClicks, 1 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "menu" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "content_dialog_escape_and_default_button" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        if ( ImFluent::Button( "show dialog" ) )
            ImFluent::OpenContentDialog( "Test Dialog" );
        if ( ImFluent::BeginContentDialog( "Test Dialog", "Save changes?" ) )
        {
            ImGui::TextUnformatted( "Body" );
            if ( const int r = ImFluent::EndContentDialog( "Save", "Don't save", "Cancel" ) )
                v.DialogResult = r;
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "show dialog" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Save" ); // The default button starts with focus
        ctx->KeyPress( ImGuiKey_Escape );             // Escape = close button
        IM_CHECK_EQ( v.DialogResult, 3 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "show dialog" );

        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Enter ); // Enter presses the focused default button
        IM_CHECK_EQ( v.DialogResult, 1 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
    };

    // [SECTION] Keyboard: hierarchical items

    t = IM_REGISTER_TEST( engine, "keyboard", "tree_node_left_right" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        if ( ImFluent::TreeNode( "node", &v.TreeOpen ) )
        {
            if ( ImFluent::TreeNode( "child", &v.TreeChild ) )
                ImFluent::TreePop();
            ImFluent::TreePop();
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "node" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK( v.TreeOpen );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "child" );
        ctx->KeyPress( ImGuiKey_UpArrow );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK( !v.TreeOpen );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "node" );
    };

    t          = IM_REGISTER_TEST( engine, "keyboard", "nav_item_left_right" );
    t->GuiFunc = []( ImGuiTestContext * )
    {
        BeginTestWindow();
        ImFluent::NavItem( "Home", true );
        if ( ImFluent::BeginNavItem( "Group" ) )
        {
            ImFluent::NavItem( "Child", false );
            ImFluent::EndNavItem();
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "Group" );
        IM_CHECK( !ctx->ItemExists( "Child" ) );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK( ctx->ItemExists( "Child" ) );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Group" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK( !ctx->ItemExists( "Child" ) );
    };

    // [SECTION] Keyboard: date and time

    t = IM_REGISTER_TEST( engine, "keyboard", "date_picker_columns_commit_cancel" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::DatePicker( "date", &v.Date );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "date" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Day" ); // Focus starts on the left-most column
        ctx->KeyPress( ImGuiKey_DownArrow );         // Day 10
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Month" );
        ctx->KeyPress( ImGuiKey_UpArrow ); // April
        ctx->KeyPress( ImGuiKey_RightArrow );
        ctx->KeyPress( ImGuiKey_RightArrow ); // No wrap past the last column
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Year" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Accept" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Dismiss" );
        ctx->KeyPress( ImGuiKey_Tab ); // Tab cycles inside the flyout
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Day" );
        IM_CHECK( DateIs( v.Date, 2026, 5, 9 ) ); // Nothing committed yet
        ctx->KeyPress( ImGuiKey_Enter );          // Enter on a column commits
        IM_CHECK( DateIs( v.Date, 2026, 4, 10 ) );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "date" );

        ctx->KeyPress( ImGuiMod_Alt | ImGuiKey_DownArrow ); // Alt+Down opens
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Home );    // Day 1
        ctx->KeyPress( ImGuiKey_UpArrow ); // Day loops: 30 (April)
        ctx->KeyPress( ImGuiKey_Escape );  // Escape cancels
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK( DateIs( v.Date, 2026, 4, 10 ) );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "date" );

        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_Home );
        ctx->KeyPress( ImGuiKey_UpArrow );
        ctx->KeyPress( ImGuiKey_RightArrow );
        ctx->KeyPress( ImGuiKey_RightArrow );
        ctx->KeyPress( ImGuiKey_End );                    // Year 2100
        ctx->KeyPress( ImGuiKey_DownArrow );              // Year does not loop
        ctx->KeyPress( ImGuiMod_Alt | ImGuiKey_UpArrow ); // Alt+Up commits
        IM_CHECK( DateIs( v.Date, 2100, 4, 30 ) );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "time_picker_columns_and_period" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::TimePicker( "time24", &v.Time );
        ImFluent::TimePicker( "time12", &v.Time, ImFluentTimePickerFlags_Hours12 );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "time24" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Hour" );
        ctx->KeyPress( ImGuiKey_UpArrow ); // 13
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Minute" );
        ctx->KeyPress( ImGuiKey_DownArrow ); // 31
        ctx->KeyPress( ImGuiKey_Space );     // Space on a column commits
        IM_CHECK_EQ( v.Time.Hour, 13 );
        IM_CHECK_EQ( v.Time.Minute, 31 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "time24" );

        ctx->NavMoveTo( "time12" );
        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Period" );
        ctx->KeyPress( ImGuiKey_DownArrow ); // PM stays PM: Period does not loop
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( v.Time.Hour, 13 );
        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        ctx->KeyPress( ImGuiKey_UpArrow ); // AM
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( v.Time.Hour, 1 );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "calendar_view_grid_one_stop" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::CalendarView( "cal", &v.Date );
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "cal/##cal_title" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Previous" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Next" );
        ctx->KeyPress( ImGuiKey_Tab ); // The grid is one stop, on the selected date
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-09" );
        ctx->KeyPress( ImGuiKey_Tab ); // ... and Tab leaves it at once
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-09" );

        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-10" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-17" );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-01" );
        ctx->KeyPress( ImGuiKey_LeftArrow ); // Crosses into April
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-04-30" );
        ctx->KeyPress( ImGuiKey_PageDown );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-30" );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-31" );
        IM_CHECK( DateIs( v.Date, 2026, 5, 9 ) ); // Arrows move focus only
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK( DateIs( v.Date, 2026, 5, 31 ) );

        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_UpArrow ); // Year view
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "May" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Jun" );
        ctx->KeyPress( ImGuiKey_Enter ); // Drill down: the same day, clamped to June
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-06-30" );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_UpArrow );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_UpArrow ); // Decade view
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026" );
        ctx->KeyPress( ImGuiKey_PageUp );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2016" );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Jun" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "calendar_date_picker_open_commit_reopen" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::CalendarDatePicker( "pick", &v.Date );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "pick" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "May 2026" ); // WinUI: focus starts on the header button
        ctx->KeyPress( ImGuiKey_Tab, 3 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-09" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        ctx->KeyPress( ImGuiKey_Space ); // Selecting a day commits and closes
        IM_CHECK( DateIs( v.Date, 2026, 5, 10 ) );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "pick" );
        ctx->KeyPress( ImGuiKey_Space ); // Open again
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK( DateIs( v.Date, 2026, 5, 10 ) );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "pick" );
    };

    // [SECTION] Keyboard: closed ComboBox, SplitButton, TreeView parent/child, TabView accelerators

    t = IM_REGISTER_TEST( engine, "keyboard", "combo_box_closed_arrows_and_type_ahead" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "Apple", "Banana", "Blueberry", "Cherry" };
        BeginTestWindow();
        ImFluent::ComboBox( "fruit", &v.Combo, items, IM_ARRAYSIZE( items ) );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "fruit" );
        ctx->KeyPress( ImGuiKey_DownArrow ); // Closed: Down selects the next item without opening
        IM_CHECK_EQ( v.Combo, 1 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK_EQ( v.Combo, 3 );
        ctx->KeyPress( ImGuiKey_DownArrow ); // Clamped, no wrap
        IM_CHECK_EQ( v.Combo, 3 );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK_EQ( v.Combo, 0 );
        ctx->KeyPress( ImGuiKey_UpArrow );
        IM_CHECK_EQ( v.Combo, 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "fruit" );
        ctx->KeyChars( "bl" ); // Type-ahead: prefix match
        IM_CHECK_EQ( v.Combo, 2 );
        ctx->SleepNoSkip( 1.2f, 0.1f ); // The search string resets after one second
        ctx->KeyChars( "c" );
        IM_CHECK_EQ( v.Combo, 3 );

        ctx->KeyPress( ImGuiKey_Space ); // Open: the list does not wrap
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Cherry" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Cherry" );
        ctx->KeyPress( ImGuiKey_Home );
        ctx->KeyPress( ImGuiKey_UpArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Apple" );
        ctx->KeyPress( ImGuiKey_Tab ); // Tab closes without committing
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_EQ( v.Combo, 3 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "fruit" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "split_button_alt_down_f4" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        bool dropdown = false;
        if ( ImFluent::SplitButton( "send", &dropdown ) )
            v.SplitPrimary++;
        if ( dropdown )
        {
            v.SplitFlyouts++;
            ImFluent::OpenMenuFlyout( "##send-menu" );
        }
        if ( ImFluent::BeginMenuFlyout( "##send-menu" ) )
        {
            ImFluent::MenuFlyoutItem( "Send later" );
            ImFluent::EndMenuFlyout();
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "send" );
        ctx->KeyPress( ImGuiKey_Space ); // Primary action
        IM_CHECK_EQ( v.SplitPrimary, 1 );
        IM_CHECK_EQ( v.SplitFlyouts, 0 );
        ctx->KeyPress( ImGuiMod_Alt | ImGuiKey_DownArrow ); // Flyout
        IM_CHECK_EQ( v.SplitFlyouts, 1 );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Send later" );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "send" );
        ctx->KeyPress( ImGuiKey_F4 ); // F4 opens it again at once
        IM_CHECK_EQ( v.SplitFlyouts, 2 );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( v.SplitPrimary, 1 );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "tree_node_child_and_parent" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        if ( ImFluent::TreeNode( "root", &v.TreeOpen2 ) )
        {
            if ( ImFluent::TreeNode( "sub", &v.TreeSub ) )
                ImFluent::TreePop();
            ImFluent::TreePop();
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "root" );
        ctx->KeyPress( ImGuiKey_RightArrow ); // Expanded: Right moves to the first child
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "sub" );
        IM_CHECK( v.TreeOpen2 );
        ctx->KeyPress( ImGuiKey_LeftArrow ); // Collapsed child: Left moves to the parent
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "root" );
        IM_CHECK( v.TreeOpen2 );
        ctx->KeyPress( ImGuiKey_LeftArrow ); // Expanded parent: Left collapses
        IM_CHECK( !v.TreeOpen2 );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "tab_view_ctrl_tab_ctrl_f4" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * names[] = { "One", "Two", "Three" };
        BeginTestWindow();
        if ( ImFluent::BeginTabView( "tabs" ) )
        {
            for ( int i = 0; i < 3; ++i )
            {
                if ( !v.TabOpen[ i ] )
                    continue;
                if ( ImFluent::BeginTabItem( names[ i ], &v.TabOpen[ i ] ) )
                {
                    v.TabSelectedIdx = i;
                    ImFluent::Button( "content" );
                    ImFluent::EndTabItem();
                }
            }
            ImFluent::EndTabView();
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v                          = ctx->GetVars< KeyboardVars >();
        ImGuiKeyChord backup_next                 = ctx->UiContext->ConfigNavWindowingKeyNext;
        ImGuiKeyChord backup_prev                 = ctx->UiContext->ConfigNavWindowingKeyPrev;
        ctx->UiContext->ConfigNavWindowingKeyNext = 0; // Leave Ctrl+Tab to the tab view, as a Fluent app does
        ctx->UiContext->ConfigNavWindowingKeyPrev = 0;
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "tabs/One/content" );
        IM_CHECK_EQ( v.TabSelectedIdx, 0 );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_Tab );
        IM_CHECK_EQ( v.TabSelectedIdx, 1 );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Tab );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Tab ); // Wraps to the last tab
        IM_CHECK_EQ( v.TabSelectedIdx, 2 );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_F4 ); // Close the selected tab
        IM_CHECK( !v.TabOpen[ 2 ] );
        ctx->UiContext->ConfigNavWindowingKeyNext = backup_next;
        ctx->UiContext->ConfigNavWindowingKeyPrev = backup_prev;
    };

    // [SECTION] Keyboard: one-tab-stop groups (WinUI TabFocusNavigation="Once")

    t = IM_REGISTER_TEST( engine, "keyboard", "radio_buttons_one_stop" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "Red", "Green", "Blue" };
        BeginTestWindow();
        ImFluent::Button( "before" );
        ImFluent::RadioButtons( "colors", &v.Radio, items, 3 );
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab ); // Enters on the selected item
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Green" );
        ctx->KeyPress( ImGuiKey_DownArrow ); // Selection follows focus
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Blue" );
        IM_CHECK_EQ( v.Radio, 2 );
        ctx->KeyPress( ImGuiKey_DownArrow ); // Stops at the end
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Blue" );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_UpArrow ); // Ctrl: focus only
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Green" );
        IM_CHECK_EQ( v.Radio, 2 );
        ctx->KeyPress( ImGuiKey_Enter ); // Enter does not check a radio button
        IM_CHECK_EQ( v.Radio, 2 );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.Radio, 1 );
        ctx->KeyPress( ImGuiKey_Tab ); // One stop: Tab leaves the group
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Green" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "check_box_toggle_switch_space_not_enter" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::Checkbox( "check", &v.Check );
        ImFluent::ToggleSwitch( "switch", &v.Switch );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "check" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK( !v.Check );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK( v.Check );
        ctx->NavMoveTo( "switch" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK( !v.Switch );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK( v.Switch );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "list_box_one_stop_selection_follows" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "Alpha", "Beta", "Gamma", "Delta", "Epsilon", "Zeta" };
        BeginTestWindow();
        ImFluent::Button( "before" );
        ImFluent::ListBox( "list", &v.List, items, 6, 3 );
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Beta" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Gamma" );
        IM_CHECK_EQ( v.List, 2 );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zeta" );
        IM_CHECK_EQ( v.List, 5 );
        ctx->KeyPress( ImGuiKey_DownArrow ); // Stops at the end
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zeta" );
        ctx->KeyPress( ImGuiKey_RightArrow ); // Left/Right do not move focus out of a vertical list
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zeta" );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Alpha" );
        IM_CHECK_EQ( v.List, 0 );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_DownArrow ); // Ctrl: focus only
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Beta" );
        IM_CHECK_EQ( v.List, 0 );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.List, 1 );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Beta" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "selector_bar_arrows_home_end" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "Recent", "Shared", "Favorites" };
        BeginTestWindow();
        ImFluent::Button( "before" );
        if ( ImFluent::BeginSelectorBar( "bar" ) )
        {
            for ( int i = 0; i < 3; ++i )
                if ( ImFluent::SelectorBarItem( items[ i ], v.Sel == i ) )
                    v.Sel = i;
            ImFluent::EndSelectorBar();
        }
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Recent" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Shared" );
        IM_CHECK_EQ( v.Sel, 1 );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Favorites" );
        IM_CHECK_EQ( v.Sel, 2 );
        ctx->KeyPress( ImGuiKey_RightArrow ); // No wrap
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Favorites" );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK_EQ( v.Sel, 0 );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "pips_pager_focus_then_select" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::Button( "before" );
        ImFluent::PipsPager( "pips", &v.Pip, 5 );
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab ); // Enters on the selected pip
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Page 3" );
        ctx->KeyPress( ImGuiKey_RightArrow ); // Focus only
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Page 4" );
        IM_CHECK_EQ( v.Pip, 2 );
        ctx->KeyPress( ImGuiKey_DownArrow ); // Down works like Right
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Page 5" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.Pip, 4 );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "breadcrumb_bar_one_stop_no_wrap" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "Home", "Docs", "Page" };
        BeginTestWindow();
        ImFluent::Button( "before" );
        const int clicked = ImFluent::BreadcrumbBar( "crumbs", items, 3 );
        if ( clicked >= 0 )
            v.Crumb = clicked;
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab ); // Enters on the first item
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Home" );
        ctx->KeyPress( ImGuiKey_LeftArrow ); // No wrap
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Home" );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Page" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Page" );
        ctx->KeyPress( ImGuiKey_Enter ); // The current (last) item is clickable too
        IM_CHECK_EQ( v.Crumb, 2 );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Home" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "tab_view_strip_one_stop" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * names[] = { "One", "Two", "Three" };
        BeginTestWindow();
        ImFluent::Button( "before" );
        if ( ImFluent::BeginTabView( "tabs" ) )
        {
            for ( int i = 0; i < 3; ++i )
            {
                if ( ImFluent::BeginTabItem( names[ i ] ) )
                {
                    v.TabSel = i;
                    ImFluent::Button( "content" );
                    ImFluent::EndTabItem();
                }
            }
            ImFluent::EndTabView();
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab ); // Enters on the selected tab
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "One" );
        ctx->KeyPress( ImGuiKey_Tab ); // One stop: the next stop is the tab content
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "content" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "One" );
        ctx->KeyPress( ImGuiKey_LeftArrow ); // Wraps to the last tab, focus only
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Three" );
        IM_CHECK_EQ( v.TabSel, 0 );
        ctx->KeyPress( ImGuiKey_RightArrow ); // Wraps back
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "One" );
        ctx->KeyPress( ImGuiKey_DownArrow ); // Stays in the strip
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "One" );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Three" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.TabSel, 2 );
    };
}
