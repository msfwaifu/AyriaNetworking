/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-11-7
    Notes:
        This module will replace WS2_32.dll with internal networking.
        If there's no module to handle it internally, WS will handle it.
*/

#include <STDInclude.h>
#include <unordered_map>

// Windows specific.
#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

// Replacement functions.
namespace Winsock
{
#ifdef _WIN32


#endif
}

// Platform layer.
std::unordered_map<uint64_t /* Hash */, void * /* Hook */> Platform::Hooks;
void Platform::Initialize_Winsock()
{
#ifdef _WIN32



#endif
}
