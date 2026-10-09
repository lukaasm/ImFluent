using System.Diagnostics;
using System.Text.Json.Nodes;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Definitions;
using static WinUiMcp.Native;

namespace WinUiMcp;

// Application-level tools: app_info, pages, theme, window size, screenshots.
internal sealed partial class Tools
{
    // [SECTION] app_info

    JsonObject AppInfo()
    {
        var (w, h) = _g.ClientSize();
        GetWindowRect(_g.Hwnd, out RECT wr);
        var (pageId, pageTitle) = CurrentPage();
        var o = _g.ClientOrigin();
        return new JsonObject
        {
            ["app"] = "WinUIGallery",
            ["display_size"] = new JsonArray(w, h),
            ["framebuffer_scale"] = 1.0,
            ["dpi_scale"] = Math.Round(_g.DpiScale, 3),
            ["theme"] = _g.ReadTheme(),
            ["page"] = pageId,
            ["page_title"] = pageTitle,
            ["window_rect"] = new JsonArray(wr.Left, wr.Top, wr.Width, wr.Height),
            ["client_origin"] = new JsonArray(o.X, o.Y),
            ["root_window"] = MainWindowName,
            ["pid"] = _g.Process?.Id ?? 0,
            ["foreground"] = _g.IsForeground,
            ["background"] = _g.Interactive ? null
                : _g.OwnDesktop ? new JsonObject { ["desktop"] = HiddenDesktop.FullName }
                : new JsonObject
                {
                    ["desktop"] = "user",
                    ["on_monitor"] = OffscreenParker.IsOnAnyMonitor(_g.Hwnd),
                    ["foreground_lock"] = ForegroundLock.LastLocked,
                    ["focus_given_back"] = _g.FocusGivenBack,
                },
        };
    }

    JsonObject ListPages()
    {
        var pages = new JsonArray();
        foreach (var p in _g.Pages)
            pages.Add(p.Id);
        return new JsonObject { ["current"] = CurrentPage().Id, ["pages"] = pages };
    }

    // [SECTION] Current page

    // Page id ("Item_Button", "Section_BasicInput", "Home", "AllControls", "Settings") and the visible title.
    (string Id, string Title) CurrentPage()
    {
        var nav = NavView();
        if (nav == null)
            return ("", "");
        var cf = _g.Automation.ConditionFactory;
        var roots = PageRoot().Elements;
        string? headerTitle = null, sectionTitle = null;
        using (ItemCache().Activate())
        {
            foreach (var root in roots)
            {
                AutomationElement[] headers;
                try { headers = root.FindAll(TreeScope.Subtree, cf.ByAutomationId("PageHeader").Or(cf.ByAutomationId("TitleTxt"))); }
                catch (Exception) { continue; }
                foreach (var hdr in headers)
                {
                    if (Get(hdr, P.E.IsOffscreen, false))
                        continue;
                    if (AutomationId(hdr) == "PageHeader")
                        headerTitle ??= Name(hdr);
                    else
                        sectionTitle ??= Name(hdr);
                }
            }
        }
        if (headerTitle != null)
        {
            var item = _g.Pages.FirstOrDefault(p => p.Id.StartsWith("Item_") && p.Title == headerTitle);
            if (item != null)
                return (item.Id, headerTitle);
            if (headerTitle == "Settings")
                return ("Settings", headerTitle);
        }
        if (sectionTitle != null)
        {
            var group = _g.Pages.FirstOrDefault(p => p.IsGroup && p.Title == sectionTitle);
            if (group != null)
                return (group.Id, sectionTitle);
        }
        // Home, All, search results...: fall back to the selected navigation item.
        var selected = SelectedNavItem(nav);
        if (selected != null)
        {
            string aid = AutomationId(selected);
            var page = _g.Pages.FirstOrDefault(p => p.UniqueId == aid);
            if (page != null)
                return (page.Id, headerTitle ?? sectionTitle ?? page.Title);
        }
        return (headerTitle != null ? "Item_" + headerTitle : "", headerTitle ?? sectionTitle ?? "");
    }

    AutomationElement? SelectedNavItem(AutomationElement nav)
    {
        try
        {
            using (ItemCache().Activate())
                return nav.FindFirst(TreeScope.Descendants, _g.Automation.ConditionFactory.ByControlType(ControlType.ListItem).And(new FlaUI.Core.Conditions.PropertyCondition(P.Sel.IsSelected, true)).And(_g.Automation.ConditionFactory.ByClassName("Microsoft.UI.Xaml.Controls.NavigationViewItem")));
        }
        catch (Exception)
        {
            return null;
        }
    }

    // [SECTION] goto_page

    JsonObject GotoPageTool(JsonObject args)
    {
        string name = Str(args, "page");
        var page = _g.ResolvePage(name);
        if (page == null)
            return ToolError($"goto_page: unknown page '{name}' (see list_pages)");
        string? err = GotoPage(page);
        if (err != null)
            return ToolError("goto_page: " + err);
        // Heavy pages (WebView2, maps) can rebuild their UIA tree for a moment after loading.
        var (id, title) = CurrentPage();
        for (int i = 0; i < 10 && id != page.Id; ++i)
        {
            Thread.Sleep(150);
            (id, title) = CurrentPage();
        }
        return ToolText(new JsonObject { ["page"] = id.Length > 0 ? id : page.Id, ["title"] = title.Length > 0 ? title : page.Title });
    }

    // Navigates through the NavigationView (UIA SelectionItem.Select, no real input), falling back to the
    // title-bar search box. Returns an error message or null.
    string? GotoPage(PageInfo page)
    {
        if (CurrentPage().Id == page.Id)
            return null;
        var nav = NavView() ?? throw new ToolException("NavigationView not found");
        var cf = _g.Automation.ConditionFactory;

        AutomationElement? FindNavItem(string aid) => nav.FindFirstDescendant(cf.ByAutomationId(aid).And(cf.ByClassName("Microsoft.UI.Xaml.Controls.NavigationViewItem")));

        // A compact pane shows an expanded category's items in a flyout. Its UIA subtree hangs off the window root,
        // outside the NavigationView.
        AutomationElement? FindNavItemInPopups(string aid) =>
            _g.MainWindow.FindFirstDescendant(cf.ByAutomationId(aid).And(cf.ByClassName("Microsoft.UI.Xaml.Controls.NavigationViewItem")));

        AutomationElement? target = FindNavItem(page.UniqueId);
        if (target == null && page.GroupId != null && FindNavItem(page.GroupId) == null)
        {
            // Minimal/compact pane: open it through the title bar's pane toggle button (Invoke, no real input).
            var toggle = _g.MainWindow.FindFirstDescendant(cf.ByAutomationId("PART_PaneToggleButton"));
            if (toggle?.Patterns.Invoke.PatternOrDefault is { } tinv)
            {
                tinv.Invoke();
                Thread.Sleep(400);
                target = FindNavItem(page.UniqueId);
            }
        }
        if (target == null && page.GroupId != null)
        {
            var group = FindNavItem(page.GroupId);
            if (group != null && group.Patterns.ExpandCollapse.PatternOrDefault is { } ec)
            {
                try { ec.Expand(); } catch (Exception) { }
                var sw = Stopwatch.StartNew();
                while (target == null && sw.ElapsedMilliseconds < 3000)
                {
                    Thread.Sleep(60);
                    target = FindNavItem(page.UniqueId) ?? FindNavItemInPopups(page.UniqueId);
                }
            }
        }

        bool requested = false;
        if (target != null)
        {
            if (target.Patterns.SelectionItem.PatternOrDefault is { } sel)
            {
                sel.Select();
                requested = true;
            }
            else if (target.Patterns.Invoke.PatternOrDefault is { } inv)
            {
                inv.Invoke();
                requested = true;
            }
        }
        if (requested && WaitForPage(page, 6000))
            return null;
        if (Interactive && !page.IsGroup && page.GroupId != null && GotoViaSearch(page) && WaitForPage(page, 6000))
            return null;
        var (cur, title) = CurrentPage();
        if (page.IsGroup && _g.Pages.Any(p => p.GroupId == page.UniqueId) && (page.UniqueId is "FundamentalsItem" or "DesignItem" or "AccessibilityItem"))
            return $"'{page.Id}' is a navigation category without its own page (current page: '{cur}')";
        return $"could not navigate to '{page.Id}' (current page: '{cur}' \"{title}\")";
    }

    bool WaitForPage(PageInfo page, int timeoutMs)
    {
        var sw = Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < timeoutMs)
        {
            if (CurrentPage().Id == page.Id)
            {
                Thread.Sleep(350); // page transition animation
                return true;
            }
            Thread.Sleep(80);
        }
        return false;
    }

    // Types the control title into the title-bar AutoSuggestBox and clicks the exact suggestion (real input).
    bool GotoViaSearch(PageInfo page)
    {
        var cf = _g.Automation.ConditionFactory;
        var box = _g.MainWindow.FindFirstDescendant(cf.ByAutomationId("controlsSearchBox"));
        var edit = box?.FindFirstDescendant(cf.ByControlType(ControlType.Edit));
        if (edit == null)
            return false;
        _g.Activate();
        try { edit.Focus(); } catch (Exception) { }
        Thread.Sleep(80);
        InputSim.Press(new KeyChord(true, false, false, false, 0x41, false)); // Ctrl+A
        InputSim.TypeText(page.Title);
        _inputSource = "keyboard";
        var sw = Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < 3000)
        {
            Thread.Sleep(120);
            foreach (var p in FindPopups())
            {
                var item = p.Element.FindFirstDescendant(cf.ByControlType(ControlType.ListItem).And(cf.ByName(page.Title)));
                if (item == null)
                    continue;
                ClickElement(item, "left", false);
                return true;
            }
        }
        InputSim.Press(new KeyChord(false, false, false, false, 0x1B, false)); // Escape
        return false;
    }

    // [SECTION] set_theme

    JsonObject SetThemeTool(JsonObject args)
    {
        string theme = Str(args, "theme").ToLowerInvariant();
        if (theme == "highcontrast")
            return ToolError("set_theme: the WinUI 3 Gallery has no high-contrast switch; turn on a Windows contrast theme (Settings > Accessibility > Contrast themes) instead");
        if (theme != "light" && theme != "dark" && theme != "system")
            return ToolError("set_theme: theme must be light, dark or highcontrast (or \"system\" to follow Windows)");
        string itemName = theme == "light" ? "Light" : theme == "dark" ? "Dark" : "Use system setting";
        var previous = _g.ResolvePage(CurrentPage().Id);
        var settings = _g.ResolvePage("Settings")!;
        string? err = GotoPage(settings);
        if (err != null)
            return ToolError("set_theme: " + err);
        var cf = _g.Automation.ConditionFactory;
        var combo = PageRoot().Elements.Select(r => r.FindFirstDescendant(cf.ByAutomationId("themeModeComboBox"))).FirstOrDefault(e => e != null);
        if (combo == null)
            return ToolError("set_theme: theme combo box not found on the Settings page");
        var ec = combo.Patterns.ExpandCollapse.PatternOrDefault;
        ec?.Expand();
        Thread.Sleep(250);
        AutomationElement? item = combo.FindFirstDescendant(cf.ByName(itemName));
        if (item == null)
            foreach (var p in FindPopups())
                item ??= p.Element.FindFirstDescendant(cf.ByName(itemName).And(cf.ByControlType(ControlType.ListItem)));
        if (item?.Patterns.SelectionItem.PatternOrDefault is not { } sel)
        {
            try { ec?.Collapse(); } catch (Exception) { }
            return ToolError($"set_theme: item '{itemName}' not found in the theme combo box");
        }
        sel.Select();
        Thread.Sleep(100);
        try
        {
            if (ec != null && ec.ExpandCollapseState.Value != ExpandCollapseState.Collapsed)
                ec.Collapse();
        }
        catch (Exception) { }
        Thread.Sleep(400);
        if (previous != null && previous.Id != "Settings")
            GotoPage(previous);
        return ToolText(new JsonObject { ["theme"] = theme });
    }

    // [SECTION] resize_window

    JsonObject ResizeWindowTool(JsonObject args)
    {
        int w = Int(args, "width", 1280), h = Int(args, "height", 800);
        if (w < 100 || h < 100)
            return ToolError("resize_window: width and height must be at least 100");
        if (IsIconic(_g.Hwnd) || IsZoomed(_g.Hwnd))
            ShowWindow(_g.Hwnd, Interactive ? SW_RESTORE : SW_SHOWNOACTIVATE);
        // The gallery draws its own title bar (ExtendsContentIntoTitleBar), so measure the real frame instead of
        // trusting AdjustWindowRectEx; a second pass corrects any rounding.
        for (int pass = 0; pass < 3; ++pass)
        {
            GetWindowRect(_g.Hwnd, out RECT wr);
            var (cw, ch) = _g.ClientSize();
            if (cw == w && ch == h)
                break;
            int fw = wr.Width - cw, fh = wr.Height - ch;
            SetWindowPos(_g.Hwnd, IntPtr.Zero, 0, 0, w + fw, h + fh, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            Thread.Sleep(150);
        }
        Thread.Sleep(250); // let XAML re-layout
        var (nw, nh) = _g.ClientSize();
        var result = new JsonObject { ["display_size"] = new JsonArray(nw, nh) };
        if (nw != w || nh != h)
            result["note"] = "the window enforces a minimum size or the requested size does not fit the monitor";
        return ToolText(result);
    }

    [System.Runtime.InteropServices.DllImport("user32.dll")] static extern bool IsZoomed(IntPtr hwnd);

    // [SECTION] screenshot

    JsonObject ScreenshotTool(JsonObject args)
    {
        var img = CaptureComposite();
        if (img == null)
            return ToolError("screenshot: PrintWindow capture failed");
        var o = _g.ClientOrigin();
        // [x, y, w, h] in client coordinates.
        double x = 0, y = 0, rw = img.Width, rh = img.Height;
        string window = Str(args, "window");
        if (window.Length > 0 && window != MainWindowName && !window.Equals("$MAIN", StringComparison.OrdinalIgnoreCase))
        {
            var popups = FindPopups();
            PopupInfo? p = window.Equals("$FOCUSED", StringComparison.OrdinalIgnoreCase)
                ? FocusedPopup(popups) ?? popups.LastOrDefault()
                : popups.FirstOrDefault(q => q.Name == window || q.Id == window) ?? popups.FirstOrDefault(q => q.Name.Equals(window, StringComparison.OrdinalIgnoreCase));
            if (p == null && !window.Equals("$FOCUSED", StringComparison.OrdinalIgnoreCase))
                return ToolError($"screenshot: no window named '{window}'");
            if (p != null)
            {
                x = p.Rect.X - o.X; y = p.Rect.Y - o.Y; rw = p.Rect.Width; rh = p.Rect.Height;
            }
        }
        if (args["rect"] is JsonArray ra && ra.Count == 4)
        {
            double V(int i) => ra[i] is JsonValue v && v.TryGetValue(out double d) ? d : 0;
            x = V(0); y = V(1); rw = V(2); rh = V(3);
        }
        double margin = Num(args, "margin", 0);
        x -= margin; y -= margin; rw += margin * 2; rh += margin * 2;
        int x0 = (int)Math.Max(0, Math.Floor(x)), y0 = (int)Math.Max(0, Math.Floor(y));
        int x1 = (int)Math.Min(img.Width, Math.Ceiling(x + rw)), y1 = (int)Math.Min(img.Height, Math.Ceiling(y + rh));
        if (x1 - x0 < 1 || y1 - y0 < 1)
            return ToolError("screenshot: empty capture rectangle");
        if (x0 != 0 || y0 != 0 || x1 != img.Width || y1 != img.Height)
            img = img.Crop(x0, y0, x1 - x0, y1 - y0);
        float scale = (float)Math.Clamp(Num(args, "scale", 1), 0.05, 1);
        if (scale < 1)
            img = img.Downscale(scale);
        byte[] png = img.EncodePng();
        var info = new JsonObject
        {
            ["rect"] = new JsonArray(x0, y0, x1 - x0, y1 - y0),
            ["size"] = new JsonArray(img.Width, img.Height),
        };
        string savePath = Str(args, "save_path");
        if (savePath.Length > 0)
        {
            try
            {
                string? dir = Path.GetDirectoryName(Path.GetFullPath(savePath));
                if (!string.IsNullOrEmpty(dir))
                    Directory.CreateDirectory(dir);
                File.WriteAllBytes(savePath, png);
            }
            catch (Exception e)
            {
                return ToolError($"screenshot: cannot write '{savePath}': {e.Message}");
            }
            info["saved"] = savePath;
        }
        var content = new JsonArray();
        if (Bool(args, "inline", true))
            content.Add(new JsonObject { ["type"] = "image", ["data"] = Convert.ToBase64String(png), ["mimeType"] = "image/png" });
        content.Add(new JsonObject { ["type"] = "text", ["text"] = info.ToJsonString(s_Compact) });
        return new JsonObject { ["content"] = content, ["isError"] = false };
    }

    // The main window's client area with the process's other visible top-level windows (windowed popups,
    // menus, tooltips) composited on top in z-order, so the picture matches what is on screen.
    Image? CaptureComposite()
    {
        var img = Screenshot.CaptureClient(_g.Hwnd);
        if (img == null || _g.Process == null)
            return img;
        var o = _g.ClientOrigin();
        var windows = ProcessWindows(_g.Process.Id);
        windows.Reverse(); // bottom-most first
        foreach (IntPtr h in windows)
        {
            if (h == _g.Hwnd || GetWindow(h, GW_OWNER) == IntPtr.Zero && ClassName(h) == "WinUIDesktopWin32WindowClass")
                continue;
            var pop = Screenshot.CaptureClient(h, keepAlpha: true);
            if (pop == null)
                continue;
            var po = ClientOrigin(h);
            Blit(pop, img, po.X - o.X, po.Y - o.Y);
        }
        return img;
    }

    // Composites a popup capture. PrintWindow returns the transparent shadow margin of windowed XAML popups as
    // opaque black, so pure-black pixels connected to the capture border are treated as transparent (the
    // shadow is dropped; black pixels inside the content are kept).
    static void Blit(Image src, Image dst, int dx, int dy)
    {
        int w = src.Width, h = src.Height;
        var clear = new bool[w * h];
        var stack = new Stack<int>();
        bool IsBlack(int i) => src.Pixels[i * 4] == 0 && src.Pixels[i * 4 + 1] == 0 && src.Pixels[i * 4 + 2] == 0;
        void Seed(int x, int y)
        {
            int i = y * w + x;
            if (!clear[i] && IsBlack(i))
            {
                clear[i] = true;
                stack.Push(i);
            }
        }
        for (int x = 0; x < w; ++x) { Seed(x, 0); Seed(x, h - 1); }
        for (int y = 0; y < h; ++y) { Seed(0, y); Seed(w - 1, y); }
        while (stack.Count > 0)
        {
            int i = stack.Pop(), x = i % w, y = i / w;
            if (x > 0) Seed(x - 1, y);
            if (x + 1 < w) Seed(x + 1, y);
            if (y > 0) Seed(x, y - 1);
            if (y + 1 < h) Seed(x, y + 1);
        }
        for (int y = 0; y < h; ++y)
        {
            int ty = y + dy;
            if (ty < 0 || ty >= dst.Height)
                continue;
            int sx0 = Math.Max(0, -dx), sx1 = Math.Min(w, dst.Width - dx);
            for (int x = sx0; x < sx1; ++x)
            {
                if (clear[y * w + x])
                    continue;
                Buffer.BlockCopy(src.Pixels, (y * w + x) * 4, dst.Pixels, (ty * dst.Width + x + dx) * 4, 3);
            }
        }
    }
}
