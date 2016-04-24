/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-4-24
    Notes:
        Replaces the WS2_32 and wsock32 API exports.
*/

#pragma once
#include <Configuration\All.h>
#include <Servers\IServer.h>

namespace Winsock
{
    void Initializehandler();
    void Registerserver(IServer *Server);
}
