/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-4-24
    Notes:
        The basic interface for exporting servers from modules.
        The system should be implemented as nonblocking.
*/

#pragma once
#include <Configuration\All.h>

// The simple interface, implement everything yourself.
struct IServer
{
    // Returns false if the request could not be completed.
    virtual bool onReadrequest(char *Databuffer, size_t *Datalength) = 0;
    virtual bool onWriterequest(const char *Databuffer, const size_t Datalength) = 0;

    // Server information in an easy to expand union.
    union
    {
        struct
        {
            char Hostname[64];
            uint8_t Hostinfo[16];
            uint64_t Hostaddress;
            uint32_t Extendedserver;
        } Segmented;
        char Raw[128];
    } Serverinfo;

    // Construct the server from a hostname.
    IServer()
    {
        // Clear the serverinfo and set the correct interface.
        std::memset(Serverinfo.Raw, 0, sizeof(Serverinfo));
        Serverinfo.Segmented.Extendedserver = 0;
    }
    IServer(const char *Hostname)
    {
        // Clear the serverinfo and set the correct interface.
        std::memset(Serverinfo.Raw, 0, sizeof(Serverinfo));
        Serverinfo.Segmented.Extendedserver = 0;

        // Copy the host information and address.
        std::strncpy(Serverinfo.Segmented.Hostname, Hostname, 63);
        Serverinfo.Segmented.Hostaddress = FNV1a_Runtime_64(Hostname, std::strlen(Hostname));
    }
};

// The extended interface, when you need even more customization.
struct IServerEx : public IServer
{
    // Per socket operations.
    virtual void onDisconnect(const size_t Socket) = 0;
    virtual void onConnect(const size_t Socket, const uint16_t Port) = 0;
    virtual bool onReadrequestEx(const size_t Socket, char *Databuffer, size_t *Datalength) = 0;
    virtual bool onWriterequestEx(const size_t Socket, const char *Databuffer, const size_t Datalength) = 0;
    virtual bool onSocketstatus(int32_t *Readcount, size_t *Readsockets, int32_t *Writecount, size_t *Writesockets) = 0;

    // Construct the server from a hostname.
    IServerEx() : IServer()
    {
        // Set the info to extended mode.
        Serverinfo.Segmented.Extendedserver = 1;
    }
    IServerEx(const char *Hostname) : IServer(Hostname)
    {
        // Set the info to extended mode.
        Serverinfo.Segmented.Extendedserver = 1;
    }
};
