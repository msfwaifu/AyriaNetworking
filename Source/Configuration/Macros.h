/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-10-21
    Notes:
        C style preprocessor macros for common functions.
*/

#pragma once
#include "Defines.h"

// Debug information.
#ifdef NDEBUG
#define PrintFunction()
#define DebugPrint(string)
#define InfoPrint(string)
#define VAPrint(format, ...)
#else
#define PrintFunction() AYRIA::DEBUG::AppendLogfile(__FUNCTION__, "Call to")
#define DebugPrint(string) AYRIA::DEBUG::AppendLogfile(string, "Debug")
#define InfoPrint(string) AYRIA::DEBUG::AppendLogfile(string, "Info")
#define VAPrint(format, ...) AYRIA::DEBUG::AppendLogfileTimestamp(AYRIA::va(format, __VA_ARGS__))
#endif
