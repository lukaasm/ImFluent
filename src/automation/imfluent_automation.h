#pragma once

// Gallery automation: Dear ImGui Test Engine + MCP server. Built only with the vcpkg feature `test-engine`.

struct ImGuiTestEngine;

struct ImFluentAutomationHost
{
    // Copies framebuffer pixels of the last frame as RGBA8 (R in the low byte), top row first.
    // Called on the main thread after the frame was submitted.
    bool ( *CaptureFramebuffer )( int x, int y, int w, int h, unsigned int * out_rgba, void * user_data ) = nullptr;
    // Resizes the OS window (window coordinates). Optional.
    void ( *SetWindowSize )( int w, int h, void * user_data )                                             = nullptr;
    void * UserData                                                                                       = nullptr;
};

struct ImFluentAutomationConfig
{
    int McpPort              = 7801;                   // 0 disables the MCP server
    const char * RootWindow  = "##imfluent-demo-root"; // Window that relative item references resolve against
    bool RunTests            = false;                  // Queue the registered tests at start-up, then request exit
    const char * TestFilter  = nullptr;                // Test Engine filter syntax, e.g. "keyboard" or "-slow"
    const char * ExportJUnit = nullptr;                // Write JUnit XML results to this path after --run-tests
    bool ShowEngineUi        = false;                  // Show the Dear ImGui Test Engine window
    bool NormalSpeed         = false;                  // Move the mouse and type at a watchable speed
};

namespace ImFluentAutomation
{
    // Call after ImGui::CreateContext() and before the first frame.
    bool Init( const ImFluentAutomationConfig & config, const ImFluentAutomationHost & host );
    // Call between ImGui::NewFrame() and ImGui::Render(), after the application UI.
    void ShowWindows();
    // Call once per frame after the frame was submitted to the GPU.
    void PostSwap();
    // True while tests run in fast mode: the application should not wait for vsync.
    bool WantsMaxSpeed();
    // True when a --run-tests session finished. exit_code receives the number of failed tests.
    bool WantsExit( int * exit_code );
    // Call before ImGui::DestroyContext().
    void Stop();
    // Call after ImGui::DestroyContext().
    void Shutdown();
    ImGuiTestEngine * GetEngine();
} // namespace ImFluentAutomation

// Registers the ImFluent regression tests (imfluent_tests.cpp).
void ImFluentTests_RegisterAll( ImGuiTestEngine * engine );
