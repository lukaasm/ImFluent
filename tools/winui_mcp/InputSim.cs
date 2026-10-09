using System.Runtime.InteropServices;
using static WinUiMcp.Native;

namespace WinUiMcp;

// A parsed key chord: modifiers plus one key.
internal readonly record struct KeyChord(bool Ctrl, bool Shift, bool Alt, bool Win, ushort Vk, bool Extended);

// Real keyboard and mouse input through SendInput. Key names follow Dear ImGui (ImGui::GetKeyName) so the
// "keys" strings are interchangeable with the ImFluent server.
internal static class InputSim
{
    // Dear ImGui named keys -> (virtual key, extended flag).
    static readonly Dictionary<string, (ushort Vk, bool Ext)> s_Keys = BuildKeyTable();

    // Same aliases as the ImFluent server's FindKeyByName().
    static readonly Dictionary<string, string> s_Aliases = new(StringComparer.OrdinalIgnoreCase)
    {
        ["left"] = "LeftArrow", ["right"] = "RightArrow", ["up"] = "UpArrow", ["down"] = "DownArrow",
        ["esc"] = "Escape", ["return"] = "Enter", ["del"] = "Delete", ["ins"] = "Insert",
        ["pgup"] = "PageUp", ["pgdn"] = "PageDown", ["ctrl"] = "LeftCtrl", ["control"] = "LeftCtrl",
        ["shift"] = "LeftShift", ["alt"] = "LeftAlt", ["win"] = "LeftSuper", ["super"] = "LeftSuper",
        ["spacebar"] = "Space", ["apps"] = "Menu", ["contextmenu"] = "Menu",
    };

    static Dictionary<string, (ushort, bool)> BuildKeyTable()
    {
        var t = new Dictionary<string, (ushort, bool)>(StringComparer.OrdinalIgnoreCase)
        {
            ["Tab"] = (0x09, false), ["LeftArrow"] = (0x25, true), ["RightArrow"] = (0x27, true), ["UpArrow"] = (0x26, true),
            ["DownArrow"] = (0x28, true), ["PageUp"] = (0x21, true), ["PageDown"] = (0x22, true), ["Home"] = (0x24, true),
            ["End"] = (0x23, true), ["Insert"] = (0x2D, true), ["Delete"] = (0x2E, true), ["Backspace"] = (0x08, false),
            ["Space"] = (0x20, false), ["Enter"] = (0x0D, false), ["Escape"] = (0x1B, false),
            ["LeftCtrl"] = (0xA2, false), ["LeftShift"] = (0xA0, false), ["LeftAlt"] = (0xA4, false), ["LeftSuper"] = (0x5B, true),
            ["RightCtrl"] = (0xA3, true), ["RightShift"] = (0xA1, false), ["RightAlt"] = (0xA5, true), ["RightSuper"] = (0x5C, true),
            ["Menu"] = (0x5D, true),
            ["Apostrophe"] = (0xDE, false), ["Comma"] = (0xBC, false), ["Minus"] = (0xBD, false), ["Period"] = (0xBE, false),
            ["Slash"] = (0xBF, false), ["Semicolon"] = (0xBA, false), ["Equal"] = (0xBB, false), ["LeftBracket"] = (0xDB, false),
            ["Backslash"] = (0xDC, false), ["RightBracket"] = (0xDD, false), ["GraveAccent"] = (0xC0, false), ["Oem102"] = (0xE2, false),
            ["CapsLock"] = (0x14, false), ["ScrollLock"] = (0x91, false), ["NumLock"] = (0x90, true), ["PrintScreen"] = (0x2C, true),
            ["Pause"] = (0x13, false),
            ["KeypadDecimal"] = (0x6E, false), ["KeypadDivide"] = (0x6F, true), ["KeypadMultiply"] = (0x6A, false),
            ["KeypadSubtract"] = (0x6D, false), ["KeypadAdd"] = (0x6B, false), ["KeypadEnter"] = (0x0D, true),
            ["AppBack"] = (0xA6, true), ["AppForward"] = (0xA7, true),
        };
        for (int i = 0; i <= 9; ++i)
        {
            t[((char)('0' + i)).ToString()] = ((ushort)(0x30 + i), false);
            t["Keypad" + i] = ((ushort)(0x60 + i), false);
        }
        for (char c = 'A'; c <= 'Z'; ++c)
            t[c.ToString()] = (c, false);
        for (int i = 1; i <= 24; ++i)
            t["F" + i] = ((ushort)(0x70 + i - 1), false);
        return t;
    }

    static bool FindKey(string name, out (ushort Vk, bool Ext) key)
    {
        if (s_Aliases.TryGetValue(name, out var alias))
            name = alias;
        return s_Keys.TryGetValue(name, out key);
    }

    // "Ctrl+Shift+Tab" -> chord. Mirrors ParseKeyChord() of the ImFluent server.
    public static bool ParseKeyChord(string text, out KeyChord chord)
    {
        chord = default;
        bool ctrl = false, shift = false, alt = false, win = false, haveKey = false;
        (ushort Vk, bool Ext) key = default;
        int start = 0;
        while (start <= text.Length)
        {
            int plus = text.IndexOf('+', start);
            if (plus == start && plus + 1 == text.Length)
                plus = -1; // trailing "+" is not a separator
            string tok = plus < 0 ? text[start..] : text[start..plus];
            bool last = plus < 0;
            if (!last && (tok.Equals("ctrl", StringComparison.OrdinalIgnoreCase) || tok.Equals("control", StringComparison.OrdinalIgnoreCase)))
                ctrl = true;
            else if (!last && tok.Equals("shift", StringComparison.OrdinalIgnoreCase))
                shift = true;
            else if (!last && tok.Equals("alt", StringComparison.OrdinalIgnoreCase))
                alt = true;
            else if (!last && (tok.Equals("super", StringComparison.OrdinalIgnoreCase) || tok.Equals("win", StringComparison.OrdinalIgnoreCase) || tok.Equals("cmd", StringComparison.OrdinalIgnoreCase)))
                win = true;
            else
            {
                if (!FindKey(tok, out key))
                    return false;
                haveKey = true;
            }
            if (last)
                break;
            start = plus + 1;
        }
        if (!haveKey)
            return false;
        chord = new KeyChord(ctrl, shift, alt, win, key.Vk, key.Ext);
        return true;
    }

    // Chords separated by spaces or commas. Mirrors SplitKeys() of the ImFluent server.
    public static List<string> SplitKeys(string text)
    {
        var list = new List<string>();
        var cur = new System.Text.StringBuilder();
        foreach (char c in text)
        {
            if (c == ' ' || c == ',')
            {
                if (cur.Length > 0)
                    list.Add(cur.ToString());
                cur.Clear();
            }
            else
                cur.Append(c);
        }
        if (cur.Length > 0)
            list.Add(cur.ToString());
        return list;
    }

    static INPUT KeyInput(ushort vk, bool ext, bool up)
    {
        uint flags = (ext ? KEYEVENTF_EXTENDEDKEY : 0) | (up ? KEYEVENTF_KEYUP : 0);
        return new INPUT
        {
            type = INPUT_KEYBOARD,
            u = new INPUTUNION { ki = new KEYBDINPUT { wVk = vk, wScan = (ushort)MapVirtualKey(vk, 0), dwFlags = flags } },
        };
    }

    static void Send(List<INPUT> inputs)
    {
        if (inputs.Count == 0)
            return;
        uint sent = SendInput((uint)inputs.Count, inputs.ToArray(), Marshal.SizeOf<INPUT>());
        if (sent != inputs.Count)
            throw new ToolException($"SendInput was blocked (sent {sent}/{inputs.Count}); is a higher-integrity window in the foreground or the workstation locked?");
    }

    public static void KeyDown(KeyChord c) => Send(ChordEvents(c, down: true, up: false));
    public static void KeyUp(KeyChord c) => Send(ChordEvents(c, down: false, up: true));

    public static void Press(KeyChord c)
    {
        // Modifiers and the key go in separate batches with a short gap: XAML keyboard accelerators and
        // access keys are more reliable when the modifier state is seen before the key.
        bool hasMods = c.Ctrl || c.Shift || c.Alt || c.Win;
        if (hasMods)
        {
            Send(ModifierEvents(c, up: false));
            Thread.Sleep(15);
        }
        Send(new List<INPUT> { KeyInput(c.Vk, c.Extended, false), KeyInput(c.Vk, c.Extended, true) });
        if (hasMods)
        {
            Thread.Sleep(15);
            Send(ModifierEvents(c, up: true));
        }
    }

    static List<INPUT> ModifierEvents(KeyChord c, bool up)
    {
        var mods = new List<(ushort, bool)>();
        if (c.Ctrl) mods.Add((0xA2, false));
        if (c.Shift) mods.Add((0xA0, false));
        if (c.Alt) mods.Add((0xA4, false));
        if (c.Win) mods.Add((0x5B, true));
        if (up)
            mods.Reverse();
        return mods.Select(m => KeyInput(m.Item1, m.Item2, up)).ToList();
    }

    static List<INPUT> ChordEvents(KeyChord c, bool down, bool up)
    {
        var list = new List<INPUT>();
        if (down)
        {
            list.AddRange(ModifierEvents(c, up: false));
            list.Add(KeyInput(c.Vk, c.Extended, false));
        }
        if (up)
        {
            list.Add(KeyInput(c.Vk, c.Extended, true));
            list.AddRange(ModifierEvents(c, up: true));
        }
        return list;
    }

    public static void TypeText(string text)
    {
        foreach (char ch in text)
        {
            if (ch == '\r')
                continue;
            if (ch == '\n')
                Press(new KeyChord(false, false, false, false, 0x0D, false));
            else if (ch == '\t')
                Press(new KeyChord(false, false, false, false, 0x09, false));
            else
            {
                Send(new List<INPUT>
                {
                    new() { type = INPUT_KEYBOARD, u = new INPUTUNION { ki = new KEYBDINPUT { wScan = ch, dwFlags = KEYEVENTF_UNICODE } } },
                    new() { type = INPUT_KEYBOARD, u = new INPUTUNION { ki = new KEYBDINPUT { wScan = ch, dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP } } },
                });
            }
            Thread.Sleep(4);
        }
    }

    // [SECTION] Mouse

    static INPUT MouseInput(uint flags, int dx = 0, int dy = 0, int data = 0) => new()
    {
        type = INPUT_MOUSE,
        u = new INPUTUNION { mi = new MOUSEINPUT { dx = dx, dy = dy, mouseData = data, dwFlags = flags } },
    };

    // Moves the real cursor to a screen position (physical pixels).
    public static void MoveTo(int x, int y)
    {
        int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vw = Math.Max(2, GetSystemMetrics(SM_CXVIRTUALSCREEN)), vh = Math.Max(2, GetSystemMetrics(SM_CYVIRTUALSCREEN));
        int nx = (int)Math.Round((x - vx) * 65535.0 / (vw - 1));
        int ny = (int)Math.Round((y - vy) * 65535.0 / (vh - 1));
        Send(new List<INPUT> { MouseInput(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, nx, ny) });
        if (GetCursorPos(out var p) && (p.X != x || p.Y != y))
            SetCursorPos(x, y);
    }

    // Moves in a few steps so the target sees PointerEntered/PointerMoved like a real user.
    public static void GlideTo(int x, int y)
    {
        if (GetCursorPos(out var p))
        {
            const int steps = 3;
            for (int i = 1; i < steps; ++i)
            {
                MoveTo(p.X + (x - p.X) * i / steps, p.Y + (y - p.Y) * i / steps);
                Thread.Sleep(10);
            }
        }
        MoveTo(x, y);
    }

    public static (uint Down, uint Up) ButtonFlags(string button) => button switch
    {
        "right" => (MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP),
        "middle" => (MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP),
        _ => (MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP),
    };

    public static void MouseDown(string button) => Send(new List<INPUT> { MouseInput(ButtonFlags(button).Down) });
    public static void MouseUp(string button) => Send(new List<INPUT> { MouseInput(ButtonFlags(button).Up) });

    public static void Click(string button, int count)
    {
        var (down, up) = ButtonFlags(button);
        for (int i = 0; i < Math.Max(1, count); ++i)
        {
            Send(new List<INPUT> { MouseInput(down) });
            Thread.Sleep(30);
            Send(new List<INPUT> { MouseInput(up) });
            if (i + 1 < count)
                Thread.Sleep(40);
        }
    }

    // Dear ImGui wheel units: dy > 0 scrolls up, dx > 0 scrolls left (one unit = one notch).
    public static void Wheel(float dx, float dy)
    {
        var list = new List<INPUT>();
        if (dy != 0)
            list.Add(MouseInput(MOUSEEVENTF_WHEEL, data: (int)Math.Round(dy * 120)));
        if (dx != 0)
            list.Add(MouseInput(MOUSEEVENTF_HWHEEL, data: (int)Math.Round(-dx * 120)));
        Send(list);
    }
}

// Background keyboard: posts WM_KEYDOWN/WM_KEYUP (WM_SYSKEY* with Alt) to a window of the gallery without
// activating it. Posted messages do not change the system key state, so the target sees Ctrl/Shift as released
// unless it tracks the posted modifier messages itself.
internal static class PostedKeys
{
    static IntPtr LParam(ushort vk, bool ext, bool up, bool alt)
    {
        uint scan = Native.MapVirtualKey(vk, 0) & 0xFF;
        uint l = 1u | (scan << 16);
        if (ext)
            l |= 1u << 24;
        if (alt)
            l |= 1u << 29;
        if (up)
            l |= (1u << 30) | (1u << 31);
        return (IntPtr)(int)l;
    }

    static void Post(IntPtr hwnd, uint msg, ushort vk, bool ext, bool up, bool alt)
    {
        if (!Native.PostMessage(hwnd, msg, (IntPtr)vk, LParam(vk, ext, up, alt)))
            throw new ToolException($"PostMessage to 0x{hwnd.ToInt64():X} failed (error {Marshal.GetLastWin32Error()})");
    }

    public static void Press(IntPtr hwnd, KeyChord c)
    {
        var mods = new List<(ushort Vk, bool Ext)>();
        if (c.Ctrl) mods.Add((0x11, false));
        if (c.Shift) mods.Add((0x10, false));
        if (c.Win) mods.Add((0x5B, true));
        bool alt = c.Alt;
        foreach (var m in mods)
            Post(hwnd, alt ? Native.WM_SYSKEYDOWN : Native.WM_KEYDOWN, m.Vk, m.Ext, false, alt);
        if (alt)
            Post(hwnd, Native.WM_SYSKEYDOWN, 0x12, false, false, true);
        Post(hwnd, alt ? Native.WM_SYSKEYDOWN : Native.WM_KEYDOWN, c.Vk, c.Extended, false, alt);
        Post(hwnd, alt ? Native.WM_SYSKEYUP : Native.WM_KEYUP, c.Vk, c.Extended, true, alt);
        if (alt)
            Post(hwnd, Native.WM_KEYUP, 0x12, false, true, false);
        for (int i = mods.Count - 1; i >= 0; --i)
            Post(hwnd, Native.WM_KEYUP, mods[i].Vk, mods[i].Ext, true, false);
    }
}
