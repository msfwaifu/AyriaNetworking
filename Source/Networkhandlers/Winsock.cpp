/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-4-24
    Notes:
        Replaces the WS2_32 and wsock32 API exports.
*/

#include <Networkhandlers\Winsock.h>
#include <Configuration\All.h>
#include <Servers\IServer.h>
#include <unordered_map>
#include <algorithm>
#include <thread>

// Maps over the sockets and servers.
static std::unordered_map<uint32_t /* Address */, IServer *> Servermap;
static std::unordered_map<size_t /* Socket */, IServer *> Socketmap;
static std::unordered_map<size_t /* Socket */, bool> Blockingmap;

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
#include <WinSock2.h>
#include <ws2tcpip.h>
#undef min
#undef max

namespace Winsock
{
    bool ProxyInternalrange = false;

    // Swap IPs for internal networking when needed, for IPv4 192.
    void ReplaceAddress(sockaddr_in *Address)
    {
        if (!ProxyInternalrange)
            return;

        if (192 == *(uint8_t *)&Address->sin_addr.S_un.S_addr)
            *(uint8_t *)&Address->sin_addr.S_un.S_addr = 240;
        else
            if (240 == *(uint8_t *)&Address->sin_addr.S_un.S_addr)
                *(uint8_t *)&Address->sin_addr.S_un.S_addr = 192;
    }

    // Stomphooked functions.
    std::unordered_map<std::string, void *> Stomphooks;
}

namespace WSReplacement
{
    // Call the stomphook if needed.
    #define CALLWSHOOK(Function, Result, ...)                                               \
    auto _Pointer = Winsock::Stomphooks[__FUNCTION__];                                      \
    if(_Pointer)                                                                            \
    {                                                                                       \
        auto _Function = ((StomphookEx<decltype(Function)> *)_Pointer)->Originalfunction;   \
        ((StomphookEx<decltype(Function)> *)_Pointer)->Removehook();                        \
        *Result = _Function(__VA_ARGS__);                                                   \
        ((StomphookEx<decltype(Function)> *)_Pointer)->Reinstall();                         \
    }                                                                                       \
    else                                                                                    \
    {                                                                                       \
        *Result = Function(__VA_ARGS__);                                                    \
    }                                                                                       \

    #define CALLWSHOOKRAW(Function, ...)                                                    \
    auto _Pointer = Winsock::Stomphooks[__FUNCTION__];                                      \
    if(_Pointer)                                                                            \
    {                                                                                       \
        auto _Function = ((StomphookEx<decltype(Function)> *)_Pointer)->Originalfunction;   \
        ((StomphookEx<decltype(Function)> *)_Pointer)->Removehook();                        \
        _Function(__VA_ARGS__);                                                             \
        ((StomphookEx<decltype(Function)> *)_Pointer)->Reinstall();                         \
    }                                                                                       \
    else                                                                                    \
    {                                                                                       \
        Function(__VA_ARGS__);                                                              \
    }                                                                                       \


    int32_t __stdcall Bind(size_t Socket, const sockaddr *Address, int32_t AddressLength)
    {
        IServer *Server;
        if(Address->sa_family == AF_INET6)
            Server = FindByAddress(*(uint32_t *) ((sockaddr_in6 *)Address)->sin6_addr.u.Byte);
        else
            Server = FindByAddress(uint32_t(((sockaddr_in *)Address)->sin_addr.S_un.S_addr));

        if (Server)
        {
            Socketmap[Socket] = Server;
            return 0;
        }

        int32_t Result;
        CALLWSHOOK(bind, &Result, Socket, Address, AddressLength);
        return Result;
    }
    int32_t __stdcall Connect(size_t Socket, const sockaddr *Address, int32_t AddressLength)
    {
        uint16_t Port;
        IServer *Server;        
        char PlainAddress[INET6_ADDRSTRLEN]{};

        // Disconnect an existing server if the socket is in use.
        Server = FindBySocket(Socket);
        if (Server)
        {
            Socketmap.erase(Socket);
            if (Server->GetServerinfo()->Extendedserver)
                ((IServerEx *)Server)->onDisconnect(Socket);
        }

        // Fetch the server from the list.
        if (Address->sa_family == AF_INET6)
        {
            Server = FindByAddress(*(uint32_t *) ((sockaddr_in6 *)Address)->sin6_addr.u.Byte);
            inet_ntop(AF_INET6, &((sockaddr_in6 *)Address)->sin6_addr, PlainAddress, INET6_ADDRSTRLEN);
            Port = ntohs(((sockaddr_in6 *)Address)->sin6_port);

            // Last chance for IP-hostnames.
            if (!Server) Server = FindByName(PlainAddress);
        }
        else
        {
            Server = FindByAddress(uint32_t(((sockaddr_in *)Address)->sin_addr.S_un.S_addr));
            inet_ntop(AF_INET, &((sockaddr_in *)Address)->sin_addr, PlainAddress, INET6_ADDRSTRLEN);
            Port = ntohs(((sockaddr_in *)Address)->sin_port);

            // Last chance for IP-hostnames.
            if (!Server) Server = FindByName(PlainAddress);
        }

        // Debug info.
        NetworkPrint(va("%s to address %s:%u on socket 0x%X", __func__, PlainAddress, Port, Socket));

        // Let winsock handle the request if we can't.
        if (!Server)
        {
            if (Address->sa_family == AF_INET)
                Winsock::ReplaceAddress((sockaddr_in *)Address);

            int32_t Result;
            CALLWSHOOK(connect, &Result, Socket, Address, AddressLength);
            return Result;
        }

        // Add the socket to the map.
        Socketmap[Socket] = Server;
        if (Server->GetServerinfo()->Extendedserver)
            ((IServerEx *)Server)->onConnect(Socket, Port);

        // If it's a real server, we do connect the socket for addressinfo.
        CALLWSHOOKRAW(connect, Socket, Address, AddressLength);
        return 0;
    }
    int32_t __stdcall IOControlSocket(size_t Socket, uint32_t Command, u_long *ArgumentPointer)
    {
        const char *ReadableCommand = "UNKNOWN";

        switch (Command)
        {
            case FIONBIO: 
            {
                ReadableCommand = "FIONBIO";

                // Set the blocking status.
                Blockingmap[Socket] = *ArgumentPointer == 0;
                break;
            }
            case FIONREAD: ReadableCommand = "FIONREAD"; break;
            case FIOASYNC: ReadableCommand = "FIOASYNC"; break;

            case SIOCSHIWAT: ReadableCommand = "SIOCSHIWAT"; break;
            case SIOCGHIWAT: ReadableCommand = "SIOCGHIWAT"; break;
            case SIOCSLOWAT: ReadableCommand = "SIOCSLOWAT"; break;
            case SIOCGLOWAT: ReadableCommand = "SIOCGLOWAT"; break;
            case SIOCATMARK: ReadableCommand = "SIOCATMARK"; break;
        }

        NetworkPrint(va("%s on socket 0x%X with command \"%s\"", __func__, Socket, ReadableCommand));

        int32_t Result;
        CALLWSHOOK(ioctlsocket, &Result, (SOCKET)Socket, Command, ArgumentPointer);
        return Result;
    }
    int32_t __stdcall Receive(size_t Socket, char *Buffer, int32_t BufferLength, int32_t Flags)
    {
        IServer *Server;
        size_t BytesReceived = BufferLength;

        // Find the server if we have one or pass to winsock.
        Server = FindBySocket(Socket);
        if (!Server)
        {
            CALLWSHOOK(recv, &BytesReceived, Socket, Buffer, BufferLength, Flags);
            return BytesReceived;
        }

        // While blocking, wait until we have data to send.
        if (Server->GetServerinfo()->Extendedserver)
        {
            IServerEx *ServerEx = (IServerEx *)Server;

            if (true == Blockingmap[Socket])
            {
                while(false == ServerEx->onReadrequestEx(Socket, Buffer, &BytesReceived))
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
            {
                if (false == ServerEx->onReadrequestEx(Socket, Buffer, &BytesReceived))
                {
                    WSASetLastError(WSAEWOULDBLOCK);
                    return int32_t(-1);
                }
            }            
        }
        else
        {
            if (true == Blockingmap[Socket])
            {
                while(false == Server->onReadrequest(Buffer, &BytesReceived))
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
            {
                if (false == Server->onReadrequest(Buffer, &BytesReceived))
                {
                    WSASetLastError(WSAEWOULDBLOCK);
                    return int32_t(-1);
                }
            } 
        }

        return std::min(BytesReceived, size_t(INT32_MAX));
    }
    int32_t __stdcall ReceiveFrom(size_t Socket, char *Buffer, int32_t BufferLength, int32_t Flags, sockaddr *Peer, int32_t *PeerLength)
    {
        IServer *Server;
        size_t BytesReceived = BufferLength;

        // Find the server if we have one or pass to winsock.
        Server = FindBySocket(Socket);
        if (!Server)
        {
            CALLWSHOOK(recvfrom, &BytesReceived, Socket, Buffer, BufferLength, Flags, Peer, PeerLength);

            if (Peer->sa_family == AF_INET)
                Winsock::ReplaceAddress((sockaddr_in *)Peer);

            return BytesReceived;
        }

        // While blocking, wait until we have data to send.
        if (Server->GetServerinfo()->Extendedserver)
        {
            IServerEx *ServerEx = (IServerEx *)Server;

            if (true == Blockingmap[Socket])
            {
                while(false == ServerEx->onReadrequestEx(Socket, Buffer, &BytesReceived))
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
            {
                if (false == ServerEx->onReadrequestEx(Socket, Buffer, &BytesReceived))
                {
                    WSASetLastError(WSAEWOULDBLOCK);
                    return int32_t(-1);
                }
            }            
        }
        else
        {
            if (true == Blockingmap[Socket])
            {
                while(false == Server->onReadrequest(Buffer, &BytesReceived))
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            else
            {
                if (false == Server->onReadrequest(Buffer, &BytesReceived))
                {
                    WSASetLastError(WSAEWOULDBLOCK);
                    return int32_t(-1);
                }
            } 
        }

        // Set the host information from previous call.
        if(16 >= sizeof(sockaddr))
            std::memcpy(Peer, Server->GetServerinfo()->Hostinfo, sizeof(sockaddr));

        return std::min(BytesReceived, size_t(INT32_MAX));
    }
    int32_t __stdcall Select(int32_t fdsCount, fd_set *Readfds, fd_set *Writefds, fd_set *Exceptfds, const timeval *Timeout)
    {
        int32_t SocketCount;

        // Request socket info from winsock.
        CALLWSHOOK(select, &SocketCount, fdsCount, Readfds, Writefds, Exceptfds, Timeout);
        if (SocketCount == -1)
            SocketCount = 0;

        // Add our connected sockets.
        for (auto Iterator = Socketmap.begin(); Iterator != Socketmap.end(); ++Iterator)
        {
            if (Readfds) FD_SET(Iterator->first, Readfds);
            if (Writefds) FD_SET(Iterator->first, Writefds);
            if (Readfds) SocketCount++;
            if (Writefds) SocketCount++;
        }

        // Return the total number of available socket operations.
        return SocketCount;
    }
    int32_t __stdcall Send(size_t Socket, const char *Buffer, int32_t BufferLength, int32_t Flags)
    {
        IServer *Server;

        // Find the server if we have one or pass to winsock.
        Server = FindBySocket(Socket);
        if (!Server)
        {
            int32_t Result;
            CALLWSHOOK(send, &Result, Socket, Buffer, BufferLength, Flags);
            return Result;
        }

        // Send the data to a server, this should always be handled.
        if (Server->GetServerinfo()->Extendedserver)
        {
            IServerEx *Extended = (IServerEx *)Server;
            if (false == Extended->onWriterequestEx(Socket, Buffer, size_t(BufferLength)))
                return int32_t(-1);
        }
        else
        {
            if (false == Server->onWriterequest(Buffer, size_t(BufferLength)))
                return int32_t(-1);
        }

        return BufferLength;
    }
    int32_t __stdcall SendTo(size_t Socket, const char *Buffer, int32_t BufferLength, int32_t Flags, const sockaddr *Peer, int32_t PeerLength)
    {
        IServer *Server;

        // Find the server if we have one or pass to winsock.
        Server = FindBySocket(Socket);
        if (!Server)
        {
            if(Peer->sa_family == AF_INET6)
                Server = FindByAddress(*(uint32_t *) ((sockaddr_in6 *)Peer)->sin6_addr.u.Byte);
            else
                Server = FindByAddress(uint32_t(((sockaddr_in *)Peer)->sin_addr.S_un.S_addr));
        }
        if (!Server)
        {
            if (Peer->sa_family == AF_INET)
                Winsock::ReplaceAddress((sockaddr_in *)Peer);

            int32_t Result;            
            CALLWSHOOK(sendto, &Result, Socket, Buffer, BufferLength, Flags, Peer, PeerLength);
            return Result;
        }            

        // Add the socket to the map and save the host info.
        Socketmap[Socket] = Server;
        if(16 >= sizeof(sockaddr))
            std::memcpy(Server->GetServerinfo()->Hostinfo, Peer, sizeof(sockaddr));

        // Send the data to a server, this should always be handled.
        if (Server->GetServerinfo()->Extendedserver)
        {
            IServerEx *Extended = (IServerEx *)Server;
            if (false == Extended->onWriterequestEx(Socket, Buffer, size_t(BufferLength)))
                return int32_t(-1);
        }
        else
        {
            if (false == Server->onWriterequest(Buffer, size_t(BufferLength)))
                return int32_t(-1);
        }

        return BufferLength;
    }
    hostent *__stdcall GetHostByName(const char *Hostname)
    {
        IServer *Server;

        // Find the server if we have one.
        Server = FindByName(Hostname);        
        if (!Server) Server = FindByAddress(uint32_t(inet_addr(Hostname)));
        if (!Server)
        {
            static hostent *ResolvedHost;
            CALLWSHOOK(gethostbyname, &ResolvedHost, Hostname);

            // Debug information about winsocks result.
            if (ResolvedHost != nullptr)
                NetworkPrint(va("%s: \"%s\" -> %s", __func__, Hostname, inet_ntoa(*(in_addr*)ResolvedHost->h_addr_list[0])));

            return ResolvedHost;
        }

        // Create the address struct.
        in_addr *LocalAddress = new in_addr();
        in_addr *LocalSocketAddrList[2];
        LocalAddress->S_un.S_addr = Server->GetServerinfo()->Hostaddress;
        LocalSocketAddrList[0] = LocalAddress;
        LocalSocketAddrList[1] = nullptr;

        hostent *LocalHost = new hostent();
        LocalHost->h_aliases = NULL;
        LocalHost->h_addrtype = AF_INET;
        LocalHost->h_length = sizeof(in_addr);
        LocalHost->h_name = const_cast<char *>(Hostname);        
        LocalHost->h_addr_list = (char **)LocalSocketAddrList;

        NetworkPrint(va("%s: \"%s\" -> %s", __func__, Hostname, inet_ntoa(*LocalAddress)));
        return LocalHost;
    }
    int32_t __stdcall GetAddressinfo(const char *Nodename, const char *Servicename, const ADDRINFOA *Hints, ADDRINFOA **Result)
    {
        IServer *Server;

        // Resolve the host using the windows function.
        int32_t Callresult;
        CALLWSHOOK(getaddrinfo, &Callresult, Nodename, Servicename, Hints, Result);
        if (0 == Callresult)
        {
            // Replace the address with ours if needed.
            Server = FindByName(Nodename);
            if (!Server) Server = FindByAddress(uint32_t(inet_addr(Nodename)));
            if (Server)
            {
                for (ADDRINFOA *ptr = *Result; ptr != NULL; ptr = ptr->ai_next)
                {
                    // We only handle IPv4 for now.
                    if (ptr->ai_family == AF_INET)
                    {
                        ((sockaddr_in *)ptr->ai_addr)->sin_addr.S_un.S_addr = Server->GetServerinfo()->Hostaddress;
                        NetworkPrint(va("%s: \"%s\" -> %s", __func__, Nodename, inet_ntoa(((sockaddr_in *)ptr->ai_addr)->sin_addr)));                        
                    }
                }
            }
            else
            {
                NetworkPrint(va("%s: \"%s\" have no local handler and will not be handled by AyriaNetworking.", __func__, Nodename));
            }
            
            return 0;
        }

        // If resolving fails, we just have to make our own.
        Server = FindByName(Nodename);
        if (!Server) Server = FindByAddress(uint32_t(inet_addr(Nodename)));
        if (Server)
        {
            // Resolve a known address to allocate it properly and then change it.
            CALLWSHOOKRAW(getaddrinfo, va("127.0.0.1"), Servicename, Hints, Result);
            ((sockaddr_in *)(*Result)->ai_addr)->sin_addr.S_un.S_addr = Server->GetServerinfo()->Hostaddress;
            NetworkPrint(va("%s: \"%s\" -> %s", __func__, Nodename, inet_ntoa(((sockaddr_in *)(*Result)->ai_addr)->sin_addr)));

            return 0;
        }

        WSASetLastError(WSAHOST_NOT_FOUND);
        return -1;
    }
    int32_t __stdcall GetPeername(size_t Socket, sockaddr *Name, int32_t *Namelength)
    {
        IServer *Server;
        sockaddr_in *Localname = (sockaddr_in *)Name;

        // If it's our socket, return IPv4.
        Server = FindBySocket(Socket);
        if (Server)
        {
            Localname->sin_family = AF_INET;
            Localname->sin_port = 0;
            Localname->sin_addr.S_un.S_addr = Server->GetServerinfo()->Hostaddress;
            *Namelength = sizeof(sockaddr_in);
            return 0;
        }
        
        int32_t Result;
        CALLWSHOOK(getpeername, &Result, Socket, Name, Namelength);
        return Result;
    }
    int32_t __stdcall GetSockname(size_t Socket, sockaddr *Name, int32_t *Namelength)
    {
        IServer *Server;
        sockaddr_in *Localname = (sockaddr_in *)Name;

        // If it's our socket, return IPv4.
        Server = FindBySocket(Socket);
        if (Server)
        {
            Localname->sin_family = AF_INET;
            Localname->sin_port = 0;
            Localname->sin_addr.S_un.S_addr = Server->GetServerinfo()->Hostaddress;
            *Namelength = sizeof(sockaddr_in);
            return 0;
        }

        int32_t Result;
        CALLWSHOOK(getsockname, &Result, Socket, Name, Namelength);
        return Result;
    }

    int32_t __stdcall Shutdown(size_t Socket, size_t How)
    {
        IServer *Server;        

        // Disconnect the server.
        Server = FindBySocket(Socket);
        if (Server)
        {
            Socketmap.erase(Socket);
            if (Server->GetServerinfo()->Extendedserver)
                ((IServerEx *)Server)->onDisconnect(Socket);
        }

        // Disconnect the actual socket.
        CALLWSHOOKRAW(shutdown, Socket, How);
        return 0;
    }
    int32_t __stdcall Closesocket(size_t Socket)
    {
        IServer *Server;        

        // Disconnect the server.
        Server = FindBySocket(Socket);
        if (Server)
        {
            Socketmap.erase(Socket);
            if (Server->GetServerinfo()->Extendedserver)
                ((IServerEx *)Server)->onDisconnect(Socket);
        }

        // Disconnect the actual socket.
        CALLWSHOOKRAW(closesocket, Socket);
        return 0;
    }
}
#endif // _WIN32

namespace Winsock
{
    void Initializehandler()
    {
        size_t Address;

        #define PATCH_WINSOCK_IAT(Export, Function)                                                             \
        Address = GetIATFunction("wsock32.dll", Export);                                                        \
        if(Address) *(size_t *)Address = size_t(Function);                                                      \
        else Address = GetIATFunction("WS2_32.dll", Export);                                                    \
        if(Address) *(size_t *)Address = size_t(Function);                                                      \

        #define PATCH_WINSOCK_STOMP(Export, Function)                                                           \
        Address = (size_t)GetProcAddress(GetModuleHandleA("wsock32.dll"), Export);                              \
        if (!Address) Address = (size_t)GetProcAddress(GetModuleHandleA("WS2_32.dll"), Export);                 \
        if (Address)                                                                                            \
        {                                                                                                       \
            Stomphooks[#Function] = new StomphookEx<decltype(Function)>();                                      \
            ((StomphookEx<decltype(Function)> *)Stomphooks[#Function])->Setfunctionaddress((void *)Address);    \
            ((StomphookEx<decltype(Function)> *)Stomphooks[#Function])->Installhook((void *)Address, Function); \
        }                                                                                                       \

        // While stomp-hooking isn't necessarily exclusive with IAT hooking
        // there's a performance impact for no benefit whatsoever.
        if (std::strstr(GetCommandLineA(), "-ws_stomphook"))
        {
            PATCH_WINSOCK_STOMP("bind", WSReplacement::Bind);
            PATCH_WINSOCK_STOMP("send", WSReplacement::Send);
            PATCH_WINSOCK_STOMP("recv", WSReplacement::Receive);
            PATCH_WINSOCK_STOMP("sendto", WSReplacement::SendTo);
            PATCH_WINSOCK_STOMP("select", WSReplacement::Select);
            PATCH_WINSOCK_STOMP("connect", WSReplacement::Connect);
            PATCH_WINSOCK_STOMP("shutdown", WSReplacement::Shutdown);
            PATCH_WINSOCK_STOMP("recvfrom", WSReplacement::ReceiveFrom);
            PATCH_WINSOCK_STOMP("getpeername", WSReplacement::GetPeername);
            PATCH_WINSOCK_STOMP("getsockname", WSReplacement::GetSockname);
            PATCH_WINSOCK_STOMP("closesocket", WSReplacement::Closesocket);
            PATCH_WINSOCK_STOMP("getaddrinfo", WSReplacement::GetAddressinfo);
            PATCH_WINSOCK_STOMP("gethostbyname", WSReplacement::GetHostByName);
            PATCH_WINSOCK_STOMP("ioctlsocket", WSReplacement::IOControlSocket);
        }
        else
        {
            PATCH_WINSOCK_IAT("bind", WSReplacement::Bind);
            PATCH_WINSOCK_IAT("send", WSReplacement::Send);
            PATCH_WINSOCK_IAT("recv", WSReplacement::Receive);
            PATCH_WINSOCK_IAT("sendto", WSReplacement::SendTo);
            PATCH_WINSOCK_IAT("select", WSReplacement::Select);
            PATCH_WINSOCK_IAT("connect", WSReplacement::Connect);
            PATCH_WINSOCK_IAT("shutdown", WSReplacement::Shutdown);
            PATCH_WINSOCK_IAT("recvfrom", WSReplacement::ReceiveFrom);
            PATCH_WINSOCK_IAT("getpeername", WSReplacement::GetPeername);
            PATCH_WINSOCK_IAT("getsockname", WSReplacement::GetSockname);
            PATCH_WINSOCK_IAT("closesocket", WSReplacement::Closesocket);
            PATCH_WINSOCK_IAT("getaddrinfo", WSReplacement::GetAddressinfo);
            PATCH_WINSOCK_IAT("gethostbyname", WSReplacement::GetHostByName);
            PATCH_WINSOCK_IAT("ioctlsocket", WSReplacement::IOControlSocket);
        }
    }
    void Registerserver(IServer *Server)
    {
        Servermap[Server->GetServerinfo()->Hostaddress] = Server;
    }
}
