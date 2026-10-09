#include "imfluent_mcp_http.h"

#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment( lib, "ws2_32.lib" )
typedef SOCKET ImSocket;
typedef int ImSockLen;
#define IM_INVALID_SOCKET INVALID_SOCKET
static void CloseSocket( ImSocket s )
{
    closesocket( s );
}
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int ImSocket;
typedef socklen_t ImSockLen;
#define IM_INVALID_SOCKET ( -1 )
static void CloseSocket( ImSocket s )
{
    close( s );
}
#endif

static const size_t kMaxHeaderBytes = 64 * 1024;
static const size_t kMaxBodyBytes   = 16 * 1024 * 1024;

static bool SendAll( ImSocket s, const char * data, size_t len )
{
    while ( len > 0 )
    {
        const int n = ( int )send( s, data, ( int )len, 0 );
        if ( n <= 0 )
            return false;
        data += n;
        len -= ( size_t )n;
    }
    return true;
}

static void SendResponse( ImSocket s, int status, const char * reason, const std::string & body, bool keep_alive )
{
    char header[ 512 ];
    const int header_len = snprintf( header, sizeof( header ),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: %s\r\n"
        "\r\n",
        status, reason, body.size(), keep_alive ? "keep-alive" : "close" );
    if ( !SendAll( s, header, ( size_t )header_len ) )
        return;
    if ( !body.empty() )
        SendAll( s, body.data(), body.size() );
}

static bool EqualsNoCase( const char * a, const char * b, size_t n )
{
    for ( size_t i = 0; i < n; ++i )
    {
        char ca = a[ i ], cb = b[ i ];
        if ( ca >= 'A' && ca <= 'Z' )
            ca = ( char )( ca - 'A' + 'a' );
        if ( cb >= 'A' && cb <= 'Z' )
            cb = ( char )( cb - 'A' + 'a' );
        if ( ca != cb )
            return false;
    }
    return true;
}

// DNS rebinding guard: browsers send Origin, which must be loopback; local tools usually send none.
static bool IsOriginAllowed( const std::string & origin )
{
    if ( origin.empty() )
        return true;
    const char * allowed[] = { "http://localhost", "https://localhost", "http://127.0.0.1", "https://127.0.0.1", "http://[::1]", "https://[::1]" };
    for ( const char * prefix : allowed )
    {
        const size_t n = strlen( prefix );
        if ( origin.size() >= n && EqualsNoCase( origin.c_str(), prefix, n ) && ( origin.size() == n || origin[ n ] == ':' || origin[ n ] == '/' ) )
            return true;
    }
    return false;
}

bool ImFluentMcpHttpServer::Start( int port, const char * path, Handler handler )
{
    if ( Running.load() )
        return true;
#if defined( _WIN32 )
    WSADATA wsa;
    if ( WSAStartup( MAKEWORD( 2, 2 ), &wsa ) != 0 )
        return false;
#endif
    ImSocket s = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( s == IM_INVALID_SOCKET )
        return false;

    sockaddr_in addr     = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons( ( unsigned short )port );
    addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
    if ( bind( s, ( sockaddr * )&addr, sizeof( addr ) ) != 0 || listen( s, 8 ) != 0 )
    {
        CloseSocket( s );
        return false;
    }

    RequestHandler = handler;
    Path           = path ? path : "/mcp";
    Port           = port;
    ListenSocket   = ( intptr_t )s;
    Running.store( true );
    AcceptThread = std::thread( &ImFluentMcpHttpServer::AcceptLoop, this );
    return true;
}

void ImFluentMcpHttpServer::Stop()
{
    if ( !Running.exchange( false ) )
        return;
    if ( ListenSocket != -1 )
    {
        CloseSocket( ( ImSocket )ListenSocket );
        ListenSocket = -1;
    }
    if ( AcceptThread.joinable() )
        AcceptThread.join();
    // Client threads poll Running every second (SO_RCVTIMEO).
    for ( int i = 0; i < 300 && ActiveClients.load() > 0; ++i )
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
#if defined( _WIN32 )
    WSACleanup();
#endif
}

void ImFluentMcpHttpServer::AcceptLoop()
{
    while ( Running.load() )
    {
        sockaddr_in client_addr = {};
        ImSockLen len           = sizeof( client_addr );
        ImSocket c              = accept( ( ImSocket )ListenSocket, ( sockaddr * )&client_addr, &len );
        if ( c == IM_INVALID_SOCKET )
        {
            if ( !Running.load() )
                break;
            continue;
        }
        ActiveClients.fetch_add( 1 );
        std::thread( &ImFluentMcpHttpServer::ServeClient, this, ( intptr_t )c ).detach();
    }
}

void ImFluentMcpHttpServer::ServeClient( intptr_t client )
{
    ImSocket s = ( ImSocket )client;

    // Wake up every second so Stop() can join us.
#if defined( _WIN32 )
    DWORD timeout_ms = 1000;
    setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, ( const char * )&timeout_ms, sizeof( timeout_ms ) );
#else
    timeval tv = { 1, 0 };
    setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) );
#endif

    std::string buf;
    int idle_seconds = 0;
    bool keep_alive  = true;
    while ( keep_alive && Running.load() )
    {
        size_t header_end = buf.find( "\r\n\r\n" );
        while ( header_end == std::string::npos )
        {
            char tmp[ 4096 ];
            const int n = ( int )recv( s, tmp, sizeof( tmp ), 0 );
            if ( n == 0 )
                goto done;
            if ( n < 0 )
            {
                if ( !Running.load() || ++idle_seconds > 120 )
                    goto done;
                continue;
            }
            idle_seconds = 0;
            buf.append( tmp, ( size_t )n );
            if ( buf.size() > kMaxHeaderBytes )
                goto done;
            header_end = buf.find( "\r\n\r\n" );
        }

        std::string method, target, origin;
        size_t content_length = 0;
        {
            const std::string head         = buf.substr( 0, header_end );
            size_t line_end                = head.find( "\r\n" );
            const std::string request_line = head.substr( 0, line_end );
            const size_t sp1               = request_line.find( ' ' );
            const size_t sp2               = request_line.find( ' ', sp1 + 1 );
            if ( sp1 == std::string::npos || sp2 == std::string::npos )
                goto done;
            method     = request_line.substr( 0, sp1 );
            target     = request_line.substr( sp1 + 1, sp2 - sp1 - 1 );
            keep_alive = request_line.compare( sp2 + 1, std::string::npos, "HTTP/1.0" ) != 0;
            while ( line_end != std::string::npos )
            {
                const size_t start     = line_end + 2;
                line_end               = head.find( "\r\n", start );
                const std::string line = head.substr( start, line_end == std::string::npos ? std::string::npos : line_end - start );
                const size_t colon     = line.find( ':' );
                if ( colon == std::string::npos )
                    continue;
                std::string value = line.substr( colon + 1 );
                while ( !value.empty() && ( value[ 0 ] == ' ' || value[ 0 ] == '\t' ) )
                    value.erase( 0, 1 );
                if ( colon == 14 && EqualsNoCase( line.c_str(), "content-length", 14 ) )
                    content_length = ( size_t )strtoull( value.c_str(), NULL, 10 );
                else if ( colon == 6 && EqualsNoCase( line.c_str(), "origin", 6 ) )
                    origin = value;
                else if ( colon == 10 && EqualsNoCase( line.c_str(), "connection", 10 ) )
                    keep_alive = !( value.size() >= 5 && EqualsNoCase( value.c_str(), "close", 5 ) );
            }
        }
        if ( content_length > kMaxBodyBytes )
        {
            SendResponse( s, 413, "Payload Too Large", "", false );
            goto done;
        }

        const size_t body_start = header_end + 4;
        while ( buf.size() < body_start + content_length )
        {
            char tmp[ 16384 ];
            const int n = ( int )recv( s, tmp, sizeof( tmp ), 0 );
            if ( n == 0 )
                goto done;
            if ( n < 0 )
            {
                if ( !Running.load() )
                    goto done;
                continue;
            }
            buf.append( tmp, ( size_t )n );
        }
        const std::string body = buf.substr( body_start, content_length );
        buf.erase( 0, body_start + content_length );

        const size_t query = target.find( '?' );
        if ( query != std::string::npos )
            target.resize( query );

        if ( !IsOriginAllowed( origin ) )
            SendResponse( s, 403, "Forbidden", "{\"error\":\"origin not allowed\"}", keep_alive );
        else if ( target != Path )
            SendResponse( s, 404, "Not Found", "{\"error\":\"unknown path\"}", keep_alive );
        else if ( method == "POST" )
        {
            const std::string reply = RequestHandler( body );
            if ( reply.empty() )
                SendResponse( s, 202, "Accepted", "", keep_alive );
            else
                SendResponse( s, 200, "OK", reply, keep_alive );
        }
        else
            SendResponse( s, 405, "Method Not Allowed", "", keep_alive );
    }
done:
    CloseSocket( s );
    ActiveClients.fetch_sub( 1 );
}