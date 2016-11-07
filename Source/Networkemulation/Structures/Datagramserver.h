/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-11-7
    Notes:
        This server provides a base for UDP-like 'connections'.
*/

#pragma once
#include "IServer.h"
#include <algorithm>
#include <vector>
#include <mutex>

// The serverversions defined in this module.
#define ISERVER_DATAGRAM    32

struct Datagramserver : public IServer
{
    std::vector<std::vector<uint8_t>> Outgoingpackets;
    std::mutex Bufferguard;

    // Return the servers version.
    virtual uint32_t Version() { return ISERVER_BASE | ISERVER_DATAGRAM; };

    // Returns false if there's no data or something went wrong.
    virtual bool onReadrequest(void *Databuffer, uint32_t *Datasize)
    {
        if (0 == Outgoingpackets.size())
            return false;

        // Datagram servers can't handle partial requests, so we drop the rest.
        Bufferguard.lock();
        {
            uint32_t Readcount = std::min(*Datasize, uint32_t(Outgoingpackets.size()));

            std::copy_n(Outgoingpackets.begin()->begin(), Readcount, reinterpret_cast<uint8_t *>(Databuffer));
            Outgoingpackets.erase(Outgoingpackets.begin());

            *Datasize = Readcount;
        }
        Bufferguard.unlock();

        return true;
    }
    virtual bool onWriterequest(const void *Databuffer, const uint32_t Datasize)
    {
        // Datagram servers just handle the data directly.
        Bufferguard.lock();
        {
            auto Pointer = reinterpret_cast<const uint8_t *>(Databuffer);
            onPacket({ Pointer, Pointer + Datasize });
        }
        Bufferguard.try_lock();
        Bufferguard.unlock();

        return true;
    }

    // Allows access from usercode.
    virtual void Send(std::string &Databuffer)
    {
        return Send(Databuffer.data(), uint32_t(Databuffer.size()));
    }
    virtual void Send(const void *Databuffer, const uint32_t Datasize)
    {
        Bufferguard.lock();
        {
            auto Pointer = reinterpret_cast<const uint8_t *>(Databuffer);
            Outgoingpackets.push_back({ Pointer, Pointer + Datasize });
        }
        Bufferguard.unlock();
    }
    virtual void onPacket(std::vector<uint8_t> Packet) = 0;
};
