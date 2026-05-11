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

<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/6c953460-cfce-48c8-9283-bfec0c289117" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/c39530cb-96f6-47e2-9049-ecb265f785f2" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/be00ff0c-85fe-4d6d-9a6c-51d10fd5784e" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/1ff5f2a0-fb15-4b3d-a2b0-b4ebbd2956ba" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/b947c106-02f8-4d85-a0cc-b135ff96e96e" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/50f37e94-d34a-448b-8f08-ab646556e571" />
<img width="1924" height="2088" alt="image" src="https://github.com/user-attachments/assets/2c87e15b-ceba-49b8-8c95-3e7824fe3621" />


## License

MIT — see [LICENSE.txt](LICENSE.txt).
