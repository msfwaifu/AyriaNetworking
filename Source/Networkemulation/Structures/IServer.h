/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-11-7
    Notes:
        The IServer(ex) class provides a base for emulated servers.
        Overriding classes must not block in read/write.
*/

#pragma once
#include <cstdint>
#include <STDInclude.h>

// The serverversions defined in this module.
#define VERSION_ISERVER             0
#define VERSION_ISERVEREX           1
#define VERSION_ISERVER_RESERVED1   2
#define VERSION_ISERVER_RESERVED2   4
#define VERSION_ISERVER_RESERVED3   8

// The base server for single-socket connections.
struct IServer
{
    // Return the servers version.
    virtual uint32_t Version() { return VERSION_ISERVER; };

    // Returns false if there's no data or something went wrong.
    virtual bool onReadrequest(void *Databuffer, uint32_t *Datasize) = 0;
    virtual bool onWriterequest(const void *Databuffer, const uint32_t Datasize) = 0;
};

// The base server for multi-socket connections.
struct IServerEx : public IServer
{
    // Return the servers version.
    virtual uint32_t Version() { return VERSION_ISERVEREX; };

    // Nullsub the base methods.
    virtual bool onReadrequest(void *Databuffer, uint32_t *Datasize) { (void)Databuffer; (void)Datasize; return false; };
    virtual bool onWriterequest(const void *Databuffer, const uint32_t Datasize) { (void)Databuffer; (void)Datasize; return false; };

    // Socket state management.
    virtual void onDisconnect(const size_t Socket) = 0;
    virtual void onConnect(const size_t Socket, const uint16_t Port) = 0;

    // Returns false if there's no data or something went wrong.
    virtual bool onReadrequestEx(const size_t Socket, void *Databuffer, uint32_t *Datasize) = 0;
    virtual bool onWriterequestEx(const size_t Socket, const void *Databuffer, const uint32_t Datasize) = 0;
};
