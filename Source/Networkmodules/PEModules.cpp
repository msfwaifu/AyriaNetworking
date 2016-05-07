/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-4-24
    Notes:
        1. Enumerate module files.
        2. Load them into process memory.
        3. Call GetServerinstance for each.
*/

#include <Configuration\All.h>
#include <Networkhandlers\Winsock.h>
#include <Networkmodules\Moduleloader.h>

// PE only.
#ifdef _WIN32
#include <Windows.h>

namespace Networkmodules
{
    std::vector<INetworkmodule> Modulelist;

    void LoadAll()
    {
        // Before loading, set the DllDirectory to resolve any dependencies.
        SetDllDirectoryA("./Plugins/Networkingmodules/");
        AddDllDirectory(L"./Plugins/Networkingmodules/");

        std::vector<std::string> Modulefiles;
        Filesystem::Searchdir("./Plugins/Networkingmodules/", &Modulefiles, "dll");
        for each (std::string Module in Modulefiles)
        {
            // Load each module into process memory.
            HMODULE Library = LoadLibraryA(va_small("./Plugins/Networkingmodules/%s", Module.c_str()));
            if (Library)
            {
                INetworkmodule Newmodule;

                std::strncpy(Newmodule.Modulename, Module.c_str(), 64);
                Newmodule.GetServerinstance = (IServer *(__cdecl *)(const char *))GetProcAddress(Library, "GetServerinstance");
                if (Newmodule.GetServerinstance)
                    Modulelist.push_back(Newmodule);
            }
        }

        // Read the configuration files.
        if (true == CSVManager::Readfile("./Plugins/Networkingmodules/Configuration.csv"))
        {
            for (size_t i = 0; ; ++i)
            {
                // End of file check.
                if (0 == CSVManager::Getvalue(i, 0).size())
                    break;

                // Find the correct module for the entry.
                for(auto Iterator = Modulelist.begin(); Iterator != Modulelist.end(); ++Iterator)
                {
                    if (std::strstr(Iterator->Modulename, CSVManager::Getvalue(i, 0).c_str()))
                    {
                        IServer *Instance = Iterator->GetServerinstance(CSVManager::Getvalue(i, 1).c_str());
                        if (Instance)
                            Iterator->Instances.push_back(Instance);
                        else
                            DebugPrint(va("Extension missing for handler \"%s\" with hostname \"%s\"", CSVManager::Getvalue(i, 0).c_str(), CSVManager::Getvalue(i, 1).c_str()));
                        break;
                    }
                }
            }
        }
        else
        {
            DebugPrint("Failed to load \"./Plugins/Networkingmodules/Configuration.csv\", verify that you have one.");
            return;
        }

        // Add all servers to the Networkhandlers.
        for each (INetworkmodule Module in Modulelist)
        {
            for each (IServer *Instance in Module.Instances)
            {
                Winsock::Registerserver(Instance);
                // TODO(Convery): Add more handlers here.
            }
        }

        // Ensure that the extensions are properly unloaded.
        std::atexit(UnloadAll);
    }
    void UnloadAll()
    {
        for each (INetworkmodule Module in Modulelist)
        {
            for each (IServerEx *Server in Module.Instances)
            {
                if (Server->GetServerinfo()->Extendedserver)
                    Server->onDisconnect(size_t(0));
            }

            FreeLibrary(GetModuleHandleA(Module.Modulename));
        }
    }
}
#endif
