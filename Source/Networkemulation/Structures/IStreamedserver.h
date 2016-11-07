/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-11-7
    Notes:
        This server provides a base for TCP-like connections.
*/

#pragma once
#include "IServer.h"
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <mutex>

// The serverversions defined in this module.
#define ISERVER_STREAMED    16

struct IStreamedserver : public IServerEx
{
    // The datastreams this server handles, per socket.
    std::unordered_map<size_t, std::vector<uint8_t>> Incomingstream;
    std::unordered_map<size_t, std::vector<uint8_t>> Outgoingstream;
    std::unordered_map<size_t, std::mutex> Streamguard;
    std::unordered_map<size_t, bool> Connectedstreams;

    // Return the servers version.
    virtual uint32_t Version() { return ISERVER_EXTENDED | ISERVER_STREAMED; };

    // Socket state management.
    virtual void onDisconnect(const size_t Socket)
    {
        Streamguard[Socket].lock();
        {
            Incomingstream[Socket].clear();
            Connectedstreams[Socket] = false;
        }
        Streamguard[Socket].unlock();
    }
    virtual void onConnect(const size_t Socket, const uint16_t Port)
    {
        Streamguard[Socket].lock();
        {
            Outgoingstream[Socket].clear();
            Incomingstream[Socket].clear();
            Connectedstreams[Socket] = true;
        }
        Streamguard[Socket].unlock();
    }

    // Returns false if there's no data or something went wrong.
    virtual bool onReadrequestEx(const size_t Socket, void *Databuffer, uint32_t *Datasize)
    {
        /*
            Streamed servers need support for lingering connections.
            So even if the socket is inactive, we send data if requested.
        */
        if (false == Connectedstreams[Socket] && 0 == Outgoingstream[Socket].size())
            return false;

        // Streamed servers can handle partial requests, so we copy what we can.
        Streamguard[Socket].lock();
        {
            uint32_t Readcount = std::min(*Datasize, uint32_t(Outgoingstream[Socket].size()));

            std::copy_n(Outgoingstream[Socket].begin(), Readcount, reinterpret_cast<uint8_t *>(Databuffer));
            Outgoingstream[Socket].erase(Outgoingstream[Socket].begin(), Outgoingstream[Socket].begin() + Readcount);

            *Datasize = Readcount;
        }
        Streamguard[Socket].unlock();
        
        return true;
    }
    virtual bool onWriterequestEx(const size_t Socket, const void *Databuffer, const uint32_t Datasize)
    {
        // If we aren't connected, we do nothing.
        if (false == Connectedstreams[Socket])
            return false;

        // Streamed servers just append the partial messages to the queue.
        Streamguard[Socket].lock();
        {
            auto Pointer = reinterpret_cast<const uint8_t *>(Databuffer);
            std::copy_n(Pointer, Datasize, Incomingstream[Socket].end());           

            // Notify the user whom needs to unlock the mutex.
            std::thread(onStreamupdatewrapper, this, Socket).detach();
        }
        // The wrapper unlocks the mutex if needed after the call.

        return true;
    }

    // Allows access from usercode.
    virtual void Send(const size_t Socket, std::string &Databuffer)
    {
        return Send(Socket, Databuffer.data(), uint32_t(Databuffer.size()));
    }
    virtual void Send(const size_t Socket, const void *Databuffer, const uint32_t Datasize)
    {
        /*
            If the socket is not defined, we treat it as a broadcast.
            This is less of a feature and more of a fallback if the user
            forgot which socket they were using in a few-sockets server.
        */

        auto Lambda = [&](const size_t lSocket) -> void
        {
            Streamguard[lSocket].lock();
            {
                auto Pointer = reinterpret_cast<const uint8_t *>(Databuffer);
                std::copy_n(Pointer, Datasize, Incomingstream[lSocket].end());            
            }
            Streamguard[lSocket].unlock();
        };

        // Return if the socket is defined.
        if (0 != Socket) return Lambda(Socket);
        
        // Broadcast to all connected sockets.
        for (auto &Item : Connectedstreams)
        {
            if (true == Item.second)
            {
                return Lambda(Item.first);
            }
        }
    }
    virtual void onStreamupdate(const size_t Socket, std::vector<uint8_t> &Stream) = 0;
    static void onStreamupdatewrapper(IStreamedserver *This, const size_t Socket)
    {
        /*
            The user needs to unlock the mutex once they have removed
            the data from the incomingstream. But we don't trust the 
            user so we need to check the mutex after the call.
        */

        // Notify the usercode about the update.
        This->onStreamupdate(Socket, This->Incomingstream[Socket]);

        // Unlock the mutex if needed.
        This->Streamguard[Socket].try_lock();
        This->Streamguard[Socket].unlock();
    }
};
