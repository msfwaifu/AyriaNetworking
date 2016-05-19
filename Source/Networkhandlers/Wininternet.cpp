/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-5-19
    Notes:
        Replaces the WININET API exports.
*/

#include <Networkhandlers\Wininternet.h>
#include <Configuration\All.h>
#include <Servers\IServer.h>
#include <unordered_map>

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
        return InternetCloseHandle(hInternet);
    }
    HINTERNET __stdcall Connect(HINTERNET hInternet, LPCSTR lpszServerName, INTERNET_PORT nServerPort, LPCSTR lpszUserName, LPCSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext)
    {
        DebugPrint(va("%s: %s:%i %s:%s", __FUNCTION__, lpszServerName, nServerPort, lpszUserName, lpszPassword));
        return InternetConnectA(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);
    }
    BOOL __stdcall Writefile(HINTERNET hFile, LPCVOID lpBuffer, DWORD dwNumberOfBytesToWrite, LPDWORD lpdwNumberOfBytesWritten)
    {
        MessageBoxA(0, __FUNCTION__, 0, 0);
        return InternetWriteFile(hFile, lpBuffer, dwNumberOfBytesToWrite, lpdwNumberOfBytesWritten);
    }
    HINTERNET __stdcall Openrequest(HINTERNET hConnect, LPCSTR lpszVerb, LPCSTR lpszObjectName, LPCSTR lpszVersion, LPCSTR lpszReferrer, LPCSTR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext)
    {
        DebugPrint(va("%s: %s %s %s", __FUNCTION__, lpszVerb, lpszObjectName, lpszReferrer));
        return HttpOpenRequestA(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
    }
    BOOL __stdcall AddRequestHeaders(HINTERNET hRequest, LPCSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwModifiers)
    {
        MessageBoxA(0, __FUNCTION__, 0, 0);
        return HttpAddRequestHeadersA(hRequest, lpszHeaders, dwHeadersLength, dwModifiers);
    }
    BOOL __stdcall SendRequestEx(HINTERNET hRequest, LPINTERNET_BUFFERSA lpBuffersIn, LPINTERNET_BUFFERSA lpBuffersOut, DWORD dwFlags, DWORD_PTR dwContext)
    {
        MessageBoxA(0, __FUNCTION__, 0, 0);
        return HttpSendRequestExA(hRequest, lpBuffersIn, lpBuffersOut, dwFlags, dwContext);
    }
    BOOL __stdcall EndRequest(HINTERNET hRequest, LPINTERNET_BUFFERSA lpBuffersOut, DWORD dwFlags, DWORD_PTR dwContext)
    {
        MessageBoxA(0, __FUNCTION__, 0, 0);
        return HttpEndRequestA(hRequest, lpBuffersOut, dwFlags, dwContext);
    }
    BOOL __stdcall QueryInfo(HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex)
    {
        MessageBoxA(0, __FUNCTION__, 0, 0);
        return HttpQueryInfoA(hRequest, dwInfoLevel, lpBuffer, lpdwBufferLength, lpdwIndex);
    }
    DWORD __stdcall AttemptConnect(DWORD dwReserved)
    {
        MessageBoxA(0, __FUNCTION__, 0, 0);
        return InternetAttemptConnect(dwReserved);
    }
    HINTERNET __stdcall Open(LPCSTR lpszAgent, DWORD dwAccessType, LPCSTR lpszProxy, LPCSTR lpszProxyBypass, DWORD dwFlags)
    {
        MessageBoxA(0, __FUNCTION__, 0, 0);
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
