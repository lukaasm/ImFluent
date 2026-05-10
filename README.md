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

<img width="2361" height="1488" alt="image" src="https://github.com/user-attachments/assets/2b951640-97b7-47e7-94f5-884c3ec5e5b5" />
<img width="2361" height="1488" alt="image" src="https://github.com/user-attachments/assets/1257535a-64f2-4338-820d-eb133b1318ed" />
<img width="2361" height="1488" alt="image" src="https://github.com/user-attachments/assets/dd322c8b-2b0d-45a4-b508-75468f7843e6" />
<img width="2361" height="1488" alt="image" src="https://github.com/user-attachments/assets/58052b1b-50d2-490b-977e-a42d0a2fe9da" />
<img width="2361" height="1488" alt="image" src="https://github.com/user-attachments/assets/86f1319c-4855-4e3b-a21d-56276d512b56" />
<img width="2361" height="1488" alt="image" src="https://github.com/user-attachments/assets/4e921354-b0eb-4092-974e-5db86e41de9f" />
<img width="2361" height="1488" alt="image" src="https://github.com/user-attachments/assets/f9efcce9-c607-4342-9dca-502449e80a1b" />
<img width="2361" height="1488" alt="image" src="https://github.com/user-attachments/assets/6440ae13-13fb-47b9-b9ec-be9222e767be" />

## License

MIT — see [LICENSE.txt](LICENSE.txt).
