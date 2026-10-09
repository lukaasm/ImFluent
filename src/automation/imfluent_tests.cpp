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
};

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
}