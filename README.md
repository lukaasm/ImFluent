# ImFluent

A [Dear ImGui](https://github.com/ocornut/imgui) wrapper for providing widgets that adhere to Microsoft [Fluent 2](https://fluent2.microsoft.design/) design system.

A built-in `ImFluent::ShowDemoWindow()` reproduces the WinUI 3 Gallery shell
with one demo page per control.

## Use

```cpp
#include "imfluent.h"

ImFluent::SetThemePreset(ImFluentThemePreset_Dark);

if (ImFluent::AccentButton("Sign in")) { /* ... */ }

static bool wifi = true;
ImFluent::ToggleSwitch("Wi-Fi", &wifi);

ImFluent::ShowDemoWindow();
```

Define `IMFLUENT_DISABLE_DEMO_WINDOWS` to compile out the demo.

## License

MIT — see [LICENSE.txt](LICENSE.txt).
