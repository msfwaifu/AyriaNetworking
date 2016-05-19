/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-5-19
    Notes:
        Replaces the WININET API exports.
*/

#pragma once
#include <Configuration\All.h>
#include <Servers\IServer.h>

namespace Wininternet
{
    void Initializehandler();
    void Registerserver(IServer *Server);
}
