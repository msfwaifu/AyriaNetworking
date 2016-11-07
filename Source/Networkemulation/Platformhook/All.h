/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-11-7
    Notes:
        Includes all the platform specific hooks.
*/

#pragma once
#include <STDInclude.h>
#include <unordered_map>

namespace Platform
{
    // The hooks themselves defined in Winsock.cpp
    extern std::unordered_map<uint64_t /* Hash */, void * /* Hook */> Hooks;

    // Initializers for the hooks.
    void Initialize_WININET();
    void Initialize_Winsock();
}

// Defines for the hooks.
#define InstallhookIAT(Module, Export, Function)                                        \
{                                                                                       \
    auto Address = IAT::Findfunction(Module, Function);                                 \
    if(Address) *(size_t *)Address = size_t(Function);                                  \
}
#define InstallhookStomp(Module, Export, Function)                                      \
{                                                                                       \
    auto Address = IAT::Findfunction(Module, Function);                                 \
    if(Address)                                                                         \
    {                                                                                   \
        auto ID = FNV1::Compiletime::FNV1_64(#Function);                                \
        auto Hook = new HOOK::StomphookEx<decltype(Function)>();                        \
        Hook->Setfunctionaddress((void *)Address);                                      \
        Hook->Installhook((void *)Address, Function);                                   \
        Platform::Hooks[ID] = Hook;                                                     \
    }                                                                                   \
}
#define CallhookRaw(Function, ...)                                                      \
{                                                                                       \
    auto ID = FNV1::Compiletime::FNV1_64(#Function);                                    \
    auto Pointer = Platform::Hooks[ID];                                                 \
    if(!Pointer) Function(__VA_ARGS__);                                                 \
    else                                                                                \
    {                                                                                   \
        auto Hook = reinterpret_cast<HOOK::StomphookEx<decltype(Function)> *>(Pointer); \
        auto _Function = Hook->Originalfunction;                                        \
        Hook->Removehook();                                                             \
        _Function(__VA_ARGS__);                                                         \
        Hook->Reinstall();                                                              \
    }                                                                                   \
}
#define Callhook(Function, Result, ...)                                                 \
{                                                                                       \
    auto ID = FNV1::Compiletime::FNV1_64(#Function);                                    \
    auto Pointer = Platform::Hooks[ID];                                                 \
    if(!Pointer) *Result = Function(__VA_ARGS__);                                       \
    else                                                                                \
    {                                                                                   \
        auto Hook = reinterpret_cast<HOOK::StomphookEx<decltype(Function)> *>(Pointer); \
        auto _Function = Hook->Originalfunction;                                        \
        Hook->Removehook();                                                             \
        *Result = _Function(__VA_ARGS__);                                               \
        Hook->Reinstall();                                                              \
    }                                                                                   \
}
