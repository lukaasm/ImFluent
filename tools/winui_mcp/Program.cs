using static WinUiMcp.Native;

namespace WinUiMcp;

internal static class Program
{
    public static string? RepoRoot;

    const string DefaultExeRelative = "docs/winui_gallery/WinUIGallery/bin/WinUIGallery/x64/Debug-Unpackaged/net9.0-windows10.0.22621.0/win-x64/WinUIGallery.exe";

    static int Main(string[] args)
    {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        Console.OutputEncoding = System.Text.Encoding.UTF8;

        int port = 7802;
        string? exe = null;
        bool attach = false;
        bool interactive = false;
        bool sameDesktop = false;
        int dumpDepth = -1;
        for (int i = 0; i < args.Length; ++i)
        {
            switch (args[i])
            {
                case "--port" when i + 1 < args.Length: port = int.Parse(args[++i]); break;
                case "--exe" when i + 1 < args.Length: exe = args[++i]; break;
                case "--attach": attach = true; break;
                case "--interactive": interactive = true; break;
                case "--same-desktop": sameDesktop = true; break;
                case "--dump":
                    dumpDepth = 12;
                    if (i + 1 < args.Length && int.TryParse(args[i + 1], out int d))
                    {
                        dumpDepth = d;
                        ++i;
                    }
                    break;
                case "-h":
                case "--help":
                    Console.WriteLine("WinUiMcp [--port 7802] [--exe <WinUIGallery.exe>] [--attach] [--interactive | --same-desktop] [--dump [depth]]");
                    Console.WriteLine("  default         background mode: the gallery runs on a hidden desktop; UIA patterns, posted keys, PrintWindow.");
                    Console.WriteLine("  --same-desktop  background mode on your desktop: the gallery is parked off-screen and the driver gives");
                    Console.WriteLine("                  the foreground back if the gallery takes it.");
                    Console.WriteLine("  --interactive   allow real mouse/keyboard input (SendInput) and bringing the gallery to the front.");
                    return 0;
                default:
                    Console.Error.WriteLine($"unknown argument '{args[i]}' (see --help)");
                    return 2;
            }
        }

        RepoRoot = FindRepoRoot(AppContext.BaseDirectory) ?? FindRepoRoot(Environment.CurrentDirectory);
        exe ??= RepoRoot != null ? Path.GetFullPath(Path.Combine(RepoRoot, DefaultExeRelative)) : DefaultExeRelative;

        bool ownDesktop = !interactive && !sameDesktop && HiddenDesktop.Create();
        using var sta = new StaDispatcher(ownDesktop ? HiddenDesktop.Handle : IntPtr.Zero);
        Tools? tools = null;
        try
        {
            tools = sta.Invoke(() =>
            {
                var gallery = new Gallery(exe, attach, interactive, ownDesktop);
                gallery.Connect(TimeSpan.FromSeconds(90));
                return new Tools(gallery);
            }, TimeSpan.FromSeconds(120));
        }
        catch (Exception e)
        {
            Console.Error.WriteLine("[winui_mcp] " + e.Message);
            return 1;
        }

        if (dumpDepth >= 0)
        {
            Console.WriteLine(sta.Invoke(() => tools.DumpTree(dumpDepth), TimeSpan.FromSeconds(120)));
            return 0;
        }

        var mcp = new McpProtocol(sta, tools);
        var server = new McpHttpServer(port, "/mcp", mcp.HandleBody);
        try
        {
            server.Start();
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"[winui_mcp] could not bind 127.0.0.1:{port}: {e.Message}");
            return 1;
        }
        string mode = interactive ? "interactive: real mouse/keyboard input"
            : ownDesktop ? $"background mode on the hidden desktop {HiddenDesktop.FullName}"
            : "background mode on this desktop: off-screen, foreground given back";
        Console.WriteLine($"[winui_mcp] MCP server listening on http://127.0.0.1:{port}/mcp ({mode})");
        Console.Out.Flush();

        var quit = new ManualResetEventSlim();
        Console.CancelKeyPress += (_, e) => { e.Cancel = true; quit.Set(); };
        quit.Wait();
        server.Stop();
        return 0;
    }

    // Walks up to the directory that contains vcpkg.json (the ImFluent repository root).
    static string? FindRepoRoot(string start)
    {
        for (var dir = new DirectoryInfo(start); dir != null; dir = dir.Parent)
            if (File.Exists(Path.Combine(dir.FullName, "vcpkg.json")))
                return dir.FullName;
        return null;
    }
}
