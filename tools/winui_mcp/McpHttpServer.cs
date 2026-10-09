using System.Net;
using System.Net.Sockets;
using System.Text;

namespace WinUiMcp;

// Minimal HTTP/1.1 server for the MCP Streamable HTTP transport with plain JSON replies (no SSE).
// Same behavior as src/automation/imfluent_mcp_http.cpp: POST <path> -> 200 + JSON, or 202 for notifications;
// other methods -> 405; non-loopback Origin -> 403.
internal sealed class McpHttpServer
{
    const int MaxHeaderBytes = 64 * 1024;
    const int MaxBodyBytes = 16 * 1024 * 1024;

    readonly int _port;
    readonly string _path;
    readonly Func<string, string> _handler;
    TcpListener? _listener;
    volatile bool _running;

    public McpHttpServer(int port, string path, Func<string, string> handler)
    {
        _port = port;
        _path = path;
        _handler = handler;
    }

    public void Start()
    {
        _listener = new TcpListener(IPAddress.Loopback, _port);
        _listener.Start(16);
        _running = true;
        var t = new Thread(AcceptLoop) { IsBackground = true, Name = "mcp-accept" };
        t.Start();
    }

    public void Stop()
    {
        _running = false;
        try { _listener?.Stop(); } catch { }
    }

    void AcceptLoop()
    {
        while (_running)
        {
            TcpClient client;
            try { client = _listener!.AcceptTcpClient(); }
            catch { if (!_running) break; continue; }
            var t = new Thread(() => Serve(client)) { IsBackground = true, Name = "mcp-client" };
            t.Start();
        }
    }

    static bool IsOriginAllowed(string? origin)
    {
        if (string.IsNullOrEmpty(origin))
            return true;
        string[] allowed = { "http://localhost", "https://localhost", "http://127.0.0.1", "https://127.0.0.1", "http://[::1]", "https://[::1]" };
        foreach (string prefix in allowed)
            if (origin.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) && (origin.Length == prefix.Length || origin[prefix.Length] == ':' || origin[prefix.Length] == '/'))
                return true;
        return false;
    }

    static void SendResponse(Stream s, int status, string reason, string body, bool keepAlive)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(body);
        string header = $"HTTP/1.1 {status} {reason}\r\nContent-Type: application/json\r\nContent-Length: {bytes.Length}\r\nCache-Control: no-store\r\nConnection: {(keepAlive ? "keep-alive" : "close")}\r\n\r\n";
        s.Write(Encoding.ASCII.GetBytes(header));
        if (bytes.Length > 0)
            s.Write(bytes);
        s.Flush();
    }

    void Serve(TcpClient client)
    {
        using (client)
        {
            try
            {
                client.ReceiveTimeout = 120_000;
                var s = client.GetStream();
                var buf = new List<byte>(8192);
                var tmp = new byte[16384];
                bool keepAlive = true;
                while (keepAlive && _running)
                {
                    int headerEnd;
                    while ((headerEnd = IndexOfHeaderEnd(buf)) < 0)
                    {
                        int n = s.Read(tmp, 0, tmp.Length);
                        if (n <= 0)
                            return;
                        buf.AddRange(new ArraySegment<byte>(tmp, 0, n));
                        if (buf.Count > MaxHeaderBytes)
                            return;
                    }
                    string head = Encoding.ASCII.GetString(buf.GetRange(0, headerEnd).ToArray());
                    string[] lines = head.Split("\r\n");
                    string[] req = lines[0].Split(' ');
                    if (req.Length < 3)
                        return;
                    string method = req[0], target = req[1];
                    keepAlive = req[2] != "HTTP/1.0";
                    int contentLength = 0;
                    string? origin = null;
                    for (int i = 1; i < lines.Length; ++i)
                    {
                        int colon = lines[i].IndexOf(':');
                        if (colon < 0)
                            continue;
                        string name = lines[i][..colon].Trim();
                        string value = lines[i][(colon + 1)..].Trim();
                        if (name.Equals("content-length", StringComparison.OrdinalIgnoreCase))
                            int.TryParse(value, out contentLength);
                        else if (name.Equals("origin", StringComparison.OrdinalIgnoreCase))
                            origin = value;
                        else if (name.Equals("connection", StringComparison.OrdinalIgnoreCase))
                            keepAlive = !value.StartsWith("close", StringComparison.OrdinalIgnoreCase);
                    }
                    if (contentLength > MaxBodyBytes || contentLength < 0)
                    {
                        SendResponse(s, 413, "Payload Too Large", "", false);
                        return;
                    }
                    int bodyStart = headerEnd + 4;
                    while (buf.Count < bodyStart + contentLength)
                    {
                        int n = s.Read(tmp, 0, tmp.Length);
                        if (n <= 0)
                            return;
                        buf.AddRange(new ArraySegment<byte>(tmp, 0, n));
                    }
                    string body = Encoding.UTF8.GetString(buf.GetRange(bodyStart, contentLength).ToArray());
                    buf.RemoveRange(0, bodyStart + contentLength);

                    int q = target.IndexOf('?');
                    if (q >= 0)
                        target = target[..q];

                    if (!IsOriginAllowed(origin))
                        SendResponse(s, 403, "Forbidden", "{\"error\":\"origin not allowed\"}", keepAlive);
                    else if (target != _path)
                        SendResponse(s, 404, "Not Found", "{\"error\":\"unknown path\"}", keepAlive);
                    else if (method == "POST")
                    {
                        string reply = _handler(body);
                        if (reply.Length == 0)
                            SendResponse(s, 202, "Accepted", "", keepAlive);
                        else
                            SendResponse(s, 200, "OK", reply, keepAlive);
                    }
                    else
                        SendResponse(s, 405, "Method Not Allowed", "", keepAlive);
                }
            }
            catch (IOException) { }
            catch (SocketException) { }
            catch (ObjectDisposedException) { }
        }
    }

    static int IndexOfHeaderEnd(List<byte> buf)
    {
        for (int i = 0; i + 3 < buf.Count; ++i)
            if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n')
                return i;
        return -1;
    }
}
