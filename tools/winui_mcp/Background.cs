using static WinUiMcp.Native;

namespace WinUiMcp;

// Background mode keeps the WinUI 3 Gallery off the user's desktop and away from the keyboard focus.
//
// - ForegroundLock: while it is held, Windows refuses SetForegroundWindow and window activation from every process.
//   The user's own clicks and the Alt key still switch windows (and release the lock). The driver holds it while it
//   launches the gallery, whose Window.Activate() would otherwise take the foreground, and during every tool call,
//   where UIA SetFocus, Invoke and opening popups could activate the gallery.
// - OffscreenParker: moves the gallery's main window outside every monitor as soon as it is created, and again
//   whenever it comes back onto a monitor.
// The default background mode: the gallery runs on its own desktop (CreateDesktop), which is never shown. Nothing
// there can appear on the user's screen or take the user's keyboard focus: activation and the foreground are per
// desktop. The driver's UI Automation thread joins that desktop, because UI Automation and window enumeration only
// see the calling thread's desktop.
internal static class HiddenDesktop
{
    public const string Name = "ImFluentWinUiGallery";

    public static IntPtr Handle { get; private set; }

    // "WinSta0\ImFluentWinUiGallery", for STARTUPINFO.lpDesktop.
    public static string FullName => @"WinSta0\" + Name;

    // Creates the desktop, or opens it when an earlier driver run created it. False when that fails.
    public static bool Create()
    {
        Handle = CreateDesktop(Name, IntPtr.Zero, IntPtr.Zero, 0, GENERIC_ALL, IntPtr.Zero);
        if (Handle == IntPtr.Zero)
            Gallery.Log($"CreateDesktop failed (error {System.Runtime.InteropServices.Marshal.GetLastWin32Error()})");
        return Handle != IntPtr.Zero;
    }
}

internal sealed class ForegroundLock : IDisposable
{
    static int s_depth;
    static bool s_locked;
    static bool s_reported;
    readonly bool _active;

    // The result of the last lock attempt: false when this process has no right to change the foreground, which
    // also means the gallery it starts has none.
    public static bool LastLocked { get; private set; }

    public ForegroundLock(bool enabled)
    {
        _active = enabled;
        if (!enabled)
            return;
        lock (s_sync)
            if (s_depth++ == 0)
                Take();
    }

    static void Take()
    {
        s_locked = LockSetForegroundWindow(LSFW_LOCK);
        LastLocked = s_locked;
        if (!s_locked && !s_reported)
        {
            s_reported = true;
            Gallery.Log("foreground lock not taken (this process may not change the foreground, so the gallery it starts cannot either)");
        }
    }

    public void Dispose()
    {
        if (!_active)
            return;
        lock (s_sync)
        {
            if (--s_depth > 0)
                return;
            if (s_locked)
                LockSetForegroundWindow(LSFW_UNLOCK);
            s_locked = false;
        }
    }

    static readonly object s_sync = new();

    // Gives the foreground back to `hwnd` (the user's window), lifting the lock for the moment it takes.
    public static bool Restore(IntPtr hwnd)
    {
        lock (s_sync)
        {
            if (s_locked)
                LockSetForegroundWindow(LSFW_UNLOCK);
            bool ok = SetForegroundWindow(hwnd);
            if (s_locked)
                s_locked = LockSetForegroundWindow(LSFW_LOCK);
            return ok;
        }
    }
}

internal sealed class OffscreenParker : IDisposable
{
    readonly uint _pid;
    readonly string _windowClass;
    readonly Thread _thread;
    readonly ManualResetEventSlim _ready = new();
    uint _threadId;
    WinEventProc? _proc; // Referenced while the hook exists, so the delegate is not collected
    int _parked;
    IntPtr _userForeground; // The last foreground window that is not the gallery's
    int _restored;

    public int Parked => _parked;

    // How often the gallery took the foreground and the driver gave it back.
    public int Restored => _restored;

    public OffscreenParker(uint pid, string windowClass)
    {
        _pid = pid;
        _windowClass = windowClass;
        _thread = new Thread(Run) { IsBackground = true, Name = "offscreen-parker" };
        _thread.Start();
        _ready.Wait(TimeSpan.FromSeconds(5));
    }

    void Run()
    {
        _threadId = GetCurrentThreadId();
        _proc = OnEvent;
        // Out of context: the events arrive through this thread's message loop.
        IntPtr hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_LOCATIONCHANGE, IntPtr.Zero, _proc, _pid, 0, WINEVENT_OUTOFCONTEXT);
        if (hook == IntPtr.Zero)
            Gallery.Log("SetWinEventHook failed; the gallery window is parked off-screen only after it appears");
        // Foreground changes of every process: if the gallery takes the foreground anyway, give it back.
        IntPtr fgHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, IntPtr.Zero, _proc, 0, 0, WINEVENT_OUTOFCONTEXT);
        IntPtr fg = GetForegroundWindow();
        if (fg != IntPtr.Zero && WindowPid(fg) != _pid)
            _userForeground = fg;
        _ready.Set();
        while (GetMessage(out MSG msg, IntPtr.Zero, 0, 0) > 0)
        {
            TranslateMessage(ref msg);
            DispatchMessage(ref msg);
        }
        if (hook != IntPtr.Zero)
            UnhookWinEvent(hook);
        if (fgHook != IntPtr.Zero)
            UnhookWinEvent(fgHook);
    }

    void OnEvent(IntPtr hook, uint evt, IntPtr hwnd, int idObject, int idChild, uint thread, uint time)
    {
        if (evt == EVENT_SYSTEM_FOREGROUND)
        {
            OnForeground(hwnd);
            return;
        }
        if (idObject != OBJID_WINDOW || idChild != 0 || hwnd == IntPtr.Zero)
            return;
        if (evt != EVENT_OBJECT_CREATE && evt != EVENT_OBJECT_SHOW && evt != EVENT_OBJECT_LOCATIONCHANGE)
            return;
        if (GetAncestor(hwnd, GA_ROOT) != hwnd || GetWindow(hwnd, GW_OWNER) != IntPtr.Zero || ClassName(hwnd) != _windowClass)
            return;
        if (Park(hwnd))
            Interlocked.Increment(ref _parked);
    }

    void OnForeground(IntPtr hwnd)
    {
        if (hwnd == IntPtr.Zero)
            return;
        if (WindowPid(hwnd) != _pid)
        {
            _userForeground = hwnd;
            return;
        }
        IntPtr back = _userForeground;
        if (back == IntPtr.Zero || !IsWindow(back))
            return;
        bool ok = ForegroundLock.Restore(back);
        Interlocked.Increment(ref _restored);
        Gallery.Log($"the gallery took the foreground ({ClassName(hwnd)}); {(ok ? "gave it back" : "could not give it back")} to '{WindowText(back)}'");
    }

    // The virtual screen is the bounding box of all monitors.
    static (int X, int Y, int W, int H) VirtualScreen() =>
        (GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));

    // A top-left corner right of the virtual screen: a window there is on no monitor.
    public static (int X, int Y) ParkPosition()
    {
        var v = VirtualScreen();
        return (v.X + v.W + 64, v.Y);
    }

    public static bool IsOnAnyMonitor(IntPtr hwnd)
    {
        if (!GetWindowRect(hwnd, out RECT r))
            return false;
        var v = VirtualScreen();
        return r.Right > v.X && r.Left < v.X + v.W && r.Bottom > v.Y && r.Top < v.Y + v.H;
    }

    // Moves a window off every monitor without activating it. Returns false when it already was.
    public static bool Park(IntPtr hwnd)
    {
        if (!IsOnAnyMonitor(hwnd))
            return false;
        var (x, y) = ParkPosition();
        return SetWindowPos(hwnd, IntPtr.Zero, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    public void Dispose()
    {
        if (_threadId != 0)
            PostThreadMessage(_threadId, WM_QUIT, IntPtr.Zero, IntPtr.Zero);
        _thread.Join(TimeSpan.FromSeconds(2));
    }
}
