using System.Diagnostics;
using System.Text.Json.Nodes;
using FlaUI.Core.AutomationElements;
using FlaUI.UIA3;
using static WinUiMcp.Native;

namespace WinUiMcp;

internal sealed class ToolException : Exception
{
    public ToolException(string message) : base(message) { }
}

// One gallery page from ControlInfoData.json.
internal sealed record PageInfo(string Id, string UniqueId, string Title, string? GroupId, bool IsGroup);

// The WinUI 3 Gallery process, its main window and the page catalog. Lives on the STA thread.
internal sealed class Gallery
{
    public const string ProcessName = "WinUIGallery";
    const string MainWindowClass = "WinUIDesktopWin32WindowClass";

    public readonly UIA3Automation Automation = new();
    public Process? Process;
    public IntPtr Hwnd;
    public AutomationElement? Window;
    public readonly List<PageInfo> Pages = new();
    public string? SettingsFile;

    readonly string _exePath;
    readonly bool _attachOnly;

    // --interactive: real input (SendInput) and bringing the gallery to the front are allowed. Off by default so the
    // driver never moves the user's mouse or steals the focus.
    public readonly bool Interactive;

    // Background mode on the hidden desktop (the default): the gallery runs on HiddenDesktop. Otherwise background
    // mode keeps it on the user's desktop, parked off-screen, and takes the foreground lock around each call.
    public readonly bool OwnDesktop;

    // Background mode on the user's desktop: hold the foreground lock while launching and during tool calls.
    public bool UsesForegroundLock => !Interactive && !OwnDesktop;

    public Gallery(string exePath, bool attachOnly, bool interactive, bool ownDesktop)
    {
        _exePath = exePath;
        _attachOnly = attachOnly;
        Interactive = interactive;
        OwnDesktop = ownDesktop;
        LoadCatalog();
    }

    public AutomationElement MainWindow => Window ?? throw new ToolException("the gallery window is not available");

    // [SECTION] Process and window

    public void EnsureConnected()
    {
        if (Process != null && !Process.HasExited && Hwnd != IntPtr.Zero && IsWindow(Hwnd) && Window != null)
            return;
        Connect(TimeSpan.FromSeconds(90));
    }

    // Background mode: keeps the gallery window off every monitor (see OffscreenParker).
    OffscreenParker? _parker;

    // How often the gallery took the foreground in background mode and the driver gave it back.
    public int FocusGivenBack => _parker?.Restored ?? 0;

    public void Connect(TimeSpan timeout)
    {
        // Background mode: no process may take the foreground while the gallery starts (WinUI's Window.Activate()).
        using var foregroundLock = new ForegroundLock(UsesForegroundLock);
        Process = null;
        Hwnd = IntPtr.Zero;
        Window = null;
        _parker?.Dispose();
        _parker = null;
        bool launched = false;
        // Window enumeration only sees this thread's desktop, so on the hidden desktop a gallery the user runs on
        // their own desktop has no window here and is left alone.
        var running = Process.GetProcessesByName(ProcessName).FirstOrDefault(p => !p.HasExited && (!OwnDesktop || FindMainWindow(p.Id) != IntPtr.Zero));
        if (running != null)
        {
            Process = running;
            Log($"attached to {ProcessName} (pid {running.Id})");
        }
        else
        {
            if (_attachOnly)
                throw new ToolException($"{ProcessName} is not running (started with --attach)");
            if (!File.Exists(_exePath))
                throw new ToolException($"gallery executable not found: {_exePath}");
            string? shim = PrepareRuntimeShim(Path.GetDirectoryName(_exePath)!);
            Process = Launch(_exePath, shim, UsesForegroundLock, OwnDesktop ? HiddenDesktop.FullName : null, out _parker);
            launched = true;
            Log(Interactive ? $"launched {_exePath} (pid {Process.Id})"
                : OwnDesktop ? $"launched {_exePath} (pid {Process.Id}) on the hidden desktop {HiddenDesktop.FullName}"
                : $"launched {_exePath} (pid {Process.Id}) off-screen, without activation");
        }

        var sw = Stopwatch.StartNew();
        while (sw.Elapsed < timeout)
        {
            if (Process.HasExited)
                throw new ToolException($"{ProcessName} exited (code {Process.ExitCode})");
            IntPtr h = FindMainWindow(Process.Id);
            if (h != IntPtr.Zero)
            {
                Hwnd = h;
                break;
            }
            Thread.Sleep(200);
        }
        if (Hwnd == IntPtr.Zero)
            throw new ToolException("timed out waiting for the gallery main window");
        if (UsesForegroundLock)
        {
            // A gallery this driver started (now or in an earlier run) stays parked off-screen. One the user started
            // and put on a monitor stays where it is.
            if (launched || !OffscreenParker.IsOnAnyMonitor(Hwnd))
            {
                _parker ??= new OffscreenParker((uint)Process.Id, MainWindowClass);
                OffscreenParker.Park(Hwnd);
            }
            else
                Log("the running gallery is on a monitor and stays there; close it to let the driver start one off-screen");
        }
        Window = Automation.FromHandle(Hwnd);

        // The navigation pane is built after the window shows; wait until the Home item exists.
        while (sw.Elapsed < timeout)
        {
            try
            {
                if (Window.FindFirstDescendant(cf => cf.ByAutomationId("Home")) != null)
                    break;
            }
            catch (Exception) { }
            Thread.Sleep(250);
        }
        Log($"main window 0x{Hwnd.ToInt64():X} '{WindowText(Hwnd)}'");
        SettingsFile = FindSettingsFile();
    }

    // A self-contained unpackaged gallery build can lack Microsoft.WindowsAppRuntime.Insights.Resource.dll; the
    // Home page header then crashes the app (ReleaseInfo.Major throws "Unable to load resource dll"). Copy the
    // DLL from the installed Windows App Runtime framework package into a private folder and put that folder on
    // the gallery's PATH. Nothing in the gallery folder is touched.
    static string? PrepareRuntimeShim(string exeDir)
    {
        const string dll = "Microsoft.WindowsAppRuntime.Insights.Resource.dll";
        if (File.Exists(Path.Combine(exeDir, dll)))
            return null;
        try
        {
            string shimDir = Path.Combine(AppContext.BaseDirectory, "runtime_shim");
            if (File.Exists(Path.Combine(shimDir, dll)))
                return shimDir;
            foreach (string family in new[] { "Microsoft.WindowsAppRuntime.2_8wekyb3d8bbwe", "Microsoft.WindowsAppRuntime.1.8_8wekyb3d8bbwe", "Microsoft.WindowsAppRuntime.1.7_8wekyb3d8bbwe" })
            {
                var candidates = PackageFullNames(family)
                    .Where(n => n.Contains("_x64_", StringComparison.OrdinalIgnoreCase))
                    .OrderByDescending(n => Version.TryParse(n.Split('_')[1], out var v) ? v : new Version(0, 0));
                foreach (string fullName in candidates)
                {
                    string? path = PackagePath(fullName);
                    if (path == null || !File.Exists(Path.Combine(path, dll)))
                        continue;
                    Directory.CreateDirectory(shimDir);
                    File.Copy(Path.Combine(path, dll), Path.Combine(shimDir, dll), overwrite: true);
                    Log($"{dll} is missing next to the gallery; using the copy from {fullName} (via PATH)");
                    return shimDir;
                }
            }
            Log($"{dll} is missing next to the gallery and no Windows App Runtime package provides it; the gallery may crash on the Home page");
        }
        catch (Exception e)
        {
            Log($"runtime shim failed: {e.Message}");
        }
        return null;
    }

    [System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
    static extern int GetPackagesByPackageFamily(string family, ref uint count, IntPtr[]? fullNames, ref uint bufferLength, char[]? buffer);

    [System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
    static extern int GetPackagePathByFullName(string fullName, ref uint length, char[]? path);

    static List<string> PackageFullNames(string family)
    {
        uint count = 0, len = 0;
        GetPackagesByPackageFamily(family, ref count, null, ref len, null);
        if (count == 0 || len == 0)
            return new List<string>();
        var ptrs = new IntPtr[count];
        var buf = new char[len];
        if (GetPackagesByPackageFamily(family, ref count, ptrs, ref len, buf) != 0)
            return new List<string>();
        return new string(buf).Split('\0', StringSplitOptions.RemoveEmptyEntries).ToList();
    }

    static string? PackagePath(string fullName)
    {
        uint len = 0;
        GetPackagePathByFullName(fullName, ref len, null);
        if (len == 0)
            return null;
        var buf = new char[len];
        return GetPackagePathByFullName(fullName, ref len, buf) == 0 ? new string(buf, 0, (int)len - 1) : null;
    }

    static IntPtr FindMainWindow(int pid)
    {
        IntPtr best = IntPtr.Zero;
        foreach (IntPtr h in ProcessWindows(pid))
        {
            if (ClassName(h) != MainWindowClass || GetWindow(h, GW_OWNER) != IntPtr.Zero)
                continue;
            string title = WindowText(h);
            if (title.StartsWith("WinUI 3 Gallery", StringComparison.OrdinalIgnoreCase))
                return h;
            if (best == IntPtr.Zero && title.Length > 0)
                best = h;
        }
        return best;
    }

    // Starts the gallery. Background mode: suspended until the OffscreenParker hook is in place, with STARTUPINFO
    // asking for no activation and an off-screen position (the parker enforces the position either way).
    static Process Launch(string exe, string? pathPrefix, bool background, string? desktop, out OffscreenParker? parker)
    {
        parker = null;
        var env = new System.Text.StringBuilder();
        var vars = Environment.GetEnvironmentVariables().Cast<System.Collections.DictionaryEntry>()
            .Select(e => (Key: (string)e.Key, Value: (string?)e.Value ?? ""))
            .ToDictionary(e => e.Key, e => e.Value, StringComparer.OrdinalIgnoreCase);
        if (pathPrefix != null)
            vars["PATH"] = pathPrefix + ";" + (vars.TryGetValue("PATH", out var path) ? path : "");
        foreach (var kv in vars.OrderBy(k => k.Key, StringComparer.OrdinalIgnoreCase))
            env.Append(kv.Key).Append('=').Append(kv.Value).Append('\0');
        env.Append('\0');

        var si = new STARTUPINFO { cb = System.Runtime.InteropServices.Marshal.SizeOf<STARTUPINFO>(), dwFlags = STARTF_USESHOWWINDOW, wShowWindow = SW_SHOWNOACTIVATE, lpDesktop = desktop };
        uint flags = CREATE_UNICODE_ENVIRONMENT;
        if (background)
        {
            (si.dwX, si.dwY) = OffscreenParker.ParkPosition();
            si.dwFlags |= STARTF_USEPOSITION;
            flags |= CREATE_SUSPENDED;
        }
        string cmd = "\"" + exe + "\"";
        if (!CreateProcess(null, new System.Text.StringBuilder(cmd), IntPtr.Zero, IntPtr.Zero, false, flags, env.ToString(), Path.GetDirectoryName(exe), ref si, out PROCESS_INFORMATION pi))
            throw new ToolException($"could not start the gallery (error {System.Runtime.InteropServices.Marshal.GetLastWin32Error()})");
        if (background)
        {
            parker = new OffscreenParker((uint)pi.dwProcessId, MainWindowClass);
            ResumeThread(pi.hThread);
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return Process.GetProcessById(pi.dwProcessId);
    }

    // Brings the gallery to the foreground so real input reaches it. Only in --interactive mode: in the default
    // background mode the driver never activates the gallery.
    public void Activate()
    {
        if (!Interactive)
            return;
        IntPtr h = Hwnd;
        if (IsIconic(h))
            ShowWindow(h, SW_RESTORE);
        if (IsForeground)
            return; // the main window or one of its popups (menus and flyouts take the foreground)
        IntPtr fg = GetForegroundWindow();
        uint fgThread = fg != IntPtr.Zero ? GetWindowThreadProcessId(fg, out _) : 0;
        uint myThread = GetCurrentThreadId();
        bool attached = fgThread != 0 && fgThread != myThread && AttachThreadInput(myThread, fgThread, true);
        try
        {
            BringWindowToTop(h);
            SetForegroundWindow(h);
        }
        finally
        {
            if (attached)
                AttachThreadInput(myThread, fgThread, false);
        }
        if (GetForegroundWindow() != h)
        {
            // The usual workaround: a synthetic Alt press unlocks SetForegroundWindow.
            InputSim.Press(new KeyChord(false, false, false, false, 0xA4, false));
            SetForegroundWindow(h);
        }
        for (int i = 0; i < 20 && GetForegroundWindow() != h; ++i)
            Thread.Sleep(25);
        Thread.Sleep(30);
    }

    // True when the gallery (its main window or one of its popup windows) is the foreground application.
    public bool IsForeground
    {
        get
        {
            IntPtr fg = GetForegroundWindow();
            return fg != IntPtr.Zero && (fg == Hwnd || (Process != null && WindowPid(fg) == (uint)Process.Id));
        }
    }

    // [SECTION] Coordinates

    public POINT ClientOrigin() => Native.ClientOrigin(Hwnd);

    public (int W, int H) ClientSize()
    {
        GetClientRect(Hwnd, out RECT r);
        return (r.Width, r.Height);
    }

    public float DpiScale => GetDpiForWindow(Hwnd) / 96f;

    // Screen rectangle -> [x, y, w, h] relative to the client origin.
    public JsonArray ToClientRect(System.Drawing.Rectangle r)
    {
        if (r.Width <= 0 && r.Height <= 0 && r.X == 0 && r.Y == 0)
            return new JsonArray(0, 0, 0, 0); // no on-screen rectangle (collapsed or scrolled out)
        var o = ClientOrigin();
        return new JsonArray(r.X - o.X, r.Y - o.Y, r.Width, r.Height);
    }

    public JsonArray ToClientRect(RECT r)
    {
        var o = ClientOrigin();
        return new JsonArray(r.Left - o.X, r.Top - o.Y, r.Width, r.Height);
    }

    // [SECTION] Page catalog (ControlInfoData.json)

    void LoadCatalog()
    {
        Pages.Add(new PageInfo("Home", "Home", "Home", null, false));
        Pages.Add(new PageInfo("AllControls", "AllControlsItem", "All", null, false));
        Pages.Add(new PageInfo("Settings", "SettingsItem", "Settings", null, false));
        string? json = FindCatalogFile();
        if (json == null)
        {
            Log("ControlInfoData.json not found; list_pages only knows Home/AllControls/Settings");
            return;
        }
        var root = JsonNode.Parse(File.ReadAllText(json), documentOptions: new System.Text.Json.JsonDocumentOptions { CommentHandling = System.Text.Json.JsonCommentHandling.Skip, AllowTrailingCommas = true });
        var groups = root?["Groups"]?.AsArray();
        if (groups == null)
            return;
        var items = new List<PageInfo>();
        foreach (var g in groups)
        {
            string gid = (string?)g?["UniqueId"] ?? "";
            string gtitle = (string?)g?["Title"] ?? gid;
            // Special sections (Fundamentals, Design, Accessibility) are navigation categories without a page.
            bool special = g?["IsSpecialSection"] is JsonValue sv && sv.TryGetValue(out bool sb) && sb;
            if (!special)
                Pages.Add(new PageInfo("Section_" + gid, gid, gtitle, null, true));
            foreach (var it in g?["Items"]?.AsArray() ?? new JsonArray())
            {
                string id = (string?)it?["UniqueId"] ?? "";
                if (id.Length > 0)
                    items.Add(new PageInfo("Item_" + id, id, (string?)it?["Title"] ?? id, gid, false));
            }
        }
        Pages.AddRange(items);
    }

    string? FindCatalogFile()
    {
        var candidates = new List<string>();
        string exeDir = Path.GetDirectoryName(_exePath) ?? "";
        candidates.Add(Path.Combine(exeDir, "Samples", "Data", "ControlInfoData.json"));
        if (Program.RepoRoot != null)
            candidates.Add(Path.Combine(Program.RepoRoot, "docs", "winui_gallery", "WinUIGallery", "Samples", "Data", "ControlInfoData.json"));
        return candidates.FirstOrDefault(File.Exists);
    }

    public PageInfo? ResolvePage(string name)
    {
        name = name.Trim();
        foreach (var p in Pages)
            if (p.Id.Equals(name, StringComparison.OrdinalIgnoreCase))
                return p;
        string bare = name;
        foreach (string prefix in new[] { "Item_", "Section_" })
            if (bare.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                bare = bare[prefix.Length..];
        // Items first ("Button"), then sections ("BasicInput"), then titles ("Basic input", "Color Contrast").
        foreach (var p in Pages.Where(p => !p.IsGroup))
            if (p.UniqueId.Equals(bare, StringComparison.OrdinalIgnoreCase))
                return p;
        foreach (var p in Pages)
            if (p.UniqueId.Equals(bare, StringComparison.OrdinalIgnoreCase))
                return p;
        foreach (var p in Pages)
            if (p.Title.Equals(bare, StringComparison.OrdinalIgnoreCase))
                return p;
        if (bare.Equals("All", StringComparison.OrdinalIgnoreCase))
            return Pages[1];
        return null;
    }

    // [SECTION] Theme

    string? FindSettingsFile()
    {
        try
        {
            string appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
            string? product = Process?.MainModule?.FileVersionInfo.ProductName;
            var names = new[] { product, "WinUI 3 Gallery", "WinUI 3 Gallery Dev", "WinUIGallery" };
            foreach (var n in names)
                if (!string.IsNullOrEmpty(n) && File.Exists(Path.Combine(appData, n, "AppConfig.json")))
                    return Path.Combine(appData, n, "AppConfig.json");
        }
        catch (Exception) { }
        return null;
    }

    // light / dark / highcontrast from the gallery settings (or the system setting when it follows the system).
    public string ReadTheme()
    {
        if (IsHighContrast())
            return "highcontrast";
        string? setting = null;
        try
        {
            SettingsFile ??= FindSettingsFile();
            if (SettingsFile != null && File.Exists(SettingsFile))
            {
                using var fs = new FileStream(SettingsFile, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                var node = JsonNode.Parse(fs);
                var v = node?["SelectedAppTheme"];
                setting = v?.ToString();
            }
        }
        catch (Exception) { }
        // ElementTheme: Default = 0, Light = 1, Dark = 2.
        if (setting == "1" || string.Equals(setting, "Light", StringComparison.OrdinalIgnoreCase))
            return "light";
        if (setting == "2" || string.Equals(setting, "Dark", StringComparison.OrdinalIgnoreCase))
            return "dark";
        try
        {
            using var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
            if (key?.GetValue("AppsUseLightTheme") is int light)
                return light != 0 ? "light" : "dark";
        }
        catch (Exception) { }
        return "light";
    }

    public static void Log(string message)
    {
        Console.Error.WriteLine("[winui_mcp] " + message);
    }
}
