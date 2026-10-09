using System.Collections.Concurrent;

namespace WinUiMcp;

// One STA thread that owns every UI Automation object. Tool calls are marshalled onto it one at a time.
// With a desktop handle the thread runs on that desktop (see HiddenDesktop): UI Automation only sees windows of
// its own desktop.
internal sealed class StaDispatcher : IDisposable
{
    readonly BlockingCollection<Action> _queue = new();
    readonly Thread _thread;
    readonly IntPtr _desktop;

    public StaDispatcher(IntPtr desktop = default)
    {
        _desktop = desktop;
        _thread = new Thread(Run) { IsBackground = true, Name = "uia-sta" };
        // A thread started as STA already owns COM's hidden window, and SetThreadDesktop refuses a thread with
        // windows (ERROR_BUSY). On a desktop the thread starts without an apartment and enters the STA itself.
        if (desktop == IntPtr.Zero)
            _thread.SetApartmentState(ApartmentState.STA);
        _thread.Start();
    }

    [System.Runtime.InteropServices.DllImport("ole32.dll")]
    static extern int CoInitializeEx(IntPtr reserved, uint coInit);

    void Run()
    {
        if (_desktop != IntPtr.Zero)
        {
            if (!Native.SetThreadDesktop(_desktop))
                Gallery.Log($"SetThreadDesktop failed (error {System.Runtime.InteropServices.Marshal.GetLastWin32Error()})");
            CoInitializeEx(IntPtr.Zero, 0x2); // COINIT_APARTMENTTHREADED
        }
        foreach (var work in _queue.GetConsumingEnumerable())
            work();
    }

    public T Invoke<T>(Func<T> func, TimeSpan timeout)
    {
        var tcs = new TaskCompletionSource<T>(TaskCreationOptions.RunContinuationsAsynchronously);
        _queue.Add(() =>
        {
            try { tcs.SetResult(func()); }
            catch (Exception e) { tcs.SetException(e); }
        });
        if (!((IAsyncResult)tcs.Task).AsyncWaitHandle.WaitOne(timeout))
            throw new TimeoutException("timed out waiting for the UI Automation thread");
        return tcs.Task.GetAwaiter().GetResult();
    }

    public void Dispose() => _queue.CompleteAdding();
}
