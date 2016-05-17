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
std::unordered_map<uint32_t /* Address */, IServer *> Servermap;
std::unordered_map<size_t /* Socket */, IServer *> Socketmap;
std::unordered_map<size_t /* Socket */, bool> Blockingmap;

// Simplified lookup methods.
IServer *FindByAddress(const uint32_t Address)
{
    for(auto Iterator = Servermap.begin(); Iterator != Servermap.end(); ++Iterator)
        if(uint32_t(Iterator->first) == Address)
            return Iterator->second;

    return nullptr;
}
IServer *FindBySocket(const size_t Socket)
{
    auto Result = Socketmap.find(Socket);
    if(Result != Socketmap.end())
        return Result->second;

    return nullptr;
}
IServer *FindByName(std::string Hostname)
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

namespace WSReplacement
{
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

        return bind(Socket, Address, AddressLength);
    }
    int32_t __stdcall CloseSocket(size_t Socket)
    {
        IServer *Server;

        // Find the server if we have one.
        Server = FindBySocket(Socket);
        if (Server)
        {
            NetworkPrint(va("%s for server \"%s\"", __func__, Server->GetServerinfo()->Hostname));
            Socketmap.erase(Socket);

            if (Server->GetServerinfo()->Extendedserver)
                ((IServerEx *)Server)->onDisconnect(Socket);
        }

        return closesocket(Socket);
    }
    int32_t __stdcall Connect(size_t Socket, const sockaddr *Address, int32_t AddressLength)
    {
        uint16_t Port;
        IServer *Server;        
        char PlainAddress[INET6_ADDRSTRLEN]{};

        // Disconnect the server.
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
            return connect(Socket, Address, AddressLength);
        }

        // Add the socket to the map.
        Socketmap[Socket] = Server;
        if (Server->GetServerinfo()->Extendedserver)
            ((IServerEx *)Server)->onConnect(Socket, Port);

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
        return ioctlsocket((SOCKET)Socket, Command, ArgumentPointer);
    }
    int32_t __stdcall Receive(size_t Socket, char *Buffer, int32_t BufferLength, int32_t Flags)
    {
        IServer *Server;
        size_t BytesReceived = BufferLength;

        // Find the server if we have one or pass to winsock.
        Server = FindBySocket(Socket);
        if (!Server) 
            return recv(Socket, Buffer, BufferLength, Flags);

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
            return recvfrom(Socket, Buffer, BufferLength, Flags, Peer, PeerLength);

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
        SocketCount = select(fdsCount, Readfds, Writefds, Exceptfds, Timeout);
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
            return send(Socket, Buffer, BufferLength, Flags);

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
            return sendto(Socket, Buffer, BufferLength, Flags, Peer, PeerLength);

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
            static hostent *ResolvedHost = nullptr;
            ResolvedHost = gethostbyname(Hostname);

            // Debug information about winsocks result.
            if (ResolvedHost != nullptr)
                NetworkPrint(va("%s: \"%s\" -> %s", __func__, Hostname, inet_ntoa(*(in_addr*)ResolvedHost->h_addr_list[0])));

            return ResolvedHost;
        }

        // Create the local address.
        static in_addr LocalAddress;
        static in_addr *LocalSocketAddrList[2];
        LocalAddress.S_un.S_addr = Server->GetServerinfo()->Hostaddress;
        LocalSocketAddrList[0] = &LocalAddress;
        LocalSocketAddrList[1] = nullptr;

        hostent *LocalHost = new hostent();
        LocalHost->h_aliases = NULL;
        LocalHost->h_addrtype = AF_INET;
        LocalHost->h_length = sizeof(in_addr);
        LocalHost->h_name = const_cast<char *>(Hostname);        
        LocalHost->h_addr_list = (char **)LocalSocketAddrList;

        NetworkPrint(va("%s: \"%s\" -> %s", __func__, Hostname, inet_ntoa(LocalAddress)));
        return LocalHost;
    }
    int32_t __stdcall GetAddressinfo(const char *Nodename, const char *Servicename, const ADDRINFOA *Hints, ADDRINFOA **Result)
    {
        IServer *Server;

        // Resolve the host using the windows function.
        if (0 == getaddrinfo(Nodename, Servicename, Hints, Result))
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
                        NetworkPrint(va("%s: \"%s\" -> %s", __func__, Nodename, inet_ntoa(((sockaddr_in *)ptr->ai_addr)->sin_addr)));
                        ((sockaddr_in *)ptr->ai_addr)->sin_addr.S_un.S_addr = Server->GetServerinfo()->Hostaddress;
                    }
                }
            }

            return 0;
        }

        // If resolving fails, we just have to make our own.
        Server = FindByName(Nodename);
        if (!Server) Server = FindByAddress(uint32_t(inet_addr(Nodename)));
        if (Server)
        {
            *Result = new ADDRINFOA();

            (*Result)->ai_family = AF_INET;
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
        
        return getpeername(Socket, Name, Namelength);
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

        return getsockname(Socket, Name, Namelength);
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
        shutdown(Socket, How);
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
        closesocket(Socket);
        return 0;
    }
}
#endif // _WIN32

namespace Winsock
{
    void Initializehandler()
    {
        #define PATCH_WINSOCK_IAT(Export, Function)                 \
        Address = GetIATFunction("wsock32.dll", Export);            \
        if(Address) *(size_t *)Address = size_t(Function);          \
        else Address = GetIATFunction("WS2_32.dll", Export);        \
        if(Address) *(size_t *)Address = size_t(Function);          \

        size_t Address;
        PATCH_WINSOCK_IAT("bind", WSReplacement::Bind);
        PATCH_WINSOCK_IAT("closesocket", WSReplacement::CloseSocket);
        PATCH_WINSOCK_IAT("connect", WSReplacement::Connect);
        PATCH_WINSOCK_IAT("ioctlsocket", WSReplacement::IOControlSocket);
        PATCH_WINSOCK_IAT("recv", WSReplacement::Receive);
        PATCH_WINSOCK_IAT("recvfrom", WSReplacement::ReceiveFrom);
        PATCH_WINSOCK_IAT("select", WSReplacement::Select);
        PATCH_WINSOCK_IAT("send", WSReplacement::Send);
        PATCH_WINSOCK_IAT("sendto", WSReplacement::SendTo);
        PATCH_WINSOCK_IAT("gethostbyname", WSReplacement::GetHostByName);
        PATCH_WINSOCK_IAT("getaddrinfo", WSReplacement::GetAddressinfo);
        PATCH_WINSOCK_IAT("getpeername", WSReplacement::GetPeername);  
        PATCH_WINSOCK_IAT("getsockname", WSReplacement::GetSockname);  

        PATCH_WINSOCK_IAT("shutdown", WSReplacement::Shutdown);  
        PATCH_WINSOCK_IAT("closesocket", WSReplacement::Closesocket);  
    }
    void Registerserver(IServer *Server)
    {
        Servermap[Server->GetServerinfo()->Hostaddress] = Server;
    }
}
