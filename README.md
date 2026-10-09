# ImFluent

A [Dear ImGui](https://github.com/ocornut/imgui) wrapper for providing widgets that adhere to Microsoft [Fluent 2](https://fluent2.microsoft.design/) design system.

A built-in `ImFluent::ShowDemoWindow()` reproduces the WinUI 3 Gallery shell
with one demo page per control.

## License

MIT — see [LICENSE](LICENSE).

## Use

```cpp
#include "imfluent.h"

ImFluent::LoadFluentSystemFonts();
ImFluent::SetThemePreset(ImFluentThemePreset_Dark);

if (ImFluent::AccentButton("Sign in")) { /* ... */ }

static bool wifi = true;
ImFluent::ToggleSwitch("Wi-Fi", &wifi);

ImFluent::ShowDemoWindow();
```

Define `IMFLUENT_DISABLE_DEMO_WINDOWS` to compile out the demo.

## Keyboard

Every widget works with the keyboard alone, following WinUI: composite controls (RadioButtons, ListBox,
SelectorBar, NavigationView, TabView, ...) are one Tab stop, arrows move inside them, and popups open, close
with Escape and give focus back to their owner. Put `ListViewItem`, `GridViewItem` and `TreeNode` rows between
`BeginListView`/`BeginGridView`/`BeginTreeView` and the matching `End` call to make the list one Tab stop too.
The command shortcuts are key chords in `ImFluentActions`,
with WinUI defaults. Change them, or set one to 0 to turn it off:

```cpp
ImFluentActions & actions = ImFluent::GetActions();
actions.RevealPassword    = ImGuiMod_Ctrl | ImGuiKey_R; // default Alt+F8
actions.TogglePopup       = 0;                          // no F4 for ComboBox and SplitButton
```

## Automation and tests

The `test-engine` vcpkg feature (on in the `configure-win64` preset) builds `ImFluentGallery` with the
[Dear ImGui Test Engine](https://github.com/ocornut/imgui_test_engine) and an MCP server.
The Test Engine has its own license; the ImFluent library itself does not depend on it.

```
ImFluentGallery                      # MCP server on http://127.0.0.1:7801/mcp
ImFluentGallery --run-tests [filter] # run the regression tests, exit code = failed tests
ImFluentGallery --run-tests --junit results.xml
ImFluentGallery --mcp-port 0 --te-ui # no MCP server, show the Test Engine window
ImFluentGallery --hidden             # no window at all: renders offscreen, MCP and screenshots still work
ImFluentGallery --no-activate        # show the window without taking keyboard focus
```

The Test Engine injects simulated input into Dear ImGui only: automation never moves the OS mouse or takes
OS focus, so it can run while you work (use `--hidden` to keep the window off your desktop).

`.mcp.json` registers the gallery server (`imfluent-gallery`, port 7801) and the WinUI 3 Gallery driver
(`winui-gallery`, port 7802, see `tools/winui_mcp`) for Claude Code. Both expose the same tools
(`goto_page`, `screenshot`, `list_items`, `click`, `key`, `type_text`, `nav_state`, `run_steps`, ...), so a
control page can be compared side by side. `tools/mcp_call.py` calls a tool from the command line:

```
python tools/mcp_call.py goto_page '{"page":"ComboBox"}'
python tools/mcp_call.py key '{"keys":"Tab Tab Space"}'
python tools/mcp_call.py screenshot '{"scale":0.5}' --out combo.png
```

ImFluent widgets report their label and checked/opened state to the Test Engine
(`IMGUI_TEST_ENGINE_ITEM_INFO`), so tests can address them by label, e.g. `ctx->ItemClick("**/Standard button")`.

<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/6c953460-cfce-48c8-9283-bfec0c289117" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/c39530cb-96f6-47e2-9049-ecb265f785f2" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/be00ff0c-85fe-4d6d-9a6c-51d10fd5784e" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/1ff5f2a0-fb15-4b3d-a2b0-b4ebbd2956ba" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/b947c106-02f8-4d85-a0cc-b135ff96e96e" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/50f37e94-d34a-448b-8f08-ab646556e571" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/2c87e15b-ceba-49b8-8c95-3e7824fe3621" />

