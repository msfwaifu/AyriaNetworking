/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-5-19
    Notes:
        Replaces the WININET API exports.
        HINTERNET handles can be treated as sockets.
*/

#include <Networkhandlers\Wininternet.h>
#include <Configuration\All.h>
#include <Servers\IServer.h>
#include <unordered_map>
#include <WinSock2.h>

// Maps over the sockets and servers.
static std::unordered_map<uint32_t /* Address */, IServer *> Servermap;
static std::unordered_map<size_t /* Socket */, IServer *> Socketmap;

// Simplified lookup methods.
static IServer *FindByAddress(const uint32_t Address)
{
    for(auto Iterator = Servermap.begin(); Iterator != Servermap.end(); ++Iterator)
        if(uint32_t(Iterator->first) == Address)
            return Iterator->second;

    return nullptr;
}
static IServer *FindBySocket(const size_t Socket)
{
    auto Result = Socketmap.find(Socket);
    if(Result != Socketmap.end())
        return Result->second;

    return nullptr;
}
static IServer *FindByName(std::string Hostname)
{
    for (auto Iterator = Servermap.begin(); Iterator != Servermap.end(); ++Iterator)
        if (0 == std::strcmp(Iterator->second->GetServerinfo()->Hostname, Hostname.c_str()))
            return Iterator->second;

    return nullptr;
}

#ifdef _WIN32
#include <Windows.h>
#include <Wininet.h>

namespace INETReplacement
{
    BOOL __stdcall Closehandle(HINTERNET hInternet)
    {
        IServer *Server;

        // Find the server if we have one.
        Server = FindBySocket(size_t(hInternet));
        if (Server)
        {
            NetworkPrint(va("%s for server \"%s\"", __func__, Server->GetServerinfo()->Hostname));
            Socketmap.erase(size_t(hInternet));

            if (Server->GetServerinfo()->Extendedserver)
                ((IServerEx *)Server)->onDisconnect(size_t(hInternet));
        }

        return InternetCloseHandle(hInternet);
    }
    HINTERNET __stdcall Connect(HINTERNET hInternet, LPCSTR lpszServerName, INTERNET_PORT nServerPort, LPCSTR lpszUserName, LPCSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext)
    {
        IServer *Server;

        // Log this event.
        DebugPrint(va("%s: %s:%i %s:%s", __FUNCTION__, lpszServerName, nServerPort, lpszUserName, lpszPassword));

        // Find the server in our list.
        Server = FindByName(lpszServerName);
        if(!Server) Server = FindByAddress(uint32_t(inet_addr(lpszServerName)));
        if(!Server) return InternetConnectA(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);

        // Create a random socket and add it to the map.
        size_t Socket;
        do
        {
            Socket = std::rand();
        } while (Socketmap.find(Socket) != Socketmap.end());
        Socketmap[Socket] = Server;

        // Notify the server about this connection.
        if (Server->GetServerinfo()->Extendedserver)
            ((IServerEx *)Server)->onConnect(Socket, nServerPort);

        return HINTERNET(Socket);
    }
    BOOL __stdcall Writefile(HINTERNET hFile, LPCVOID lpBuffer, DWORD dwNumberOfBytesToWrite, LPDWORD lpdwNumberOfBytesWritten)
    {
        IServer *Server;
        *lpdwNumberOfBytesWritten = 0;

        // Find the server in our list.
        Server = FindBySocket(size_t(hFile));
        if (!Server)
        {
            // Log this event.
            NetworkPrint(va("%s: Writing %u bytes on socket 0x%p", __FUNCTION__, dwNumberOfBytesToWrite, hFile));
#ifdef WININET_LOCALONLY
            return TRUE;
#else
            return InternetWriteFile(hFile, lpBuffer, dwNumberOfBytesToWrite, lpdwNumberOfBytesWritten);
#endif
        }

        // Log this event.
        NetworkPrint(va("%s: Writing %u bytes to \"%s\"", __FUNCTION__, dwNumberOfBytesToWrite, Server->GetServerinfo()->Hostname));

        // Send the data to a server, this should always be handled.
        if (Server->GetServerinfo()->Extendedserver)
        {
            IServerEx *Extended = (IServerEx *)Server;
            if (false == Extended->onWriterequestEx(size_t(hFile), (const char *)lpBuffer, size_t(dwNumberOfBytesToWrite)))
                return FALSE;
        }
        else
        {
            if (false == Server->onWriterequest((const char *)lpBuffer, size_t(dwNumberOfBytesToWrite)))
                return FALSE;
        }

        *lpdwNumberOfBytesWritten = dwNumberOfBytesToWrite;
        return TRUE;
    }
    HINTERNET __stdcall Openrequest(HINTERNET hConnect, LPCSTR lpszVerb, LPCSTR lpszObjectName, LPCSTR lpszVersion, LPCSTR lpszReferrer, LPCSTR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext)
    {
        // Create a readable request.
        std::string Request;
        {
            if (lpszVerb) Request.append(lpszVerb);
            else Request.append("GET");
            Request.append(" ");

            Request.append(lpszObjectName);
            Request.append(" ");

            if (lpszVersion) Request.append(lpszVersion);
            else Request.append("HTTP/1.1");
            Request.append("\r\n");

            if (lpszReferrer)
            {
                // Spelling as per rfc1945
                Request.append("Referer: ");
                Request.append(lpszReferrer);
                Request.append("\r\n");
            }

            NetworkPrint(va("%s: %s", __FUNCTION__, Request.c_str()));
        }

        IServer *Server;
        Server = FindBySocket(size_t(hConnect));
        if (!Server)
        {
#ifdef WININET_LOCALONLY
            return hConnect;
#else
            return HttpOpenRequestA(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
#endif
        }
        
        // We send the partial header directly to the server, they can decide how to deal with it.
        if (Server->GetServerinfo()->Extendedserver)
        {
            IServerEx *Extended = (IServerEx *)Server;
            if (false == Extended->onWriterequestEx(size_t(hConnect), Request.c_str(), Request.size()))
                return NULL;
        }
        else
        {
            if (false == Server->onWriterequest(Request.c_str(), Request.size()))
                return NULL;
        }

        return hConnect;
    }
    BOOL __stdcall AddRequestHeaders(HINTERNET hRequest, LPCSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwModifiers)
    {
        // Create a readable string of the header.
        std::string Header;
        if (dwHeadersLength) Header.append(lpszHeaders, dwHeadersLength);
        else Header.append(lpszHeaders);
        NetworkPrint(va("%s: %s", __FUNCTION__, Header.c_str()));

        IServer *Server;
        Server = FindBySocket(size_t(hRequest));
        if (!Server) return HttpAddRequestHeadersA(hRequest, lpszHeaders, dwHeadersLength, dwModifiers);

        // We send the partial header directly to the server, they can decide how to deal with it.
        if (Server->GetServerinfo()->Extendedserver)
        {
            IServerEx *Extended = (IServerEx *)Server;
            if (false == Extended->onWriterequestEx(size_t(hRequest), Header.c_str(), Header.size()))
                return FALSE;
        }
        else
        {
            if (false == Server->onWriterequest(Header.c_str(), Header.size()))
                return FALSE;
        }

        return TRUE;
    }
    BOOL __stdcall SendRequestEx(HINTERNET hRequest, LPINTERNET_BUFFERSA lpBuffersIn, LPINTERNET_BUFFERSA lpBuffersOut, DWORD dwFlags, DWORD_PTR dwContext)
    {
        // As we send and receive the data when available, this function does nothing.
        if (FindBySocket(size_t(hRequest)))
        {
#ifdef WININET_LOCALONLY
            return TRUE;
#else
            return HttpSendRequestExA(hRequest, lpBuffersIn, lpBuffersOut, dwFlags, dwContext);
#endif
        }
        else return TRUE;
    }
    BOOL __stdcall EndRequest(HINTERNET hRequest, LPINTERNET_BUFFERSA lpBuffersOut, DWORD dwFlags, DWORD_PTR dwContext)
    {
        if (lpBuffersOut)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        // As we send and receive the data when available, this function does nothing.
        if (!FindBySocket(size_t(hRequest)))
        {
#ifdef WININET_LOCALONLY
            return TRUE;
#else
            return HttpEndRequestA(hRequest, lpBuffersOut, dwFlags, dwContext);
#endif
        }
        else return TRUE;
    }
    BOOL __stdcall QueryInfo(HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex)
    {
        // We don't track this data, when there's a need for it we can create a map.
        if (!FindBySocket(size_t(hRequest)))
        {
#ifdef WININET_LOCALONLY
            return TRUE;
#else
            return HttpQueryInfoA(hRequest, dwInfoLevel, lpBuffer, lpdwBufferLength, lpdwIndex);
#endif
        }
        else return TRUE;
    }
    DWORD __stdcall AttemptConnect(DWORD dwReserved)
    {
        return InternetAttemptConnect(dwReserved);
    }
    HINTERNET __stdcall Open(LPCSTR lpszAgent, DWORD dwAccessType, LPCSTR lpszProxy, LPCSTR lpszProxyBypass, DWORD dwFlags)
    {
        return InternetOpenA(lpszAgent, dwAccessType, lpszProxy, lpszProxyBypass, dwFlags);
    }
}

#endif

namespace Wininternet
{
    void Initializehandler()
    {
#define PATCH_WININET_IAT(Export, Function)                         \
        Address = GetIATFunction("WININET.dll", Export);            \
        if(Address) *(size_t *)Address = size_t(Function);          \

        size_t Address;
        PATCH_WININET_IAT("InternetCloseHandle", INETReplacement::Closehandle);
        PATCH_WININET_IAT("InternetConnectA", INETReplacement::Connect);
        PATCH_WININET_IAT("InternetWriteFile", INETReplacement::Writefile);
        PATCH_WININET_IAT("HttpOpenRequestA", INETReplacement::Openrequest);
        PATCH_WININET_IAT("HttpAddRequestHeadersA", INETReplacement::AddRequestHeaders);
        PATCH_WININET_IAT("HttpSendRequestExA", INETReplacement::SendRequestEx);
        PATCH_WININET_IAT("HttpEndRequestA", INETReplacement::EndRequest);
        PATCH_WININET_IAT("HttpQueryInfoA", INETReplacement::QueryInfo);
        PATCH_WININET_IAT("InternetAttemptConnect", INETReplacement::AttemptConnect);
        PATCH_WININET_IAT("InternetOpenA", INETReplacement::Open);
    }
    void Registerserver(IServer *Server)
    {
        Servermap[Server->GetServerinfo()->Hostaddress] = Server;
    }
}
