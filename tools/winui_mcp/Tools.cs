using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using FlaUI.Core;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Conditions;
using FlaUI.Core.Definitions;
using FlaUI.Core.Identifiers;
using static WinUiMcp.Native;

namespace WinUiMcp;

// A search root for list_items / ref resolution: one or more UIA elements plus the window name they belong to.
internal sealed record SearchRoot(List<AutomationElement> Elements, string WindowName, System.Drawing.Rectangle? Clip = null);

// An open popup: a separate top-level HWND of the gallery process, or an in-window XAML popup element.
internal sealed record PopupInfo(string Name, string Id, AutomationElement Element, IntPtr Hwnd, List<string> Kind, System.Drawing.Rectangle Rect);

// Tool implementations. Every method runs on the STA thread (see StaDispatcher).
internal sealed partial class Tools
{
    readonly Gallery _g;
    string _inputSource = "keyboard";
    CacheRequest? _itemCache;

    public Tools(Gallery gallery)
    {
        _g = gallery;
    }

    PropertyLibraryShortcut P => new(_g.Automation);

    internal readonly struct PropertyLibraryShortcut
    {
        readonly FlaUI.UIA3.UIA3Automation _a;
        public PropertyLibraryShortcut(FlaUI.UIA3.UIA3Automation a) => _a = a;
        public FlaUI.Core.AutomationElements.Infrastructure.IAutomationElementPropertyIds E => _a.PropertyLibrary.Element;
        public FlaUI.Core.AutomationElements.Infrastructure.IAutomationElementPatternAvailabilityPropertyIds Avail => _a.PropertyLibrary.PatternAvailability;
        public FlaUI.Core.Patterns.ITogglePatternPropertyIds Toggle => _a.PropertyLibrary.Toggle;
        public FlaUI.Core.Patterns.IExpandCollapsePatternPropertyIds Expand => _a.PropertyLibrary.ExpandCollapse;
        public FlaUI.Core.Patterns.ISelectionItemPatternPropertyIds Sel => _a.PropertyLibrary.SelectionItem;
        public FlaUI.Core.Patterns.IValuePatternPropertyIds Value => _a.PropertyLibrary.Value;
    }

    // [SECTION] Dispatch

    public JsonObject Call(string name, JsonObject args)
    {
        // Background mode: nothing this call does (UIA SetFocus, Invoke, popups) may activate the gallery.
        using var foregroundLock = new ForegroundLock(_g.UsesForegroundLock);
        try
        {
            _g.EnsureConnected();
            return name switch
            {
                "app_info" => ToolText(AppInfo()),
                "list_pages" => ToolText(ListPages()),
                "goto_page" => GotoPageTool(args),
                "set_theme" => SetThemeTool(args),
                "resize_window" => ResizeWindowTool(args),
                "screenshot" => ScreenshotTool(args),
                "list_windows" => ToolText(ListWindows()),
                "nav_state" => ToolText(NavState()),
                "list_items" => ToolText(ListItems(args)),
                "item_info" => ToolText(ItemInfo(args)),
                "click" or "hover" or "key" or "type_text" or "mouse" or "run_steps" => RunActionTool(name, args),
                "run_tests" => ToolError("run_tests: not supported on the WinUI driver (it drives the real WinUI 3 Gallery; there is no test registry)"),
                _ => ToolError($"unknown tool '{name}'"),
            };
        }
        catch (ToolException e)
        {
            return ToolError($"{name}: {e.Message}");
        }
        catch (COMException e)
        {
            return ToolError($"{name}: UI Automation error 0x{e.HResult:X8}: {e.Message}");
        }
        catch (Exception e) when (e is FlaUI.Core.Exceptions.ElementNotAvailableException || e is FlaUI.Core.Exceptions.PropertyNotSupportedException || e is FlaUI.Core.Exceptions.PatternNotSupportedException)
        {
            return ToolError($"{name}: {e.Message}");
        }
        catch (Exception e)
        {
            return ToolError($"{name}: {e.GetType().Name}: {e.Message}");
        }
    }

    static readonly JsonSerializerOptions s_Indent1 = new() { WriteIndented = true, IndentSize = 1, Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping };
    static readonly JsonSerializerOptions s_Compact = new() { Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping };

    public static JsonObject ToolText(JsonNode payload, bool isError = false) => new()
    {
        ["content"] = new JsonArray(new JsonObject { ["type"] = "text", ["text"] = payload.ToJsonString(s_Indent1) }),
        ["isError"] = isError,
    };

    public static JsonObject ToolError(string message) => new()
    {
        ["content"] = new JsonArray(new JsonObject { ["type"] = "text", ["text"] = message }),
        ["isError"] = true,
    };

    static string Str(JsonObject a, string key, string def = "") => a[key] is JsonValue v && v.TryGetValue(out string? s) ? s ?? def : def;
    static bool Bool(JsonObject a, string key, bool def) => a[key] is JsonValue v && v.TryGetValue(out bool b) ? b : def;
    static double Num(JsonObject a, string key, double def)
    {
        if (a[key] is not JsonValue v)
            return def;
        if (v.TryGetValue(out double d)) return d;
        if (v.TryGetValue(out int i)) return i;
        if (v.TryGetValue(out long l)) return l;
        if (v.TryGetValue(out string? s) && double.TryParse(s, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out d)) return d;
        return def;
    }
    static int Int(JsonObject a, string key, int def) => (int)Math.Round(Num(a, key, def));

    // [SECTION] Element properties (cached when a CacheRequest is active)

    CacheRequest ItemCache()
    {
        if (_itemCache != null)
            return _itemCache;
        var p = P;
        var cr = new CacheRequest { TreeScope = TreeScope.Element, AutomationElementMode = AutomationElementMode.Full };
        foreach (var id in new PropertyId[]
        {
            p.E.Name, p.E.AutomationId, p.E.ControlType, p.E.ClassName, p.E.BoundingRectangle, p.E.IsOffscreen, p.E.IsEnabled,
            p.E.HasKeyboardFocus, p.E.RuntimeId, p.E.IsPassword, p.E.NativeWindowHandle, p.E.LocalizedControlType, p.E.ProcessId,
            p.Avail.IsTogglePatternAvailable, p.Avail.IsExpandCollapsePatternAvailable, p.Avail.IsSelectionItemPatternAvailable, p.Avail.IsValuePatternAvailable,
            p.Toggle.ToggleState, p.Expand.ExpandCollapseState, p.Sel.IsSelected, p.Value.Value, p.Value.IsReadOnly,
        })
            cr.Add(id);
        _itemCache = cr;
        return cr;
    }

    CacheRequest SubtreeCache(int depth)
    {
        var src = ItemCache();
        var cr = new CacheRequest { TreeScope = depth == 1 ? TreeScope.Element | TreeScope.Children : TreeScope.Subtree, AutomationElementMode = AutomationElementMode.Full };
        foreach (var id in src.Properties)
            cr.Add(id);
        cr.TreeFilter = new PropertyCondition(P.E.IsControlElement, true);
        return cr;
    }

    static T Get<T>(AutomationElement e, PropertyId id, T def)
    {
        try
        {
            if (e.FrameworkAutomationElement.TryGetPropertyValue(id, out T? value) && value != null)
                return value;
        }
        catch (Exception) { }
        return def;
    }

    string Name(AutomationElement e) => Get(e, P.E.Name, "");
    string AutomationId(AutomationElement e) => Get(e, P.E.AutomationId, "");
    ControlType Type(AutomationElement e) => Get(e, P.E.ControlType, ControlType.Unknown);
    System.Drawing.Rectangle Rect(AutomationElement e) => Get(e, P.E.BoundingRectangle, System.Drawing.Rectangle.Empty);
    int[] RuntimeId(AutomationElement e) => Get(e, P.E.RuntimeId, Array.Empty<int>());

    string ElementId(AutomationElement e, bool forceRuntimeId = false)
    {
        string aid = AutomationId(e);
        if (aid.Length > 0 && !forceRuntimeId)
            return aid;
        int[] rt = RuntimeId(e);
        return rt.Length > 0 ? "rt:" + string.Join('.', rt) : "";
    }

    static string TypeName(ControlType t) => t == ControlType.Unknown ? "Unknown" : t.ToString();

    bool IsVisible(AutomationElement e, System.Drawing.Rectangle clientScreen)
    {
        var r = Rect(e);
        if (r.Width <= 0 || r.Height <= 0 || Get(e, P.E.IsOffscreen, false))
            return false;
        return r.IntersectsWith(clientScreen);
    }

    // Has a rectangle and is not off screen (for popups, which may extend past the main window).
    bool IsShown(AutomationElement e)
    {
        var r = Rect(e);
        return r.Width > 0 && r.Height > 0 && !Get(e, P.E.IsOffscreen, false);
    }

    System.Drawing.Rectangle ClientScreenRect()
    {
        var o = _g.ClientOrigin();
        var (w, h) = _g.ClientSize();
        return new System.Drawing.Rectangle(o.X, o.Y, w, h);
    }

    JsonArray Flags(AutomationElement e)
    {
        var p = P;
        var flags = new JsonArray();
        if (Get(e, p.E.HasKeyboardFocus, false))
            flags.Add("focused");
        if (!Get(e, p.E.IsEnabled, true))
            flags.Add("disabled");
        if (Get(e, p.Avail.IsTogglePatternAvailable, false))
        {
            var ts = Get(e, p.Toggle.ToggleState, ToggleState.Off);
            flags.Add(ts == ToggleState.On ? "checked" : ts == ToggleState.Indeterminate ? "indeterminate" : "unchecked");
        }
        if (Get(e, p.Avail.IsExpandCollapsePatternAvailable, false))
        {
            var es = Get(e, p.Expand.ExpandCollapseState, ExpandCollapseState.LeafNode);
            if (es == ExpandCollapseState.Expanded || es == ExpandCollapseState.PartiallyExpanded)
                flags.Add("opened");
            else if (es == ExpandCollapseState.Collapsed)
                flags.Add("closed");
        }
        if (Get(e, p.Avail.IsSelectionItemPatternAvailable, false) && Get(e, p.Sel.IsSelected, false))
            flags.Add("selected");
        if (Get(e, p.E.IsOffscreen, false))
            flags.Add("offscreen");
        var type = Type(e);
        if (type == ControlType.Edit || type == ControlType.Document || (Get(e, p.Avail.IsValuePatternAvailable, false) && !Get(e, p.Value.IsReadOnly, true) && type != ControlType.ComboBox && type != ControlType.Slider && type != ControlType.Spinner))
            flags.Add("inputable");
        return flags;
    }

    JsonObject ItemToJson(AutomationElement e, string? window = null, bool forceRuntimeId = false)
    {
        var j = new JsonObject
        {
            ["id"] = ElementId(e, forceRuntimeId),
            ["label"] = Name(e),
            ["type"] = TypeName(Type(e)),
            ["rect"] = _g.ToClientRect(Rect(e)),
        };
        var flags = Flags(e);
        if (flags.Count > 0)
            j["flags"] = flags;
        if (Get(e, P.Avail.IsValuePatternAvailable, false) && !Get(e, P.E.IsPassword, false))
        {
            string v = Get(e, P.Value.Value, "");
            if (v.Length > 0)
                j["value"] = v.Length > 200 ? v[..200] + "..." : v;
        }
        if (!string.IsNullOrEmpty(window))
            j["window"] = window;
        return j;
    }

    // Re-reads an element with all item properties in one round trip.
    AutomationElement Refresh(AutomationElement e)
    {
        try
        {
            using (ItemCache().Activate())
                return e.FindFirst(TreeScope.Element, TrueCondition.Default) ?? e;
        }
        catch (Exception)
        {
            return e;
        }
    }

    JsonObject LiveItemJson(AutomationElement e, string? window = null)
    {
        var cached = Refresh(e);
        using (ItemCache().Activate())
            return ItemToJson(cached, window);
    }

    // [SECTION] Search roots

    AutomationElement? _navView;

    AutomationElement? NavView()
    {
        try
        {
            if (_navView != null && Get(_navView, P.E.AutomationId, "") == "NavigationViewControl")
                return _navView;
        }
        catch (Exception) { }
        _navView = _g.MainWindow.FindFirstDescendant(cf => cf.ByAutomationId("NavigationViewControl"));
        return _navView;
    }

    // The current page: the NavigationView content (its children except the navigation pane).
    SearchRoot PageRoot()
    {
        var nav = NavView();
        if (nav == null)
            return new SearchRoot(new List<AutomationElement> { _g.MainWindow }, MainWindowName);
        var list = new List<AutomationElement>();
        foreach (var c in nav.FindAllChildren())
        {
            string aid = AutomationId(c);
            if (aid is "PaneRoot" or "TogglePaneButton" or "PaneAutoSuggestBoxPresenter" or "NavigationViewBackButton" or "NavigationViewCloseButton" or "TopNavArea" or "PaneHeaderOnTopPane" or "PaneCustomContentOnTopPane" or "TopNavMenuItemsHost" or "TopFooterMenuItemsHost")
                continue;
            list.Add(c);
        }
        if (list.Count == 0)
            list.Add(nav);
        return new SearchRoot(list, MainWindowName);
    }

    string MainWindowName => WindowText(_g.Hwnd) is { Length: > 0 } t ? t : "WinUI 3 Gallery";

    SearchRoot MainRoot() => new(new List<AutomationElement> { _g.MainWindow }, MainWindowName);

    // "" -> current page, "$FOCUSED" -> focused popup (else the page), "$MAIN" or the main window title -> whole window,
    // otherwise a window/popup name or id from list_windows.
    SearchRoot ResolveWindow(string window)
    {
        if (string.IsNullOrEmpty(window) || window.Equals("$PAGE", StringComparison.OrdinalIgnoreCase))
            return PageRoot();
        if (window.Equals("$MAIN", StringComparison.OrdinalIgnoreCase) || window.Equals("$WINDOW", StringComparison.OrdinalIgnoreCase) || window == MainWindowName || window.Equals("main", StringComparison.OrdinalIgnoreCase))
            return MainRoot();
        var popups = FindPopups();
        if (window.Equals("$FOCUSED", StringComparison.OrdinalIgnoreCase))
        {
            var focused = FocusedPopup(popups) ?? popups.LastOrDefault(p => !p.Kind.Contains("tooltip")) ?? popups.LastOrDefault();
            return focused != null ? PopupRoot(focused) : PageRoot();
        }
        var match = popups.FirstOrDefault(p => p.Name == window || p.Id == window) ?? popups.FirstOrDefault(p => p.Name.Equals(window, StringComparison.OrdinalIgnoreCase));
        if (match != null)
            return PopupRoot(match);
        throw new ToolException($"no window named '{window}' (see list_windows; use \"$FOCUSED\" for the open popup or \"$MAIN\" for the whole window)");
    }

    // XAML popup islands also expose the page behind the light-dismiss layer, so popup searches keep only the
    // elements that sit inside the popup's rectangle.
    static SearchRoot PopupRoot(PopupInfo p) => new(new List<AutomationElement> { p.Element }, p.Name, p.Rect);

    static bool InsideClip(System.Drawing.Rectangle r, System.Drawing.Rectangle? clip)
    {
        if (clip == null)
            return true;
        int cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2;
        return clip.Value.Contains(cx, cy);
    }

    // [SECTION] Popups

    // Popups: the process's other visible top-level windows (WinUI windowed popups use a
    // "Microsoft.UI.Content.PopupWindowSiteBridge" HWND: combo box lists, flyouts, menus, tooltips) plus in-window
    // XAML popups (class "Popup", Menu or ToolTip directly under the main XAML island root). Topmost last.
    public List<PopupInfo> FindPopups()
    {
        var list = new List<PopupInfo>();
        if (_g.Process == null)
            return list;
        var seen = new HashSet<string>();
        var hwnds = ProcessWindows(_g.Process.Id);
        hwnds.Reverse(); // bottom-most first
        foreach (IntPtr h in hwnds)
        {
            if (h == _g.Hwnd)
                continue;
            GetWindowRect(h, out RECT r);
            if (r.Width <= 0 || r.Height <= 0)
                continue;
            string cls = ClassName(h);
            // The text input framework's indicator windows ("UAC Input Indicator") belong to the gallery process on
            // a desktop of its own; they are not gallery popups.
            if (cls.StartsWith("UAC_", StringComparison.Ordinal) || WindowText(h).StartsWith("UAC ", StringComparison.Ordinal))
                continue;
            bool appWindow = GetWindow(h, GW_OWNER) == IntPtr.Zero && cls == "WinUIDesktopWin32WindowClass";
            AutomationElement el;
            try { el = _g.Automation.FromHandle(h); }
            catch (Exception) { continue; }
            var rect = new System.Drawing.Rectangle(r.Left, r.Top, r.Width, r.Height);
            var (name, kind) = appWindow ? (WindowText(h), "window") : DescribePopup(el, rect);
            if (name.Length == 0)
                name = WindowText(h).Length > 0 ? WindowText(h) : cls;
            if (name.StartsWith("UAC ", StringComparison.Ordinal) || name.StartsWith("UAC_", StringComparison.Ordinal))
                continue;
            string id = $"0x{h.ToInt64():X8}";
            if (seen.Add(id))
                list.Add(new PopupInfo(name, id, el, h, new List<string> { kind }, rect));
        }

        // In-window popups live directly under the main XAML island root.
        try
        {
            var islandRoots = new List<AutomationElement>();
            foreach (var bridge in _g.MainWindow.FindAllChildren(cf => cf.ByClassName("Microsoft.UI.Content.DesktopChildSiteBridge")))
                islandRoots.AddRange(bridge.FindAllChildren());
            foreach (var root in islandRoots)
            {
                AutomationElement[] children;
                using (ItemCache().Activate())
                    children = root.FindAllChildren();
                foreach (var c in children)
                {
                    string cls;
                    ControlType ct;
                    System.Drawing.Rectangle rect;
                    string eid;
                    using (ItemCache().Activate())
                    {
                        cls = Get(c, P.E.ClassName, "");
                        ct = Type(c);
                        rect = Rect(c);
                        eid = ElementId(c);
                    }
                    if (cls != "Popup" && ct != ControlType.Menu && ct != ControlType.ToolTip)
                        continue;
                    if (rect.Width <= 0 || rect.Height <= 0)
                        continue;
                    var (name, kind) = DescribePopup(c, rect);
                    if (name.Length == 0)
                        name = "Popup";
                    if (seen.Add(eid))
                        list.Add(new PopupInfo(name, eid, c, IntPtr.Zero, new List<string> { kind }, rect));
                }
            }
        }
        catch (Exception) { }
        return list;
    }

    // Name and kind of a popup from its content: a menu, a tooltip, or the control whose drop-down it is.
    (string Name, string Kind) DescribePopup(AutomationElement popup, System.Drawing.Rectangle rect)
    {
        var cf = _g.Automation.ConditionFactory;
        try
        {
            using (ItemCache().Activate())
            {
                var hits = popup.FindAll(TreeScope.Subtree, cf.ByControlType(ControlType.Menu).Or(cf.ByControlType(ControlType.ToolTip))
                    .Or(new PropertyCondition(P.Expand.ExpandCollapseState, ExpandCollapseState.Expanded)));
                foreach (var e in hits)
                {
                    var ct = Type(e);
                    if (ct == ControlType.ToolTip)
                        return (Name(e).Length > 0 ? Name(e) : "ToolTip", "tooltip");
                    if (ct == ControlType.Menu && InsideClip(Rect(e), rect))
                        return (Name(e).Length > 0 ? Name(e) : "Menu", "menu");
                }
                foreach (var e in hits)
                    if (Type(e) != ControlType.Menu && Name(e).Length > 0)
                        return ($"{Name(e)} popup", "popup");
                // A flyout: name it after its first labeled element inside the popup rectangle.
                foreach (var e in popup.FindAll(TreeScope.Descendants, new PropertyCondition(P.E.IsControlElement, true)))
                {
                    string aid = AutomationId(e);
                    if (Name(e).Length > 0 && aid != "Light Dismiss" && Get(e, P.E.ClassName, "") != "Popup" && InsideClip(Rect(e), rect))
                        return (Name(e).Length > 40 ? Name(e)[..40] : Name(e), "popup");
                }
            }
        }
        catch (Exception) { }
        return ("", "popup");
    }

    PopupInfo? FocusedPopup(List<PopupInfo> popups)
    {
        if (popups.Count == 0)
            return null;
        var focused = FocusedElement();
        if (focused == null)
            return null;
        return PopupOf(focused, popups);
    }

    // The popup that contains an element: by hosting HWND, by UIA ancestry, then by geometry (topmost popup
    // whose rectangle contains the element's center; combo box items are UIA children of the combo box).
    PopupInfo? PopupOf(AutomationElement e, List<PopupInfo> popups)
    {
        if (popups.Count == 0)
            return null;
        IntPtr hwnd = TopLevelHwndOf(e);
        foreach (var p in popups)
            if (p.Hwnd != IntPtr.Zero && p.Hwnd == hwnd)
                return p;
        var walker = _g.Automation.TreeWalkerFactory.GetControlViewWalker();
        var cur = e;
        for (int i = 0; i < 64 && cur != null; ++i)
        {
            foreach (var p in popups)
                if (p.Hwnd == IntPtr.Zero && SameElement(p.Element, cur))
                    return p;
            try { cur = walker.GetParent(cur); }
            catch (Exception) { break; }
        }
        System.Drawing.Rectangle r;
        using (ItemCache().Activate())
            r = Rect(Refresh(e));
        if (r.Width <= 0 || r.Height <= 0)
            return null;
        for (int i = popups.Count - 1; i >= 0; --i)
            if (!popups[i].Kind.Contains("tooltip") && !popups[i].Kind.Contains("window") && InsideClip(r, popups[i].Rect))
                return popups[i];
        return null;
    }

    bool SameElement(AutomationElement a, AutomationElement b)
    {
        try { return _g.Automation.Compare(a, b); }
        catch (Exception) { return false; }
    }

    // The top-level HWND that hosts an element (walks up to the first element with a native window handle).
    IntPtr TopLevelHwndOf(AutomationElement e)
    {
        var walker = _g.Automation.TreeWalkerFactory.GetRawViewWalker();
        var cur = e;
        for (int i = 0; i < 64 && cur != null; ++i)
        {
            IntPtr h = Get(cur, P.E.NativeWindowHandle, IntPtr.Zero);
            if (h != IntPtr.Zero)
            {
                IntPtr root = GetAncestorRoot(h);
                return root != IntPtr.Zero ? root : h;
            }
            try { cur = walker.GetParent(cur); }
            catch (Exception) { break; }
        }
        return IntPtr.Zero;
    }

    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr hwnd, uint flags);
    static IntPtr GetAncestorRoot(IntPtr h) => GetAncestor(h, 2 /* GA_ROOT */);

    // [SECTION] Focus

    // The UIA focused element when it belongs to the gallery. When another application is in the foreground,
    // fall back to the gallery element that still reports HasKeyboardFocus (XAML keeps its logical focus).
    AutomationElement? FocusedElement()
    {
        try
        {
            var f = _g.Automation.FocusedElement();
            if (f != null)
            {
                f = Refresh(f);
                int pid;
                using (ItemCache().Activate())
                    pid = Get(f, P.E.ProcessId, 0);
                if (_g.Process == null || pid == _g.Process.Id)
                    return f;
            }
            var cond = new PropertyCondition(P.E.HasKeyboardFocus, true);
            var hit = _g.MainWindow.FindFirst(TreeScope.Descendants, cond);
            if (hit == null && _g.Process != null)
                foreach (IntPtr h in ProcessWindows(_g.Process.Id))
                    if (h != _g.Hwnd && (hit = _g.Automation.FromHandle(h).FindFirst(TreeScope.Descendants, cond)) != null)
                        break;
            return hit == null ? null : Refresh(hit);
        }
        catch (Exception)
        {
            return null;
        }
    }

    public JsonObject NavState()
    {
        var j = new JsonObject();
        var popups = FindPopups();
        var focused = FocusedElement();
        var focus = new JsonObject();
        bool textInput = false;
        if (focused != null)
        {
            using (ItemCache().Activate())
            {
                var type = Type(focused);
                focus["id"] = ElementId(focused);
                focus["label"] = Name(focused);
                focus["type"] = TypeName(type);
                focus["rect"] = _g.ToClientRect(Rect(focused));
                focus["automation_id"] = AutomationId(focused);
                var flags = Flags(focused);
                flags.Remove(flags.FirstOrDefault(n => n?.GetValue<string>() == "focused"));
                if (flags.Count > 0)
                    focus["flags"] = flags;
                textInput = type == ControlType.Edit || type == ControlType.Document;
            }
            var popup = PopupOf(focused, popups);
            focus["window"] = popup?.Name ?? MainWindowName;
        }
        else
        {
            focus["id"] = "";
            focus["label"] = "";
            focus["window"] = _g.IsForeground ? MainWindowName : "";
        }
        j["focus"] = focus;
        if (popups.Count > 0)
        {
            var arr = new JsonArray();
            foreach (var p in popups)
                arr.Add(new JsonObject
                {
                    ["id"] = p.Id,
                    ["window"] = p.Name,
                    ["kind"] = new JsonArray(p.Kind.Select(k => (JsonNode)k).ToArray()),
                    ["rect"] = _g.ToClientRect(p.Rect),
                });
            j["popups"] = arr;
        }
        j["input_source"] = _inputSource;
        if (textInput)
            j["text_input"] = true;
        if (!_g.IsForeground)
            j["foreground"] = false;
        return j;
    }

    // [SECTION] list_windows

    public JsonArray ListWindows()
    {
        var arr = new JsonArray();
        var popups = FindPopups();
        var focusedPopup = FocusedPopup(popups);
        var (w, h) = _g.ClientSize();
        var main = new JsonObject
        {
            ["name"] = MainWindowName,
            ["id"] = $"0x{_g.Hwnd.ToInt64():X8}",
            ["rect"] = new JsonArray(0, 0, w, h),
            ["kind"] = new JsonArray(focusedPopup == null && _g.IsForeground ? new JsonNode[] { "main", "focused" } : new JsonNode[] { "main" }),
        };
        arr.Add(main);
        foreach (var p in popups)
        {
            var kind = new JsonArray(p.Kind.Select(k => (JsonNode)k).ToArray());
            if (p.Hwnd != IntPtr.Zero)
                kind.Add("hwnd");
            if (focusedPopup == p)
                kind.Add("focused");
            arr.Add(new JsonObject
            {
                ["name"] = p.Name,
                ["id"] = p.Id,
                ["rect"] = _g.ToClientRect(p.Rect),
                ["kind"] = kind,
                ["parent"] = MainWindowName,
            });
        }
        return arr;
    }

    // [SECTION] list_items / item_info

    public JsonObject ListItems(JsonObject args)
    {
        string window = Str(args, "window");
        string reference = Str(args, "ref");
        int depth = Int(args, "depth", -1);
        bool visibleOnly = Bool(args, "visible_only", true);
        bool labeledOnly = Bool(args, "labeled_only", true);
        string filter = Str(args, "filter");
        int max = Int(args, "max", 400);

        SearchRoot root = ResolveWindow(window);
        List<AutomationElement> starts;
        if (reference.Length > 0)
        {
            var (el, _) = ResolveRef(reference, window);
            starts = new List<AutomationElement> { el };
        }
        else
            starts = root.Elements;

        var client = ClientScreenRect();
        var items = new JsonArray();
        int skipped = 0;
        var walked = new List<AutomationElement>();
        var aidCount = new Dictionary<string, int>();
        var cr = SubtreeCache(depth);
        using (cr.Activate())
        {
            foreach (var start in starts)
            {
                AutomationElement? cached;
                try { cached = start.FindFirst(TreeScope.Element, TrueCondition.Default); }
                catch (Exception) { continue; }
                if (cached == null)
                    continue;
                bool includeSelf = reference.Length == 0 && starts.Count > 1;
                Walk(cached, 0, includeSelf);
            }
            foreach (var e in walked)
                Consider(e);
        }

        void Walk(AutomationElement e, int level, bool includeSelf)
        {
            if (includeSelf)
            {
                walked.Add(e);
                string aid = AutomationId(e);
                if (aid.Length > 0)
                    aidCount[aid] = aidCount.GetValueOrDefault(aid) + 1;
            }
            if (depth >= 0 && level >= depth)
                return;
            AutomationElement[] children;
            try { children = e.CachedChildren; }
            catch (Exception) { return; }
            foreach (var c in children)
                Walk(c, level + 1, true);
        }

        void Consider(AutomationElement e)
        {
            string label = Name(e);
            if (root.Clip != null && !InsideClip(Rect(e), root.Clip))
                return;
            if (visibleOnly && !(root.Clip != null ? IsShown(e) : IsVisible(e, client)))
                return;
            if (labeledOnly && label.Length == 0)
                return;
            string aid = AutomationId(e);
            if (visibleOnly && aid.StartsWith("__", StringComparison.Ordinal))
                return; // the gallery's invisible test-automation helpers (__CurrentPage, __TestContentLoadedCheckBox, ...)
            if (filter.Length > 0 && label.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0 && aid.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0)
                return;
            if (items.Count >= max)
            {
                ++skipped;
                return;
            }
            // Template parts repeat AutomationIds ("TitleText" in every card): use the runtime id for those.
            items.Add(ItemToJson(e, window.Length > 0 ? root.WindowName : null, forceRuntimeId: aidCount.GetValueOrDefault(aid) > 1));
        }

        var result = new JsonObject { ["items"] = items };
        if (skipped > 0)
            result["truncated"] = skipped;
        return result;
    }

    public JsonObject ItemInfo(JsonObject args)
    {
        string reference = Str(args, "ref");
        if (reference.Length == 0)
            throw new ToolException("'ref' is required");
        AutomationElement el;
        string? windowName;
        try
        {
            (el, windowName) = ResolveRef(reference, Str(args, "window"));
        }
        catch (ToolException)
        {
            return new JsonObject { ["error"] = "item not found" };
        }
        var item = LiveItemJson(el, windowName);
        var cached = Refresh(el);
        using (ItemCache().Activate())
        {
            item["automation_id"] = AutomationId(cached);
            item["class"] = Get(cached, P.E.ClassName, "");
            item["localized_type"] = Get(cached, P.E.LocalizedControlType, "");
        }
        var patterns = new JsonArray();
        try
        {
            foreach (var pat in el.GetSupportedPatterns())
                patterns.Add(pat.Name);
        }
        catch (Exception) { }
        item["patterns"] = patterns;
        return new JsonObject { ["item"] = item };
    }

    // [SECTION] Ref resolution

    // "id:XYZ" / "rt:1.2.3" / list_items id -> AutomationId or runtime id; otherwise Name (exact, then case-insensitive),
    // then AutomationId. "Parent/Child" finds Parent first and searches Child under it. Searches the given window
    // (default: the current page), then the whole main window and the open popups.
    public (AutomationElement Element, string? Window) ResolveRef(string reference, string window)
    {
        var root = ResolveWindow(window);
        var scopes = new List<SearchRoot> { root };
        if (window.Length == 0)
        {
            // Popups first: when a flyout is open, the user means its items.
            foreach (var p in FindPopups().AsEnumerable().Reverse())
                if (!p.Kind.Contains("tooltip"))
                    scopes.Insert(0, PopupRoot(p));
            scopes.Add(MainRoot());
        }
        foreach (var scope in scopes)
        {
            var found = FindInRoots(reference, scope.Elements, scope.Clip);
            if (found != null)
                return (found, scope.WindowName == MainWindowName ? null : scope.WindowName);
        }
        throw new ToolException($"item '{reference}' not found" + (window.Length > 0 ? $" in window '{window}'" : "") + " (see list_items)");
    }

    AutomationElement? FindInRoots(string reference, List<AutomationElement> roots, System.Drawing.Rectangle? clip)
    {
        // Whole string first (labels may contain '/'), then "Parent/Child" paths.
        var el = FindSingle(reference, roots, clip);
        if (el != null || !reference.Contains('/'))
            return el;
        var parts = reference.Split('/', StringSplitOptions.RemoveEmptyEntries);
        var current = roots;
        AutomationElement? last = null;
        foreach (var part in parts)
        {
            last = FindSingle(part, current, clip);
            if (last == null)
                return null;
            current = new List<AutomationElement> { last };
        }
        return last;
    }

    AutomationElement? FindSingle(string reference, List<AutomationElement> roots, System.Drawing.Rectangle? clip)
    {
        var cf = _g.Automation.ConditionFactory;
        var client = ClientScreenRect();

        // Control-view elements first (what list_items shows), then raw-view ones such as text inside templates.
        AutomationElement? First(ConditionBase cond) => FirstIn(cond.And(new PropertyCondition(P.E.IsControlElement, true))) ?? FirstIn(cond);

        AutomationElement? FirstIn(ConditionBase cond)
        {
            AutomationElement? fallback = null;
            using (ItemCache().Activate())
            {
                foreach (var root in roots)
                {
                    AutomationElement[] all;
                    try { all = root.FindAll(TreeScope.Subtree, cond); }
                    catch (Exception) { continue; }
                    foreach (var e in all)
                    {
                        if (clip != null && !InsideClip(Rect(e), clip))
                            continue;
                        if (clip != null ? IsShown(e) : IsVisible(e, client))
                            return e;
                        fallback ??= e;
                    }
                }
            }
            return fallback;
        }

        if (reference.StartsWith("id:", StringComparison.Ordinal))
            reference = reference[3..];
        else if (!reference.StartsWith("rt:", StringComparison.Ordinal))
        {
            var byName = First(cf.ByName(reference));
            if (byName != null)
                return byName;
            byName = First(cf.ByName(reference, PropertyConditionFlags.IgnoreCase));
            if (byName != null)
                return byName;
        }
        if (reference.StartsWith("rt:", StringComparison.Ordinal))
        {
            int[] rt;
            try { rt = reference[3..].Split('.').Select(int.Parse).ToArray(); }
            catch (Exception) { return null; }
            try
            {
                var byRt = First(new PropertyCondition(P.E.RuntimeId, rt));
                if (byRt != null)
                    return byRt;
            }
            catch (Exception) { }
            // Fallback: walk and compare.
            foreach (var root in roots)
            {
                try
                {
                    using (SubtreeCache(-1).Activate())
                    {
                        var cached = root.FindFirst(TreeScope.Element, TrueCondition.Default);
                        var hit = cached == null ? null : FindByRuntimeId(cached, rt);
                        if (hit != null)
                            return hit;
                    }
                }
                catch (Exception) { }
            }
            return null;
        }
        return First(cf.ByAutomationId(reference));
    }

    AutomationElement? FindByRuntimeId(AutomationElement e, int[] rt)
    {
        if (RuntimeId(e).SequenceEqual(rt))
            return e;
        AutomationElement[] children;
        try { children = e.CachedChildren; }
        catch (Exception) { return null; }
        foreach (var c in children)
        {
            var hit = FindByRuntimeId(c, rt);
            if (hit != null)
                return hit;
        }
        return null;
    }

    // [SECTION] Debug dump (--dump)

    public string DumpTree(int maxDepth)
    {
        var sb = new StringBuilder();
        var cr = SubtreeCache(-1);
        using (cr.Activate())
        {
            var root = _g.MainWindow.FindFirst(TreeScope.Element, TrueCondition.Default);
            if (root != null)
                Dump(root, 0, maxDepth, sb);
            if (_g.Process != null)
                foreach (IntPtr h in ProcessWindows(_g.Process.Id))
                {
                    if (h == _g.Hwnd)
                        continue;
                    sb.Append($"-- top-level 0x{h.ToInt64():X} class='{ClassName(h)}' title='{WindowText(h)}'\n");
                    var el = _g.Automation.FromHandle(h).FindFirst(TreeScope.Element, TrueCondition.Default);
                    if (el != null)
                        Dump(el, 1, maxDepth, sb);
                }
        }
        return sb.ToString();
    }

    void Dump(AutomationElement e, int depth, int maxDepth, StringBuilder sb)
    {
        var r = Rect(e);
        sb.Append(' ', depth * 2)
          .Append(TypeName(Type(e)))
          .Append(" name='").Append(Name(e)).Append('\'')
          .Append(" id='").Append(AutomationId(e)).Append('\'')
          .Append(" class='").Append(Get(e, P.E.ClassName, "")).Append('\'')
          .Append($" rect=[{r.X},{r.Y},{r.Width},{r.Height}]");
        var flags = Flags(e);
        if (flags.Count > 0)
            sb.Append(" flags=").Append(flags.ToJsonString());
        IntPtr h = Get(e, P.E.NativeWindowHandle, IntPtr.Zero);
        if (h != IntPtr.Zero)
            sb.Append($" hwnd=0x{h.ToInt64():X}");
        sb.Append('\n');
        if (depth >= maxDepth)
            return;
        AutomationElement[] children;
        try { children = e.CachedChildren; }
        catch (Exception) { return; }
        foreach (var c in children)
            Dump(c, depth + 1, maxDepth, sb);
    }
}
