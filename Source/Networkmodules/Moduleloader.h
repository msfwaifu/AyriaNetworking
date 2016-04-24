/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-4-24
    Notes:
        1. Enumerate module files.
        2. Load them into process memory.
        3. Call GetServerinstance for each.
*/

#pragma once
#include <Configuration\All.h>
#include <Servers\IServer.h>

// Module data-representation.
struct INetworkmodule
{
    char Modulename[65]{};
    std::vector<IServer *> Instances;
    IServer *(__cdecl *GetServerinstance)(const char *Hostname);
};

// Load and unload the extensions.
namespace Networkmodules
{
    void LoadAll();
    void UnloadAll();
}
