using System.Text.Json.Nodes;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using static WinUiMcp.Native;

namespace WinUiMcp;

// Input tools: click, hover, key, type_text, mouse and run_steps.
//
// Default (background) mode never moves the mouse, never sends hardware input and never activates the gallery:
// item actions use UIA patterns (Invoke, Toggle, SelectionItem, ExpandCollapse, Value, ScrollItem, SetFocus) and keys
// are posted as WM_KEYDOWN/WM_KEYUP to the gallery's XAML input window. Real input (SendInput) and bringing the
// gallery to the front are only used when the driver was started with --interactive.
internal sealed partial class Tools
{
    bool Interactive => _g.Interactive;

    void RequireInteractive(string what)
    {
        if (!Interactive)
            throw new ToolException($"{what} needs real mouse/keyboard input: restart the driver with --interactive (it moves the mouse and brings the gallery to the front)");
    }

    JsonObject RunActionTool(string tool, JsonObject args)
    {
        JsonArray steps;
        if (tool == "run_steps")
            steps = args["steps"] as JsonArray ?? new JsonArray();
        else
        {
            var step = (JsonObject)args.DeepClone();
            step["action"] = tool switch
            {
                "click" => "click",
                "hover" => "hover",
                "key" => "key",
                "type_text" => "type",
                _ => Str(args, "action", "mouse_move"),
            };
            steps = new JsonArray(step);
        }
        string window = Str(args, "window");
        bool keyboard = Bool(args, "keyboard", false);

        var results = new JsonArray();
        var result = new JsonObject();
        string? firstError = null;
        for (int i = 0; i < steps.Count; ++i)
        {
            var o = new JsonObject();
            string err;
            if (steps[i] is not JsonObject step)
                err = "step must be an object";
            else
            {
                try { err = RunStep(step, window, ref keyboard, o); }
                catch (ToolException e) { err = e.Message; }
                catch (FlaUI.Core.Exceptions.ElementNotAvailableException) { err = "the item disappeared"; }
                catch (FlaUI.Core.Exceptions.PatternNotSupportedException e) { err = e.Message; }
                catch (System.Runtime.InteropServices.COMException e) { err = $"UI Automation error 0x{e.HResult:X8}: {e.Message}"; }
            }
            o["step"] = i;
            if (err.Length > 0)
            {
                o["error"] = err;
                results.Add(o);
                result["failed_step"] = i;
                firstError = $"step {i} ({(steps[i] as JsonObject)?["action"]}): {err}";
                break;
            }
            if (o.Count > 1)
                results.Add(o);
        }
        Thread.Sleep(150); // let the UI settle so the reported state matches the next screenshot
        if (results.Count > 0)
            result["steps"] = results;
        result["state"] = NavState();
        if (firstError != null)
        {
            result["error"] = firstError;
            return ToolText(result, isError: true);
        }
        return ToolText(result);
    }

    static readonly KeyChord EnterKey = new(false, false, false, false, 0x0D, false);
    static readonly KeyChord EscapeKey = new(false, false, false, false, 0x1B, false);

    // Executes one step. Returns an error message, or an empty string on success.
    string RunStep(JsonObject step, string window, ref bool keyboard, JsonObject output)
    {
        string action = Str(step, "action");
        string reference = Str(step, "ref");
        string stepWindow = Str(step, "window", window);
        bool needsRef = action is "click" or "hover" or "open" or "close" or "check" or "uncheck" or "focus" or "scroll_to" or "hold";
        if (needsRef && reference.Length == 0)
            return $"step '{action}' needs a 'ref'";
        if (action is "hover" or "hold" or "mouse_move" or "mouse_click" or "mouse_down" or "mouse_up" or "wheel")
            RequireInteractive($"'{action}'");
        AutomationElement? el = reference.Length > 0 ? ResolveRef(reference, stepWindow).Element : null;
        string button = Str(step, "button", "left");
        var o = _g.ClientOrigin();

        switch (action)
        {
            case "click":
                if (!Interactive)
                {
                    if (button != "left" || Bool(step, "double", false))
                        RequireInteractive(button != "left" ? $"a {button} click" : "a double click");
                    PatternClick(el!, reference);
                }
                else if (keyboard)
                    KeyboardActivate(el!);
                else
                    ClickElement(el!, button, Bool(step, "double", false));
                break;
            case "hover":
            {
                var (x, y) = ElementPoint(el!);
                _g.Activate();
                InputSim.GlideTo(x, y);
                _inputSource = "mouse";
                break;
            }
            case "hold":
            {
                var (x, y) = ElementPoint(el!);
                _g.Activate();
                InputSim.GlideTo(x, y);
                InputSim.MouseDown(button);
                Thread.Sleep((int)(Num(step, "seconds", 1) * 1000));
                InputSim.MouseUp(button);
                _inputSource = "mouse";
                break;
            }
            case "open":
            case "close":
            {
                bool open = action == "open";
                var ec = el!.Patterns.ExpandCollapse.PatternOrDefault;
                if (ec != null)
                {
                    var state = ec.ExpandCollapseState.Value;
                    if (open && state != ExpandCollapseState.Expanded)
                        ec.Expand();
                    else if (!open && state != ExpandCollapseState.Collapsed && state != ExpandCollapseState.LeafNode)
                        ec.Collapse();
                }
                else if (open && Interactive)
                    ClickElement(el, "left", false);
                else
                    return $"'{reference}' cannot be {(open ? "opened" : "closed")} (no ExpandCollapse pattern)";
                Thread.Sleep(250);
                if (!Interactive)
                    _inputSource = "automation";
                break;
            }
            case "check":
            case "uncheck":
            {
                var want = action == "check" ? ToggleState.On : ToggleState.Off;
                var tp = el!.Patterns.Toggle.PatternOrDefault;
                if (tp == null)
                {
                    if (!Interactive)
                        return $"'{reference}' has no Toggle pattern";
                    ClickElement(el, "left", false);
                    break;
                }
                for (int i = 0; i < 3 && tp.ToggleState.Value != want; ++i)
                {
                    tp.Toggle();
                    Thread.Sleep(60);
                }
                if (tp.ToggleState.Value != want)
                    return $"'{reference}' did not reach the {action}ed state";
                if (!Interactive)
                    _inputSource = "automation";
                break;
            }
            case "focus":
                _g.Activate();
                el!.Focus();
                Thread.Sleep(60);
                break;
            case "scroll_to":
                ScrollIntoView(el!);
                break;
            case "key":
            {
                string keys = Str(step, "keys");
                int count = Math.Max(1, Int(step, "count", 1));
                var chords = InputSim.SplitKeys(keys);
                if (chords.Count == 0)
                    return "step 'key' needs 'keys', e.g. \"Tab\" or \"Shift+Tab Enter\"";
                var parsed = new List<KeyChord>();
                foreach (string c in chords)
                {
                    if (!InputSim.ParseKeyChord(c, out var chord))
                        return $"unknown key '{c}'";
                    parsed.Add(chord);
                }
                for (int n = 0; n < count; ++n)
                    foreach (var chord in parsed)
                        PressKey(chord);
                _inputSource = "keyboard";
                break;
            }
            case "key_down":
            case "key_up":
            {
                RequireInteractive($"'{action}'");
                string keys = Str(step, "keys");
                if (!InputSim.ParseKeyChord(keys, out var chord))
                    return $"unknown key '{keys}'";
                _g.Activate();
                if (action == "key_down")
                    InputSim.KeyDown(chord);
                else
                    InputSim.KeyUp(chord);
                _inputSource = "keyboard";
                break;
            }
            case "type":
                TypeText(el, reference, Str(step, "text"), Bool(step, "replace", false), Bool(step, "enter", false), keyboard);
                break;
            case "mouse_move":
                _g.Activate();
                InputSim.GlideTo(o.X + (int)Math.Round(Num(step, "x", 0)), o.Y + (int)Math.Round(Num(step, "y", 0)));
                _inputSource = "mouse";
                break;
            case "mouse_click":
                _g.Activate();
                if (step.ContainsKey("x") && step.ContainsKey("y"))
                {
                    InputSim.GlideTo(o.X + (int)Math.Round(Num(step, "x", 0)), o.Y + (int)Math.Round(Num(step, "y", 0)));
                    Thread.Sleep(30);
                }
                InputSim.Click(button, Math.Max(1, Int(step, "count", 1)));
                _inputSource = "mouse";
                break;
            case "mouse_down":
            case "mouse_up":
                _g.Activate();
                if (step.ContainsKey("x") && step.ContainsKey("y"))
                    InputSim.GlideTo(o.X + (int)Math.Round(Num(step, "x", 0)), o.Y + (int)Math.Round(Num(step, "y", 0)));
                if (action == "mouse_down")
                    InputSim.MouseDown(button);
                else
                    InputSim.MouseUp(button);
                _inputSource = "mouse";
                break;
            case "wheel":
                _g.Activate();
                if (step.ContainsKey("x") && step.ContainsKey("y"))
                    InputSim.GlideTo(o.X + (int)Math.Round(Num(step, "x", 0)), o.Y + (int)Math.Round(Num(step, "y", 0)));
                InputSim.Wheel((float)Num(step, "dx", 0), (float)Num(step, "dy", 0));
                Thread.Sleep(120);
                _inputSource = "mouse";
                break;
            case "wait":
                if (step.ContainsKey("seconds"))
                    Thread.Sleep((int)Math.Clamp(Num(step, "seconds", 0) * 1000, 0, 120_000));
                else
                    Thread.Sleep(16 * Math.Clamp(Int(step, "frames", 1), 1, 7200));
                break;
            case "close_popups":
                ClosePopups();
                break;
            case "input_mode":
                keyboard = Str(step, "mode", "mouse") == "keyboard";
                break;
            default:
                return $"unknown action '{action}'";
        }
        if (Bool(step, "state", false))
        {
            Thread.Sleep(100);
            output["state"] = NavState();
        }
        return "";
    }

    // [SECTION] Keys

    void PressKey(KeyChord chord)
    {
        if (Interactive)
        {
            _g.Activate();
            InputSim.Press(chord);
        }
        else
        {
            if (chord.Ctrl || chord.Shift || chord.Win)
                throw new ToolException("Ctrl/Shift/Win chords need real keyboard input (posted messages do not change the modifier state XAML reads): restart the driver with --interactive");
            PostedKeys.Press(KeyTarget(), chord);
        }
        Thread.Sleep(Interactive ? 45 : 90);
    }

    // The window that receives XAML keyboard input: the island input window ("InputSiteWindowClass") of the focused
    // popup if one is open, otherwise of the main window.
    IntPtr KeyTarget()
    {
        var popups = FindPopups().Where(p => p.Hwnd != IntPtr.Zero && !p.Kind.Contains("tooltip") && !p.Kind.Contains("window")).ToList();
        var popup = FocusedPopup(popups) ?? popups.LastOrDefault();
        if (popup != null)
        {
            IntPtr site = FindChildByClass(popup.Hwnd, "InputSiteWindowClass");
            if (site != IntPtr.Zero)
                return site;
        }
        IntPtr main = FindChildByClass(_g.Hwnd, "InputSiteWindowClass");
        if (main == IntPtr.Zero)
            throw new ToolException("the gallery's XAML input window (InputSiteWindowClass) was not found; keyboard needs --interactive");
        return main;
    }

    // [SECTION] Text

    void TypeText(AutomationElement? el, string reference, string text, bool replace, bool enter, bool keyboard)
    {
        if (Interactive)
        {
            if (el != null)
            {
                if (keyboard)
                {
                    _g.Activate();
                    el.Focus();
                }
                else
                    ClickElement(el, "left", false);
                Thread.Sleep(60);
            }
            _g.Activate();
            if (replace)
            {
                InputSim.Press(new KeyChord(true, false, false, false, 0x41, false)); // Ctrl+A
                Thread.Sleep(30);
                if (text.Length == 0)
                    InputSim.Press(new KeyChord(false, false, false, false, 0x2E, true)); // Delete
            }
            InputSim.TypeText(text);
            if (enter)
            {
                Thread.Sleep(30);
                InputSim.Press(EnterKey);
            }
            _inputSource = "keyboard";
            return;
        }

        // Background: ValuePattern.SetValue (appends unless 'replace').
        var target = el ?? FocusedElement();
        if (target == null)
            throw new ToolException("no 'ref' and no focused text field: pass the text field as 'ref'");
        var vp = target.Patterns.Value.PatternOrDefault;
        if (vp == null)
        {
            // A label or container: use the first editable descendant.
            var edit = target.FindFirstDescendant(cf => cf.ByControlType(ControlType.Edit));
            vp = edit?.Patterns.Value.PatternOrDefault;
            if (edit != null)
                target = edit;
        }
        if (vp == null || vp.IsReadOnly.ValueOrDefault)
            throw new ToolException($"'{(reference.Length > 0 ? reference : Name(Refresh(target)))}' has no writable Value pattern; typing into it needs --interactive");
        string current = replace ? "" : vp.Value.ValueOrDefault ?? "";
        vp.SetValue(current + text);
        Thread.Sleep(60);
        if (enter)
        {
            try { target.Focus(); } catch (Exception) { }
            PressKey(EnterKey);
        }
        _inputSource = "keyboard";
    }

    // [SECTION] Pattern actions (background mode)

    // The UIA equivalent of a left click: Invoke, Toggle, SelectionItem.Select (closing the combo box it belongs
    // to) or ExpandCollapse. Labels without a pattern (text inside a check box) act on the nearest ancestor that has one.
    void PatternClick(AutomationElement el, string reference)
    {
        ScrollIntoView(el);
        var walker = _g.Automation.TreeWalkerFactory.GetControlViewWalker();
        var cur = el;
        for (int depth = 0; depth < 4 && cur != null; ++depth)
        {
            var p = cur.Patterns;
            if (p.Invoke.PatternOrDefault is { } inv)
            {
                inv.Invoke();
                break;
            }
            if (p.Toggle.PatternOrDefault is { } tg)
            {
                tg.Toggle();
                break;
            }
            if (p.SelectionItem.PatternOrDefault is { } si)
            {
                si.Select();
                CollapseOwningComboBox(cur);
                break;
            }
            if (p.ExpandCollapse.PatternOrDefault is { } ec)
            {
                var state = ec.ExpandCollapseState.ValueOrDefault;
                if (state == ExpandCollapseState.Expanded || state == ExpandCollapseState.PartiallyExpanded)
                    ec.Collapse();
                else if (state == ExpandCollapseState.Collapsed)
                    ec.Expand();
                else
                    throw new ToolException($"'{reference}' is a leaf node and has no Invoke pattern");
                break;
            }
            try { cur = walker.GetParent(cur); }
            catch (Exception) { cur = null; }
            if (depth == 3 || cur == null)
                throw new ToolException($"'{reference}' supports no Invoke/Toggle/SelectionItem/ExpandCollapse pattern; a real click needs --interactive");
        }
        _inputSource = "automation";
        Thread.Sleep(150); // let flyouts and pages react before the next step
    }

    // Clicking a combo box item selects it and closes the drop-down; Select() alone leaves it open.
    void CollapseOwningComboBox(AutomationElement item)
    {
        var walker = _g.Automation.TreeWalkerFactory.GetControlViewWalker();
        try
        {
            var parent = walker.GetParent(item);
            for (int i = 0; i < 3 && parent != null; ++i)
            {
                if (parent.Properties.ControlType.ValueOrDefault == ControlType.ComboBox)
                {
                    if (parent.Patterns.ExpandCollapse.PatternOrDefault is { } ec && ec.ExpandCollapseState.ValueOrDefault == ExpandCollapseState.Expanded)
                        ec.Collapse();
                    return;
                }
                parent = walker.GetParent(parent);
            }
        }
        catch (Exception) { }
    }

    // Closes open popups: collapse the expanded opener (combo box, drop-down button, menu bar item), otherwise send
    // Escape (posted in background mode).
    void ClosePopups()
    {
        var cf = _g.Automation.ConditionFactory;
        int quiet = 0;
        for (int i = 0; i < 12 && quiet < 2; ++i)
        {
            var popups = FindPopups().Where(p => !p.Kind.Contains("window") && !p.Kind.Contains("tooltip")).ToList();
            if (popups.Count == 0)
            {
                ++quiet;
                Thread.Sleep(120);
                continue;
            }
            quiet = 0;
            bool collapsed = false;
            if (!Interactive)
            {
                try
                {
                    var openers = _g.MainWindow.FindAll(FlaUI.Core.Definitions.TreeScope.Descendants,
                        new FlaUI.Core.Conditions.PropertyCondition(P.Expand.ExpandCollapseState, ExpandCollapseState.Expanded));
                    foreach (var e in openers.Reverse())
                    {
                        string cls = e.Properties.ClassName.ValueOrDefault ?? "";
                        var ct = e.Properties.ControlType.ValueOrDefault;
                        if (cls.Contains("NavigationViewItem") || cls.Contains("Expander") || cls.Contains("TreeViewItem") || ct == ControlType.TreeItem)
                            continue;
                        if (e.Patterns.ExpandCollapse.PatternOrDefault is { } ec)
                        {
                            ec.Collapse();
                            collapsed = true;
                            break;
                        }
                    }
                }
                catch (Exception) { }
            }
            if (!collapsed)
                PressKey(EscapeKey);
            Thread.Sleep(220);
        }
        _inputSource = Interactive ? "keyboard" : "automation";
    }

    // [SECTION] Real input helpers (--interactive)

    // Scrolls an element into view (ScrollItem pattern) when it is off screen or clipped by the window.
    void ScrollIntoView(AutomationElement el)
    {
        var client = ClientScreenRect();
        var live = Refresh(el);
        bool visible;
        using (ItemCache().Activate())
            visible = IsVisible(live, client) && client.Contains(Rect(live));
        if (visible)
            return;
        if (el.Patterns.ScrollItem.PatternOrDefault is { } si)
        {
            try
            {
                si.ScrollIntoView();
                Thread.Sleep(250);
            }
            catch (Exception) { }
        }
    }

    // Screen point to click: the center of the element's visible part.
    (int X, int Y) ElementPoint(AutomationElement el)
    {
        ScrollIntoView(el);
        // Wait for open/scroll animations (popups slide in) until the rectangle stops moving.
        System.Drawing.Rectangle r = System.Drawing.Rectangle.Empty;
        for (int i = 0; i < 12; ++i)
        {
            System.Drawing.Rectangle cur;
            var live = Refresh(el);
            using (ItemCache().Activate())
                cur = Rect(live);
            if (i > 0 && cur == r)
                break;
            r = cur;
            Thread.Sleep(50);
        }
        var client = ClientScreenRect();
        var vis = System.Drawing.Rectangle.Intersect(r, client);
        if (r.Width <= 0 || r.Height <= 0)
            throw new ToolException("the item has no on-screen rectangle (collapsed or virtualized?)");
        if (vis.Width <= 0 || vis.Height <= 0)
        {
            // Popups may extend past the main window; use the element's own rectangle then.
            vis = r;
        }
        return (vis.X + vis.Width / 2, vis.Y + vis.Height / 2);
    }

    void ClickElement(AutomationElement el, string button, bool dbl)
    {
        RequireInteractive("a real mouse click");
        var (x, y) = ElementPoint(el);
        _g.Activate();
        InputSim.GlideTo(x, y);
        Thread.Sleep(40);
        InputSim.Click(button, dbl ? 2 : 1);
        _inputSource = "mouse";
        Thread.Sleep(100); // let flyouts and pages react before the next step
    }

    // Keyboard equivalent of a click: focus the element, then Space (Enter for links and menu items).
    void KeyboardActivate(AutomationElement el)
    {
        _g.Activate();
        ScrollIntoView(el);
        el.Focus();
        Thread.Sleep(60);
        var type = el.Properties.ControlType.ValueOrDefault;
        ushort vk = type is ControlType.Hyperlink or ControlType.MenuItem or ControlType.SplitButton ? (ushort)0x0D : (ushort)0x20;
        PressKey(new KeyChord(false, false, false, false, vk, false));
        _inputSource = "keyboard";
    }
}
