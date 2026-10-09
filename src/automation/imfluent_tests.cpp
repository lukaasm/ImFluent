// ImFluent regression tests for the Dear ImGui Test Engine: `ImFluentGallery --run-tests [filter]` or the MCP tool `run_tests`.
// "smoke" tests drive the gallery; the others put widgets in their own "Test Window" (GuiFunc).

#include "imfluent_automation.h"

#include "imfluent.h"
#include "imfluent_icons.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "imgui_te_context.h"
#include "imgui_te_engine.h"

static const char * kRoot = "//##imfluent-demo-root";

static void GotoPage( ImGuiTestContext * ctx, const char * page )
{
    ImFluent::SetDemoWindowPage( page );
    ctx->Yield( 3 );
    ctx->SetRef( kRoot );
}

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
    int Combo2       = 0;
    int Page         = 0;
    int BelowClicks  = 0;
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
    int Loose   = 0;

    int ListView     = 1;
    int Grid         = 0;
    bool TreeRoot    = true;
    bool TreeSubOpen = false;
    bool TreeSubChk  = false;

    int MenuCmd  = 0;
    int CbfCmd   = 0;
    int Flip     = 0;
    int PivotSel = 0;

    bool LeakHeader    = false;
    char LeakBox[ 16 ] = "";
    float LeakBoxY     = 0.f;

    char City[ 64 ]     = "";
    char Text[ 64 ]     = "";
    char Password[ 64 ] = "secret";
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
        IM_CHECK( strstr( FocusLabel( ctx ), "Documentation" ) != NULL );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Settings" );
    };

    // The pane toggle sits in the title bar (WinUI Gallery), before the search box, and toggles the pane.
    t           = IM_REGISTER_TEST( engine, "smoke", "title_bar_pane_toggle_reachable" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        GotoPage( ctx, "Item_Button" );
        ctx->SetInputMode( ImGuiInputSource_Keyboard );
        ctx->NavMoveTo( "**/Home" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
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
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "slider" );

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
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK( ctx->UiContext->ActiveId != 0 );
        ctx->KeyPress( ImGuiKey_UpArrow );
        IM_CHECK( NearlyEqual( v.Number, 6.0 ) );
        ctx->KeyPress( ImGuiKey_PageUp );
        IM_CHECK( NearlyEqual( v.Number, 16.0 ) );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK( NearlyEqual( v.Number, 15.0 ) );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK( NearlyEqual( v.Number, 15.0 ) );
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
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Red" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( v.Combo, 1 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "combo" );

        ctx->KeyPress( ImGuiKey_Space );
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

    // GitHub #9: a ComboBox list over another ComboBox takes the click.
    t = IM_REGISTER_TEST( engine, "mouse", "combo_box_list_covers_combo_box" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight" };
        BeginTestWindow();
        ImFluent::ComboBox( "upper", &v.Combo, items, IM_ARRAYSIZE( items ) );
        ImFluent::ComboBox( "lower", &v.Combo2, items, IM_ARRAYSIZE( items ) );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        ctx->PopupCloseAll();
        ctx->SetRef( "Test Window" );
        ctx->SetInputMode( ImGuiInputSource_Mouse );
        static const char * items[] = { "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight" };
        v.Combo                     = -1;
        const ImRect lower          = ctx->ItemInfo( "lower" ).RectFull;
        ctx->ItemClick( "upper" );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        int covering = -1;
        ctx->SetRef( ctx->UiContext->OpenPopupStack[ 0 ].Window );
        for ( int i = 0; i < IM_ARRAYSIZE( items ) && covering < 0; ++i )
        {
            char path[ 64 ];
            ImFormatString( path, sizeof( path ), "**/%s", items[ i ] );
            const ImGuiTestItemInfo info = ctx->ItemInfo( path, ImGuiTestOpFlags_NoError );
            if ( info.ID != 0 && info.RectFull.Contains( lower.GetCenter() ) )
                covering = i;
        }
        ctx->SetRef( "Test Window" );
        IM_CHECK( covering >= 0 );
        ctx->MouseMoveToPos( lower.GetCenter() );
        ctx->MouseClick();
        ctx->Yield( 2 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_EQ( v.Combo, covering );
        IM_CHECK_EQ( v.Combo2, 0 );
    };

    // GitHub #9 with a DatePicker under the list, by mouse and by keyboard.
    t = IM_REGISTER_TEST( engine, "mouse", "combo_box_list_covers_date_picker" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight" };
        BeginTestWindow();
        ImFluent::ComboBox( "upper", &v.Combo, items, IM_ARRAYSIZE( items ) );
        ImFluent::DatePicker( "date", &v.Date );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        ctx->PopupCloseAll();
        ctx->SetRef( "Test Window" );
        ctx->SetInputMode( ImGuiInputSource_Mouse );
        v.Combo                             = -1;
        const ImRect date                   = ctx->ItemInfo( "date" ).RectFull;
        const ImFluent::ImFluentDate before = v.Date;
        ctx->ItemClick( "upper" );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->MouseMoveToPos( ImVec2( date.Min.x + 8.f, date.GetCenter().y ) );
        ctx->MouseClick();
        ctx->Yield( 2 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_GE( v.Combo, 0 );
        IM_CHECK( v.Date.Year == before.Year && v.Date.Month == before.Month && v.Date.Day == before.Day );

        ctx->SetInputMode( ImGuiInputSource_Keyboard );
        ctx->NavMoveTo( "upper" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_DownArrow );
        ctx->KeyPress( ImGuiKey_Enter );
        ctx->Yield( 2 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "upper" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "popup_enter_does_not_reach_control_below" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "One", "Two", "Three", "Four", "Five", "Six" };
        BeginTestWindow();
        ImFluent::ComboBox( "upper", &v.Combo, items, IM_ARRAYSIZE( items ) );
        if ( ImFluent::Button( "below" ) )
            v.BelowClicks++;
        ImFluent::DatePicker( "date", &v.Date );
        if ( ImFluent::Button( "below date" ) )
            v.BelowClicks++;
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "upper" );
        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_DownArrow );
        ctx->KeyPress( ImGuiKey_Enter );
        ctx->Yield( 2 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_EQ( v.BelowClicks, 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "upper" );

        ctx->NavMoveTo( "date" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Enter );
        ctx->Yield( 2 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_EQ( v.BelowClicks, 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "date" );
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
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zoom" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_EQ( PopupCount( ctx ), 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "50%" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zoom" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( PopupCount( ctx ), 2 );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zoom" );
        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.Zoom, 50 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "menu" );

        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Enter );
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
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Save" );
        ctx->KeyPress( ImGuiKey_Escape ); // Escape = close button
        IM_CHECK_EQ( v.DialogResult, 3 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "show dialog" );

        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Enter );
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
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Day" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Month" );
        ctx->KeyPress( ImGuiKey_UpArrow );
        ctx->KeyPress( ImGuiKey_RightArrow );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Year" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Accept" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Dismiss" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Day" );
        IM_CHECK( DateIs( v.Date, 2026, 5, 9 ) );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK( DateIs( v.Date, 2026, 4, 10 ) );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "date" );

        ctx->KeyPress( ImGuiMod_Alt | ImGuiKey_DownArrow );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Home );
        ctx->KeyPress( ImGuiKey_UpArrow ); // Day loops: 30 (April)
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK( DateIs( v.Date, 2026, 4, 10 ) );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "date" );

        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_Home );
        ctx->KeyPress( ImGuiKey_UpArrow );
        ctx->KeyPress( ImGuiKey_RightArrow );
        ctx->KeyPress( ImGuiKey_RightArrow );
        ctx->KeyPress( ImGuiKey_End );
        ctx->KeyPress( ImGuiKey_DownArrow );
        ctx->KeyPress( ImGuiMod_Alt | ImGuiKey_UpArrow );
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
        ctx->KeyPress( ImGuiKey_UpArrow );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Minute" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.Time.Hour, 13 );
        IM_CHECK_EQ( v.Time.Minute, 31 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "time24" );

        ctx->NavMoveTo( "time12" );
        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Period" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( v.Time.Hour, 13 );
        ctx->KeyPress( ImGuiKey_Space );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        ctx->KeyPress( ImGuiKey_UpArrow );
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
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-09" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-09" );

        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-10" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-17" );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-01" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-04-30" );
        ctx->KeyPress( ImGuiKey_PageDown );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-30" );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-31" );
        IM_CHECK( DateIs( v.Date, 2026, 5, 9 ) );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK( DateIs( v.Date, 2026, 5, 31 ) );

        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_UpArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "May" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Jun" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-06-30" );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_UpArrow );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_UpArrow );
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
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "May 2026" );
        ctx->KeyPress( ImGuiKey_Tab, 3 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "2026-05-09" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK( DateIs( v.Date, 2026, 5, 10 ) );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "pick" );
        ctx->KeyPress( ImGuiKey_Space );
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
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_EQ( v.Combo, 1 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK_EQ( v.Combo, 3 );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_EQ( v.Combo, 3 );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK_EQ( v.Combo, 0 );
        ctx->KeyPress( ImGuiKey_UpArrow );
        IM_CHECK_EQ( v.Combo, 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "fruit" );
        ctx->KeyChars( "bl" );
        IM_CHECK_EQ( v.Combo, 2 );
        ctx->SleepNoSkip( 1.2f, 0.1f ); // The search string resets after one second
        ctx->KeyChars( "c" );
        IM_CHECK_EQ( v.Combo, 3 );

        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Cherry" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Cherry" );
        ctx->KeyPress( ImGuiKey_Home );
        ctx->KeyPress( ImGuiKey_UpArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Apple" );
        ctx->KeyPress( ImGuiKey_Tab );
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
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.SplitPrimary, 1 );
        IM_CHECK_EQ( v.SplitFlyouts, 0 );
        ctx->KeyPress( ImGuiMod_Alt | ImGuiKey_DownArrow );
        IM_CHECK_EQ( v.SplitFlyouts, 1 );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Send later" );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "send" );
        ctx->KeyPress( ImGuiKey_F4 );
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
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "sub" );
        IM_CHECK( v.TreeOpen2 );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "root" );
        IM_CHECK( v.TreeOpen2 );
        ctx->KeyPress( ImGuiKey_LeftArrow );
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
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_EQ( v.TabSelectedIdx, 2 );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_F4 );
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
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Green" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Blue" );
        IM_CHECK_EQ( v.Radio, 2 );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Blue" );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_UpArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Green" );
        IM_CHECK_EQ( v.Radio, 2 );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( v.Radio, 2 );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.Radio, 1 );
        ctx->KeyPress( ImGuiKey_Tab );
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
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zeta" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Zeta" );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Alpha" );
        IM_CHECK_EQ( v.List, 0 );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_DownArrow );
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
        ctx->KeyPress( ImGuiKey_RightArrow );
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
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Page 3" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Page 4" );
        IM_CHECK_EQ( v.Pip, 2 );
        ctx->KeyPress( ImGuiKey_DownArrow );
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
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Home" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Home" );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Page" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Page" );
        ctx->KeyPress( ImGuiKey_Enter );
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
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "One" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "content" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "One" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Three" );
        IM_CHECK_EQ( v.TabSel, 0 );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "One" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "One" );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Three" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.TabSel, 2 );
    };

    // Loose radio buttons (one int* = one group): each is a tab stop; arrows move focus inside the group and wrap.
    t = IM_REGISTER_TEST( engine, "keyboard", "loose_radio_buttons_wrap_focus_only" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::RadioButton( "A", &v.Loose, 0 );
        ImFluent::RadioButton( "B", &v.Loose, 1 );
        ImFluent::RadioButton( "C", &v.Loose, 2 );
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "A" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "B" );
        IM_CHECK_EQ( v.Loose, 0 );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "C" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "A" );
        ctx->KeyPress( ImGuiKey_UpArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "C" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( v.Loose, 2 );
        ctx->NavMoveTo( "A" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "B" );
    };

    // WinUI TextBox keeps its text on Escape (ImGui InputText would revert the edit).
    t = IM_REGISTER_TEST( engine, "keyboard", "text_box_escape_keeps_text" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::TextBox( "name", v.Text, sizeof( v.Text ) );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "name" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( ctx->UiContext->ActiveId, ctx->GetID( "name" ) );
        ctx->KeyCharsAppend( "abc" );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_STR_EQ( v.Text, "abc" );
        IM_CHECK_EQ( ctx->UiContext->ActiveId, ( ImGuiID )0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "name" );
    };

    // WinUI PasswordBox: Alt+F8 reveals the password while F8 is held.
    t = IM_REGISTER_TEST( engine, "keyboard", "password_box_alt_f8_peek" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::PasswordBox( "pw", v.Password, sizeof( v.Password ) );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "pw" );
        ImGuiWindow * win = ctx->GetWindowByRef( "//Test Window" );
        IM_CHECK_NO_RET( win != NULL );
        if ( !win )
            return;
        const ImGuiID key = ctx->GetID( "pw" ) ^ 0x9EE6F8u; // Peek state, see ImFluent::PasswordBox
        IM_CHECK( !win->StateStorage.GetBool( key, false ) );
        ctx->KeyDown( ImGuiMod_Alt );
        ctx->KeyDown( ImGuiKey_F8 );
        ctx->Yield( 2 );
        IM_CHECK( win->StateStorage.GetBool( key, false ) );
        ctx->KeyUp( ImGuiMod_Alt );
        ctx->Yield( 2 );
        IM_CHECK( win->StateStorage.GetBool( key, false ) );
        ctx->KeyUp( ImGuiKey_F8 );
        ctx->Yield( 2 );
        IM_CHECK( !win->StateStorage.GetBool( key, false ) );
    };

    // ImFluentActions: an application rebinds a command chord, and the old chord stops working.
    t = IM_REGISTER_TEST( engine, "keyboard", "actions_rebind_toggle_popup" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * items[] = { "Apple", "Banana", "Cherry" };
        BeginTestWindow();
        ImFluent::ComboBox( "fruit", &v.Combo, items, 3 );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        ImFluentActions & actions  = ImFluent::GetActions();
        const ImGuiKeyChord backup = actions.TogglePopup;
        actions.TogglePopup        = ImGuiMod_Ctrl | ImGuiKey_F2;
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "fruit" );
        ctx->KeyPress( ImGuiKey_F4 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_F2 );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_F2 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        actions.TogglePopup = 0;
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_F2 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        actions.TogglePopup = backup;
    };

    // [SECTION] Keyboard: ListView / GridView / TreeView containers (one tab stop each)

    t = IM_REGISTER_TEST( engine, "keyboard", "list_view_one_stop_selection_follows" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * names[] = { "Inbox", "Sent", "Drafts", "Junk" };
        BeginTestWindow();
        ImFluent::Button( "before" );
        ImFluent::BeginListView( "mail" );
        for ( int i = 0; i < 4; ++i )
            if ( ImFluent::ListViewItem( names[ i ], v.ListView == i ) )
                v.ListView = i;
        ImFluent::EndListView();
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Sent" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Drafts" );
        IM_CHECK_EQ( v.ListView, 2 );
        ctx->KeyPress( ImGuiKey_End );
        IM_CHECK_EQ( v.ListView, 3 );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Junk" );
        ctx->KeyPress( ImGuiKey_Home );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Inbox" );
        IM_CHECK_EQ( v.ListView, 0 );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Inbox" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "grid_view_arrows_stay_inside" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v            = ctx->GetVars< KeyboardVars >();
        static const char * tiles[] = { "T1", "T2", "T3", "T4", "T5" };
        BeginTestWindow();
        ImFluent::Button( "before" );
        ImFluent::BeginGridView( "grid" );
        for ( int i = 0; i < 5; ++i )
        {
            if ( ImFluent::GridViewItem( tiles[ i ], v.Grid == i, ImVec2( 60.f, 40.f ) ) )
                v.Grid = i;
            if ( i != 2 && i != 4 )
                ImGui::SameLine();
        }
        ImFluent::EndGridView();
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "T1" );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "T3" );
        IM_CHECK_EQ( v.Grid, 2 );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "T3" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "T5" );
        IM_CHECK_EQ( v.Grid, 4 );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "T5" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "T4" );
        ctx->KeyPress( ImGuiKey_UpArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "T1" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "T1" );
        ctx->KeyPress( ImGuiMod_Ctrl | ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "T2" );
        IM_CHECK_EQ( v.Grid, 0 );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "tree_view_one_stop_space_checks" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::Button( "before" );
        ImFluent::BeginTreeView( "tree" );
        if ( ImFluent::TreeNode( "root", &v.TreeRoot ) )
        {
            ImFluent::ListViewItem( "leaf" );
            if ( ImFluent::TreeNode( "sub", &v.TreeSubOpen, &v.TreeSubChk ) )
                ImFluent::TreePop();
            ImFluent::TreePop();
        }
        ImFluent::EndTreeView();
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "root" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "leaf" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "root" );
        ctx->KeyPress( ImGuiKey_DownArrow, 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "sub" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "sub" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK( v.TreeSubChk );
        IM_CHECK( !v.TreeSubOpen );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
        ctx->KeyPress( ImGuiMod_Shift | ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "sub" );
    };

    // WinUI CommandBar: Left/Right stop at the first command and at More; Down on More opens the overflow, Escape focuses More.
    t          = IM_REGISTER_TEST( engine, "keyboard", "command_bar_left_right_and_more" );
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        IM_UNUSED( ctx );
        BeginTestWindow();
        if ( ImFluent::BeginCommandBar( "bar" ) )
        {
            ImFluent::AppBarButton( "Save", ImFluentIcon_Save );
            ImFluent::AppBarButton( "Copy", ImFluentIcon_Copy );
            if ( ImFluent::BeginCommandBarOverflow() )
            {
                ImFluent::MenuFlyoutItem( "Settings" );
                ImFluent::MenuFlyoutItem( "About" );
                ImFluent::EndCommandBarOverflow();
            }
            ImFluent::EndCommandBar();
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "**/Save" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Save" );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "##cb-more" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "##cb-more" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Settings" );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "##cb-more" );
        ctx->KeyPress( ImGuiKey_UpArrow ); // Up opens on the last item
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "About" );
        ctx->KeyPress( ImGuiKey_Escape );
    };

    // WinUI AutoSuggestBox: Escape closes the suggestion list and keeps the typed text; focus stays in the box.
    t = IM_REGISTER_TEST( engine, "keyboard", "auto_suggest_box_escape_keeps_typed_text" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v             = ctx->GetVars< KeyboardVars >();
        static const char * cities[] = { "Paris", "Parma", "Prague" };
        BeginTestWindow();
        ImFluent::AutoSuggestBox( "city", v.City, sizeof( v.City ), cities, 3 );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "city" );
        ctx->KeyPress( ImGuiKey_Enter );
        ctx->KeyCharsAppend( "Par" );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( v.City, "Par" );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "city" );
    };

    // [SECTION] Keyboard: popups opened by a button (open, focus inside, Escape, focus back, open again)

    // Flyout, TeachingTip (light dismiss) and DropDownButton share the WinUI popup rules; Tab stays inside.
    struct PopupCase
    {
        const char * Name;
        int Kind; // 0 Flyout, 1 TeachingTip, 2 DropDownButton + MenuFlyout
    };
    static const PopupCase popup_cases[] = { { "flyout_escape_and_reopen", 0 }, { "teaching_tip_escape_and_reopen", 1 }, { "drop_down_button_escape_and_reopen", 2 } };
    for ( const PopupCase & pc : popup_cases )
    {
        t             = IM_REGISTER_TEST( engine, "keyboard", pc.Name );
        t->ArgVariant = pc.Kind;
        t->GuiFunc    = []( ImGuiTestContext * ctx )
        {
            BeginTestWindow();
            switch ( ctx->Test->ArgVariant )
            {
                case 0:
                    if ( ImFluent::Button( "owner" ) )
                        ImFluent::OpenFlyout( "##popup" );
                    if ( ImFluent::BeginFlyout( "##popup" ) )
                    {
                        ImFluent::Button( "first" );
                        ImFluent::Button( "second" );
                        ImFluent::EndFlyout();
                    }
                    break;
                case 1:
                    if ( ImFluent::Button( "owner" ) )
                        ImFluent::OpenTeachingTip( "##popup" );
                    if ( ImFluent::BeginTeachingTip( "##popup", "Tip" ) )
                    {
                        ImFluent::Button( "first" );
                        ImFluent::Button( "second" );
                        ImFluent::EndTeachingTip();
                    }
                    break;
                default:
                    if ( ImFluent::DropDownButton( "owner" ) )
                        ImFluent::OpenMenuFlyout( "##popup" );
                    if ( ImFluent::BeginMenuFlyout( "##popup" ) )
                    {
                        ImFluent::MenuFlyoutItem( "first" );
                        ImFluent::MenuFlyoutItem( "second" );
                        ImFluent::EndMenuFlyout();
                    }
                    break;
            }
            EndTestWindow();
        };
        t->TestFunc = []( ImGuiTestContext * ctx )
        {
            StartKeyboardTest( ctx );
            ctx->NavMoveTo( "owner" );
            ctx->KeyPress( ImGuiKey_Space );
            IM_CHECK_EQ( PopupCount( ctx ), 1 );
            IM_CHECK_STR_EQ( FocusLabel( ctx ), "first" );
            ctx->KeyPress( ImGuiKey_Tab );
            IM_CHECK_STR_EQ( FocusLabel( ctx ), ctx->Test->ArgVariant == 2 ? "first" : "second" ); // WinUI menus ignore Tab
            IM_CHECK_EQ( PopupCount( ctx ), 1 );
            ctx->KeyPress( ImGuiKey_Escape );
            IM_CHECK_EQ( PopupCount( ctx ), 0 );
            IM_CHECK_STR_EQ( FocusLabel( ctx ), "owner" );
            ctx->KeyPress( ImGuiKey_Enter );
            IM_CHECK_EQ( PopupCount( ctx ), 1 );
            IM_CHECK_STR_EQ( FocusLabel( ctx ), "first" );
            ctx->KeyPress( ImGuiKey_Escape );
            IM_CHECK_EQ( PopupCount( ctx ), 0 );
        };
    }

    // [SECTION] Keyboard: MenuBar, CommandBarFlyout, FlipView, Pivot

    t = IM_REGISTER_TEST( engine, "keyboard", "menu_bar_open_switch_escape" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::Button( "before" );
        if ( ImFluent::BeginMenuBar( "bar" ) )
        {
            if ( ImFluent::BeginMenuBarItem( "File" ) )
            {
                if ( ImFluent::MenuFlyoutItem( "New" ) )
                    v.MenuCmd = 1;
                ImFluent::EndMenuBarItem();
            }
            if ( ImFluent::BeginMenuBarItem( "Edit" ) )
            {
                if ( ImFluent::MenuFlyoutItem( "Undo" ) )
                    v.MenuCmd = 2;
                ImFluent::EndMenuBarItem();
            }
            ImFluent::EndMenuBar();
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
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "File" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Edit" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "File" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "New" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Undo" );
        ctx->KeyPress( ImGuiKey_Escape );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Edit" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Undo" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( v.MenuCmd, 2 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "command_bar_flyout_rows" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        if ( ImFluent::Button( "show" ) )
            ImFluent::OpenFlyout( "##cbf" );
        if ( ImFluent::BeginCommandBarFlyout( "##cbf" ) )
        {
            ImFluent::AppBarButton( "Share", ImFluentIcon_Share );
            ImFluent::AppBarButton( "Save", ImFluentIcon_Save );
            ImFluent::CommandBarFlyoutSecondary();
            if ( ImFluent::MenuFlyoutItem( "Resize" ) )
                v.CbfCmd++;
            ImFluent::EndCommandBarFlyout();
        }
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "show" );
        ctx->KeyPress( ImGuiKey_Space );
        IM_CHECK_EQ( PopupCount( ctx ), 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Share" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Share" );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Save" );
        ctx->KeyPress( ImGuiKey_DownArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "Resize" );
        ctx->KeyPress( ImGuiKey_Enter );
        IM_CHECK_EQ( v.CbfCmd, 1 );
        IM_CHECK_EQ( PopupCount( ctx ), 0 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "show" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "flip_view_left_right" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::Button( "before" );
        if ( ImFluent::BeginFlipView( "flip", &v.Flip, 3, ImVec2( 200.f, 100.f ) ) )
            ImGui::Text( "Item %d", v.Flip );
        ImFluent::EndFlipView();
        ImFluent::Button( "after" );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        StartKeyboardTest( ctx );
        ctx->NavMoveTo( "before" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "flip" );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_EQ( v.Flip, 1 );
        ctx->KeyPress( ImGuiKey_RightArrow, 2 );
        IM_CHECK_EQ( v.Flip, 2 );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_EQ( v.Flip, 1 );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "flip" );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
    };

    t = IM_REGISTER_TEST( engine, "keyboard", "pivot_one_stop_wraps" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v              = ctx->GetVars< KeyboardVars >();
        static const char * headers[] = { "A", "B", "C" };
        BeginTestWindow();
        ImFluent::Button( "before" );
        if ( ImFluent::BeginPivot( "pivot" ) )
        {
            for ( int i = 0; i < 3; ++i )
                if ( ImFluent::PivotItem( headers[ i ], v.PivotSel == i ) )
                    v.PivotSel = i;
            ImFluent::EndPivot();
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
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "A" );
        ctx->KeyPress( ImGuiKey_LeftArrow );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "C" );
        IM_CHECK_EQ( v.PivotSel, 2 );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_EQ( v.PivotSel, 0 );
        ctx->KeyPress( ImGuiKey_RightArrow );
        IM_CHECK_EQ( v.PivotSel, 1 );
        ctx->KeyPress( ImGuiKey_Tab );
        IM_CHECK_STR_EQ( FocusLabel( ctx ), "after" );
    };

    // [SECTION] Library behaviour

    // SetNextItemHeader() for a widget that never runs must not reach a later widget in another window.
    t = IM_REGISTER_TEST( engine, "library", "next_item_header_does_not_leak" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        if ( v.LeakHeader )
            ImFluent::SetNextItemHeader( "leaked header" ); // No widget follows
        EndTestWindow();
        ImGui::SetNextWindowPos( ImVec2( 400.f, 80.f ), ImGuiCond_Appearing );
        ImGui::Begin( "Test Window 2", NULL, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize );
        ImFluent::TextBox( "box", v.LeakBox, sizeof( v.LeakBox ) );
        v.LeakBoxY = ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y;
        ImGui::End();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        ctx->Yield( 2 );
        const float before = v.LeakBoxY;
        v.LeakHeader       = true;
        ctx->Yield( 3 );
        IM_CHECK_EQ( v.LeakBoxY, before );
    };

    // GitHub #8: Dear ImGui widgets use the accent.
    t           = IM_REGISTER_TEST( engine, "library", "imgui_colors_follow_accent" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        auto same_rgb = []( const ImVec4 & a, const ImVec4 & b )
        {
            return ImAbs( a.x - b.x ) < 1e-3f && ImAbs( a.y - b.y ) < 1e-3f && ImAbs( a.z - b.z ) < 1e-3f;
        };
        const ImFluentThemePreset preset    = ImFluent::GetThemePreset();
        const ImFluentThemePreset presets[] = { ImFluentThemePreset_Light, ImFluentThemePreset_Dark };
        for ( ImFluentThemePreset p : presets )
        {
            ImFluent::SetThemePreset( p );
            const ImVec4 accent = ImFluent::GetStyle().Colors[ ImFluentCol_AccentFillDefault ];
            const ImVec4 * c    = ImGui::GetStyle().Colors;
            IM_CHECK( same_rgb( c[ ImGuiCol_CheckMark ], accent ) );
            IM_CHECK( same_rgb( c[ ImGuiCol_Header ], accent ) && c[ ImGuiCol_Header ].w > 0.f );
            IM_CHECK( same_rgb( c[ ImGuiCol_HeaderHovered ], accent ) );
            IM_CHECK( same_rgb( c[ ImGuiCol_SliderGrab ], accent ) );
        }
        ImFluent::SetThemePreset( preset );
    };

    // GitHub #7: step arrows repeat while held, unless ImFluentActions::RepeatStepButtons is off.
    t = IM_REGISTER_TEST( engine, "mouse", "step_buttons_repeat_while_held" );
    t->SetVarsDataType< KeyboardVars >();
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        BeginTestWindow();
        ImFluent::PagerControl( "pager", &v.Page, 100 );
        EndTestWindow();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        KeyboardVars & v = ctx->GetVars< KeyboardVars >();
        ctx->SetRef( "Test Window" );
        ctx->SetInputMode( ImGuiInputSource_Mouse );
        ImFluentActions & actions = ImFluent::GetActions();
        const bool backup         = actions.RepeatStepButtons;
        for ( int repeat = 1; repeat >= 0; --repeat )
        {
            actions.RepeatStepButtons = repeat != 0;
            v.Page                    = 0;
            ctx->Yield( 2 );
            ctx->MouseMove( "**/Next page" );
            ctx->MouseDown( ImGuiMouseButton_Left );
            ctx->SleepNoSkip( 1.f, 1.f / 60.f );
            ctx->MouseUp( ImGuiMouseButton_Left );
            if ( repeat )
                IM_CHECK_GT( v.Page, 2 );
            else
                IM_CHECK_EQ( v.Page, 1 );
        }
        actions.RepeatStepButtons = backup;
    };

    // [SECTION] API

    t           = IM_REGISTER_TEST( engine, "api", "theme_preset_round_trip" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        const ImFluentThemePreset backup    = ImFluent::GetThemePreset();
        const ImFluentThemePreset presets[] = { ImFluentThemePreset_Light, ImFluentThemePreset_Dark, ImFluentThemePreset_HighContrast };
        for ( ImFluentThemePreset p : presets )
        {
            ImFluent::SetThemePreset( p );
            IM_CHECK_EQ( ImFluent::GetThemePreset(), p );
        }
        ImFluent::SetThemePreset( backup );
    };

    // An app's metric and text overrides survive a theme change.
    t           = IM_REGISTER_TEST( engine, "api", "theme_change_keeps_style_overrides" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        ImFluentStyle & style                                              = ImFluent::GetStyle();
        const ImFluentThemePreset preset                                   = ImFluent::GetThemePreset();
        const float height                                                 = style.ControlHeight;
        const char * text                                                  = style.LocalizationTable[ ImFluentLocKey_AutoSuggestNoSuggestions ];
        style.ControlHeight                                                = 40.f;
        style.LocalizationTable[ ImFluentLocKey_AutoSuggestNoSuggestions ] = "Nichts gefunden";
        ImFluent::SetThemePreset( preset == ImFluentThemePreset_Dark ? ImFluentThemePreset_Light : ImFluentThemePreset_Dark );
        IM_CHECK_EQ( style.ControlHeight, 40.f );
        IM_CHECK_STR_EQ( style.LocalizationTable[ ImFluentLocKey_AutoSuggestNoSuggestions ], "Nichts gefunden" );
        style.ControlHeight                                                = height;
        style.LocalizationTable[ ImFluentLocKey_AutoSuggestNoSuggestions ] = text;
        ImFluent::SetThemePreset( preset );
    };

    t           = IM_REGISTER_TEST( engine, "api", "style_color_and_var_stacks" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        ImFluentStyle & style = ImFluent::GetStyle();
        const ImVec4 color    = style.Colors[ ImFluentCol_AccentFillDefault ];
        const float height    = style.ControlHeight;
        ImFluent::PushStyleColor( ImFluentCol_AccentFillDefault, IM_COL32( 255, 0, 0, 255 ) );
        ImFluent::PushStyleVar( ImFluentStyleVar_ControlHeight, 48.f );
        IM_CHECK_EQ( style.ControlHeight, 48.f );
        IM_CHECK( style.Colors[ ImFluentCol_AccentFillDefault ].x == 1.f && style.Colors[ ImFluentCol_AccentFillDefault ].y == 0.f );
        ImFluent::PopStyleVar();
        ImFluent::PopStyleColor();
        IM_CHECK_EQ( style.ControlHeight, height );
        IM_CHECK( style.Colors[ ImFluentCol_AccentFillDefault ].x == color.x && style.Colors[ ImFluentCol_AccentFillDefault ].z == color.z );
    };

    t           = IM_REGISTER_TEST( engine, "api", "actions_defaults" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        const ImFluentActions a;
        IM_CHECK_EQ( a.RevealPassword, ImGuiMod_Alt | ImGuiKey_F8 );
        IM_CHECK_EQ( a.TogglePopup, ( ImGuiKeyChord )ImGuiKey_F4 );
        IM_CHECK_EQ( a.NextTab, ImGuiMod_Ctrl | ImGuiKey_Tab );
        IM_CHECK_EQ( a.CloseDialog, ( ImGuiKeyChord )ImGuiKey_Escape );
        IM_CHECK( a.RepeatStepButtons );
    };

    // Without platform hooks the backdrop is the solid fallback colour; high contrast turns it off.
    t          = IM_REGISTER_TEST( engine, "api", "backdrop_without_hooks" );
    t->GuiFunc = []( ImGuiTestContext * ctx )
    {
        ImFluent::RenderBackdrop();
    };
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        ImFluentBackdropHooks & hooks      = ImFluent::GetBackdropHooks();
        const ImFluentBackdropHooks backup = hooks;
        const ImFluentThemePreset preset   = ImFluent::GetThemePreset();
        hooks                              = ImFluentBackdropHooks();
        ImFluent::SetBackdrop( ImFluentBackdrop_Mica );
        ImFluent::SetThemePreset( ImFluentThemePreset_Dark );
        IM_CHECK_EQ( ImFluent::GetBackdrop(), ImFluentBackdrop_Mica );
        IM_CHECK( ImFluent::IsBackdropVisible() );
        ctx->Yield( 2 );
        ImFluent::SetThemePreset( ImFluentThemePreset_HighContrast );
        IM_CHECK( !ImFluent::IsBackdropVisible() );
        ImFluent::SetBackdrop( ImFluentBackdrop_None );
        IM_CHECK( !ImFluent::IsBackdropVisible() );
        ImFluent::SetThemePreset( preset );
        hooks = backup;
    };

    // The wallpaper placement every backend uses, on a 1920x1080 monitor.
    t           = IM_REGISTER_TEST( engine, "api", "place_wallpaper" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        auto near = []( const ImVec2 & a, float x, float y )
        {
            return ImAbs( a.x - x ) < 1e-4f && ImAbs( a.y - y ) < 1e-4f;
        };
        const ImVec4 monitor( 0.f, 0.f, 1920.f, 1080.f );
        ImFluentWallpaper wp;
        ImFluent::PlaceWallpaper( &wp, ImFluentWallpaperFit_Fill, 1000.f, 1000.f, monitor, monitor ); // Crops top and bottom
        IM_CHECK( near( wp.UV0, 0.f, 0.21875f ) && near( wp.UV1, 1.f, 0.78125f ) );
        ImFluent::PlaceWallpaper( &wp, ImFluentWallpaperFit_Fit, 1000.f, 1000.f, monitor, monitor ); // Letterboxed left and right
        IM_CHECK( near( wp.UV0, -420.f / 1080.f, 0.f ) && near( wp.UV1, 1500.f / 1080.f, 1.f ) );
        ImFluent::PlaceWallpaper( &wp, ImFluentWallpaperFit_Center, 960.f, 540.f, monitor, monitor );
        IM_CHECK( near( wp.UV0, -0.5f, -0.5f ) && near( wp.UV1, 1.5f, 1.5f ) );
        ImFluent::PlaceWallpaper( &wp, ImFluentWallpaperFit_Tile, 960.f, 540.f, monitor, monitor );
        IM_CHECK( wp.Repeat && near( wp.UV0, 0.f, 0.f ) && near( wp.UV1, 2.f, 2.f ) );
        ImFluent::PlaceWallpaper( &wp, ImFluentWallpaperFit_Stretch, 100.f, 300.f, monitor, monitor );
        IM_CHECK( !wp.Repeat && near( wp.UV0, 0.f, 0.f ) && near( wp.UV1, 1.f, 1.f ) );
        const ImVec4 right( 1920.f, 0.f, 3840.f, 1080.f ), desktop( 0.f, 0.f, 3840.f, 1080.f ); // Right half of a span
        ImFluent::PlaceWallpaper( &wp, ImFluentWallpaperFit_Span, 3840.f, 1080.f, right, desktop );
        IM_CHECK( near( wp.UV0, 0.5f, 0.f ) && near( wp.UV1, 1.f, 1.f ) );
    };
}
