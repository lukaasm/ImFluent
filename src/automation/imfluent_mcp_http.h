#pragma once

// Minimal HTTP/1.1 server for the MCP "Streamable HTTP" transport.
// - Binds to 127.0.0.1 only and rejects browser requests from non-local origins.
// - POST <path> carries one JSON-RPC message (or a batch); the handler returns the JSON reply.
//   An empty reply means "notification accepted" and is sent as 202.
// - GET/DELETE return 405: the server never opens an SSE stream.
// One thread accepts connections; each connection is served on its own short-lived thread.

#include <atomic>
#include <functional>
#include <string>
#include <thread>

struct ImFluentMcpHttpServer
{
    typedef std::function< std::string( const std::string & body ) > Handler;

    bool Start( int port, const char * path, Handler handler );
    void Stop();
    bool IsRunning() const
    {
        return Running.load();
    }
    int GetPort() const
    {
        return Port;
    }

    ~ImFluentMcpHttpServer()
    {
        Stop();
    }

private:
    void AcceptLoop();
    void ServeClient( intptr_t client );

    Handler RequestHandler;
    std::string Path;
    int Port              = 0;
    intptr_t ListenSocket = -1;
    std::atomic< bool > Running{ false };
    std::thread AcceptThread;
    std::atomic< int > ActiveClients{ 0 };
};
