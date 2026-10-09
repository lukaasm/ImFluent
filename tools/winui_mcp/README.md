# winui_mcp

An MCP server that drives the real **WinUI 3 Gallery** through UI Automation (FlaUI.UIA3), with the same tool names,
arguments and result shapes as the ImFluent gallery server (`src/automation/imfluent_automation.cpp`, port 7801).
Both apps can be driven side by side with identical calls and their rectangles compared directly.

## Build

```
dotnet build tools/winui_mcp -c Release
```

Needs the .NET 9 SDK (or newer) on Windows. NuGet dependency: `FlaUI.UIA3`.

## Run

```
tools/winui_mcp/bin/Release/net9.0-windows/WinUiMcp.exe [--port 7802] [--exe <WinUIGallery.exe>] [--attach] [--same-desktop | --interactive]
```

- Modes. None of the background modes moves the mouse or sends real input.
  - Default, background on a hidden desktop: the driver creates the desktop `WinSta0\ImFluentWinUiGallery`, which is
    never shown, and runs the gallery there. Nothing the gallery does can appear on your screen or take your
    keyboard focus, because activation and the foreground are per desktop. UI Automation, posted keys and
    PrintWindow screenshots work there. A gallery you run on your own desktop is left alone.
  - `--same-desktop`, background on your desktop: the gallery starts suspended, and a WinEvent hook parks its window
    right of all monitors as it is created. The driver holds `LockSetForegroundWindow` while it launches and during
    each call. WinUI still activates the gallery after UIA selections; the driver then gives the foreground back
    within milliseconds (`app_info.background.focus_given_back` counts these).
  - `--interactive`: real input (`SendInput`); the gallery comes to the front and the mouse moves.
- Attach to a running `WinUIGallery` process (on the driver's desktop), otherwise launch
  `docs/winui_gallery/WinUIGallery/bin/WinUIGallery/x64/Debug-Unpackaged/net9.0-windows10.0.22621.0/win-x64/WinUIGallery.exe`
  (resolved from the repository root, the directory that holds `vcpkg.json`). If the gallery exits, the next tool call
  relaunches it (or fails with `--attach`).
- `--dump [depth]` prints the UIA control tree of the gallery window and exits (debugging aid).
- Endpoint: `http://127.0.0.1:7802/mcp`, MCP Streamable HTTP with plain JSON replies (no SSE). POST only; 202 for
  notifications; GET/DELETE get 405; requests with a non-loopback `Origin` get 403.
- Stop with Ctrl+C.

```
python tools/mcp_call.py --port 7802 goto_page '{"page":"ComboBox"}'
python tools/mcp_call.py --port 7802 screenshot '{"scale":0.5}' --out shot.png
```

Claude Code: `claude mcp add --transport http winui-gallery http://127.0.0.1:7802/mcp`.

## Tools

`app_info`, `list_pages`, `goto_page`, `set_theme`, `resize_window`, `screenshot`, `list_windows`, `nav_state`,
`list_items`, `item_info`, `click`, `hover`, `key`, `type_text`, `mouse`, `run_steps`, `run_tests`.

- Pages are the gallery's `ControlInfoData.json` entries: `Home`, `AllControls`, `Settings`, `Section_<Group>`,
  `Item_<Control>`. `goto_page` also accepts `Button`, `Item_Button`, `BasicInput` or a title. Navigation selects the
  NavigationView item through UIA (expanding its category), falling back to typing into the title-bar search box when
  the pane is collapsed. It waits until the page header shows the page.
- `list_items` walks the UIA control view of the page content (the NavigationView content, not the pane). Item
  `id` = AutomationId, or `rt:<runtime id>` when the element has none or its AutomationId repeats (template parts).
  `label` = UIA Name, which is the accessible name and can differ from the visible text. Extra field: `value`
  (Value pattern). Flags: `focused, disabled, checked, unchecked, indeterminate, opened, closed, selected, offscreen,
  inputable`.
- `ref`: `id:XYZ`/`rt:...`/an id from `list_items`, else a Name (exact, then case-insensitive), else an AutomationId.
  `Parent/Child` searches Child under Parent. Search order: open popups, the page (or `window`), the whole window.
- `window`: `$FOCUSED` (the focused popup), `$MAIN` (whole window), or a name/id from `list_windows`.
- `key` uses the ImFluent chord syntax (`"Tab"`, `"Shift+Tab"`, `"Alt+Down"`, `"Down Down Enter"`, Dear ImGui key
  names plus Left/Right/Up/Down/Esc/Return/Del/PgUp/PgDn/Apps). Action tools return `{steps?, failed_step?, state}`
  where `state` is `nav_state`; a failed step also sets `isError`.
- `run_steps` supports the ImFluent step set; `wait{frames}` counts 16 ms per frame; `keyboard:true` turns `click`
  into focus + Space (Enter for links and menu items). `speed` is accepted and ignored.
- `set_theme` goes through the gallery's Settings page (the choice is saved in the gallery's `AppConfig.json`) and
  returns to the previous page. `"system"` restores "Use system setting". `highcontrast` is an error.
- `run_tests` always returns an error (no test registry on the WinUI side).

## Coordinates

Everything is in **physical pixels relative to the top-left of the gallery window's client area** (the gallery draws
its own title bar, so the client area starts at the title bar). That is the same convention as the ImFluent server.
`app_info.dpi_scale` is `GetDpiForWindow / 96`; `window_rect` and `client_origin` are screen coordinates.

## Known limits

- Background modes: `hover`, `mouse`, right and double clicks, `key_down`/`key_up` and Ctrl/Shift/Win key chords need
  `--interactive`. Clicks use UIA patterns (Invoke, Toggle, SelectionItem, ExpandCollapse), text uses the Value
  pattern, and keys are posted to the XAML input window. WinUI keyboard behaviour is taken from the WinUI source, not
  measured live.
- `--interactive`: the gallery is brought to the foreground (AttachThreadInput / Alt-key workaround) and the mouse
  cursor moves. Do not use the mouse or keyboard while a call runs. A locked workstation or an elevated foreground
  window blocks input.
- `nav_state.focus` is empty (and `foreground:false`) when another application has the foreground.
- Popups: WinUI 3 opens combo box lists, menus, flyouts and tooltips as separate `PopupWindowSiteBridge` windows. They
  are listed by `list_windows`/`nav_state` and named after their content (`"Colors popup"`, `"Menu"`). Their UIA
  subtree also exposes the page behind the light-dismiss layer, so popup searches keep only elements inside the popup
  rectangle. Screenshots composite popup windows on top of the main window; their drop shadows are dropped.
- UIA names come from the OS for system parts (e.g. the Settings item is "Ustawienia" on a Polish Windows); use the
  AutomationIds (`SettingsItem`) when in doubt.
- The checked-in unpackaged Debug build lacks `Microsoft.WindowsAppRuntime.Insights.Resource.dll` and crashes on the
  Home page. When the driver launches the gallery it copies that DLL from the installed Windows App Runtime package
  into `bin/.../runtime_shim/` and puts that folder on the gallery's `PATH`; the gallery folder is not modified. A
  gallery started by hand needs the DLL next to `WinUIGallery.exe`.
- Some sample pages are heavy (NavigationView, Acrylic: ~4 s). The MapControl page closed the gallery once during a
  full page sweep; the driver relaunches it on the next call.
