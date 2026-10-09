// ImFluent regression tests for the Dear ImGui Test Engine.
// Each test drives the gallery (ImFluent::ShowDemoWindow) the way a person does.
// Run them with `ImFluentGallery --run-tests [filter]` or the MCP tool `run_tests`.

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

static ImGuiID NavId( ImGuiTestContext * ctx )
{
    return ctx->UiContext->NavId;
}

static const char * NavLabel( ImGuiTestContext * ctx )
{
    const char * label = ImGuiTestEngine_FindItemDebugLabel( ctx->UiContext, ctx->UiContext->NavId );
    return label ? label : "";
}

void ImFluentTests_RegisterAll( ImGuiTestEngine * engine )
{
    ImGuiTest * t = NULL;

    // [SECTION] Smoke

    t           = IM_REGISTER_TEST( engine, "smoke", "button_click" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        GotoPage( ctx, "Item_Button" );
        ctx->ItemClick( "**/Standard button" );
        IM_CHECK_STR_EQ( NavLabel( ctx ), "Standard button" );
    };

    t           = IM_REGISTER_TEST( engine, "smoke", "keyboard_tab_reaches_button" );
    t->TestFunc = []( ImGuiTestContext * ctx )
    {
        GotoPage( ctx, "Item_Button" );
        ctx->SetInputMode( ImGuiInputSource_Keyboard );
        ctx->NavMoveTo( "**/Standard button" );
        IM_CHECK_STR_EQ( NavLabel( ctx ), "Standard button" );
        IM_CHECK( NavId( ctx ) != 0 );
    };
}