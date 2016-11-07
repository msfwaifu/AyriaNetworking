/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-10-21
    Notes:
        Entrypoint for the plugin, exports per the 2016 standard.
*/

#include "STDInclude.h"
#include <cstdarg>

extern "C"
{
    EXPORT_ATTR void __cdecl onExtensionUnloading(void)
    {
        /*
            This export is called for each plugin when the game exists.
            It should be used to save your configurations and similar info.
        */
    }
    EXPORT_ATTR void __cdecl onInitializationStart(void)
    {
        /*
            This export is called when all other dlls and plugins have been loaded.
            It should be used to do your initialization and .text segment modifications.
        */
    }
    EXPORT_ATTR void __cdecl onInitializationComplete(void)
    {
        /*
            This export is called either 3 seconds after the start or when
            another plugin such as AyriaPlatform notifies the bootstrap loader.
            It should be used to do your memory modifications.
        */
    }
    EXPORT_ATTR void __cdecl onMessage(uint32_t Message, ...)
    {
        /*
            This export is called when a message is broadcasted to all plugins.
            As such you'll want to have a unique name for the messages your
            plugin listens for. Message IDs are FNV1a 32-bit hashes of the name.
        */

        std::va_list Variadic;
        va_start(Variadic, Message);

        switch (Message)
        {
            case AYRIA::FNV1::Compiletime::FNV1a_32("AyriaNetworking_DefaultCase"):
            default: break;
        }

        va_end(Variadic);
    }
}

#ifdef _WIN32
#include <Windows.h>
BOOLEAN WINAPI DllMain(HINSTANCE hDllHandle, DWORD nReason, LPVOID Reserved)
{
    switch ( nReason )
    {
    case DLL_PROCESS_ATTACH:
        // Rather not handle all thread updates.
        DisableThreadLibraryCalls(hDllHandle);

        // Clean the logfile so we only save this session.
        AYRIA::DEBUG::DeleteLogfile();
        break;
    }

    return TRUE;
}

#else
void __attribute__((constructor)) SOMain()
{
    // Clean the logfile so we only save this session.
    AYRIA::DEBUG::DeleteLogfile();
}

#endif
