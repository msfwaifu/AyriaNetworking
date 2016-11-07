/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-10-20
    Notes:
        Reads the binaries ImportAddressTable.
*/

#include "IATReader.h"
#include "../Strings/Variadicstring.h"

#ifndef DebugPrint
#define DebugPrint(string)
#endif

#ifdef _WIN32
#include <Windows.h>

size_t PE_Findfunction(std::string Module, std::string Function)
{
    // Target information if we have to use ordinals.
    auto OriginalEntry = (size_t)GetProcAddress(GetModuleHandleA(Module.c_str()), Function.c_str());

    // PE header information.
    size_t Imagebase = (size_t)GetModuleHandleA(NULL);
    auto DOSHeader = (PIMAGE_DOS_HEADER)Imagebase;
    auto NTHeader = (PIMAGE_NT_HEADERS)(DOSHeader->e_lfanew + Imagebase);

    // Verify that the import table has not been destroyed by a packer.
    if (NTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size == 0)
    {
        DebugPrint(AYRIA::va("%s: The current application does not have an import table.", __func__));
        return 0;
    }

    // Iterate through the import table until we find our entry.
    auto Imports = (PIMAGE_IMPORT_DESCRIPTOR)((NTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress) + Imagebase);
    for (size_t i = 0; Imports[i].Characteristics != 0; ++i)
    {
        // Skip unrelated modules.
        if (_stricmp((char *)(Imports[i].Name + Imagebase), Module.c_str()))
            continue;

        // Iterate through the thunks.
        for (size_t c = 0; ; ++c)
        {
            // Fetch the next thunk and verify that it's not the last.
            auto ImportThunkData = (PIMAGE_THUNK_DATA)((size_t)Imports[i].OriginalFirstThunk + (c * sizeof(IMAGE_THUNK_DATA)) + Imagebase);
            if (ImportThunkData->u1.AddressOfData == NULL)
                break;

            // If it's an ordinal, check the address.
            if (IMAGE_SNAP_BY_ORDINAL(ImportThunkData->u1.Ordinal))
            {
                PIMAGE_THUNK_DATA OrdinalThunk = (PIMAGE_THUNK_DATA)((size_t)Imports[i].FirstThunk + (c * sizeof(IMAGE_THUNK_DATA)) + Imagebase);

                if (OriginalEntry == OrdinalThunk->u1.Function)
                {
                    return size_t(&OrdinalThunk->u1.Function);
                }
                else
                    continue;
            }

            // Else we verify by name.
            if (!_stricmp(((PIMAGE_IMPORT_BY_NAME)(ImportThunkData->u1.AddressOfData + Imagebase))->Name, Function.c_str()))
            {
                PIMAGE_THUNK_DATA NameThunk = (PIMAGE_THUNK_DATA)((size_t)Imports[i].FirstThunk + (c * sizeof(IMAGE_THUNK_DATA)) + Imagebase);
                return size_t(&NameThunk->u1.Function);
            }
        }
    }

    return 0;
}
size_t PE_Findordinal(std::string Module, uint32_t Ordinal)
{
    // PE header information.
    size_t Imagebase = (size_t)GetModuleHandleA(NULL);
    auto DOSHeader = (PIMAGE_DOS_HEADER)Imagebase;
    auto NTHeader = (PIMAGE_NT_HEADERS)(DOSHeader->e_lfanew + Imagebase);

    // Verify that the import table has not been destroyed by a packer.
    if (NTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size == 0)
    {
        DebugPrint(AYRIA::va("%s: The current application does not have an import table.", __func__));
        return 0;
    }

    // Iterate through the import table until we find our entry.
    auto Imports = (PIMAGE_IMPORT_DESCRIPTOR)((NTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress) + Imagebase);
    for (size_t i = 0; Imports[i].Characteristics != 0; ++i)
    {
        // Skip unrelated modules.
        if (_stricmp((char *)(Imports[i].Name + Imagebase), Module.c_str()))
            continue;

        // Iterate through the thunks.
        for (size_t c = 0; ; ++c)
        {
            // Fetch the next thunk and verify that it's not the last.
            auto ImportThunkData = (PIMAGE_THUNK_DATA)((size_t)Imports[i].OriginalFirstThunk + (c * sizeof(IMAGE_THUNK_DATA)) + Imagebase);
            if (ImportThunkData->u1.AddressOfData == NULL)
                break;

            // Skip to our ordinal.
            if (IMAGE_SNAP_BY_ORDINAL(ImportThunkData->u1.Ordinal) && IMAGE_ORDINAL(ImportThunkData->u1.Ordinal) != Ordinal)
                continue;

            auto OrdinalThunk = (PIMAGE_THUNK_DATA)((size_t)Imports[i].FirstThunk + (c * sizeof(IMAGE_THUNK_DATA)) + Imagebase);
            size_t(&OrdinalThunk->u1.Function);
        }
    }

    return 0;
}

#else

size_t ELF_Findfunction(std::string Module, std::string Function)
{
    /* TODO(Convery): Implement this when needed. */
    return 0;
}
size_t ELF_Findordinal(std::string Module, uint32_t Ordinal)
{
    /* TODO(Convery): Implement this when needed. */
    return 0;
}
#endif

size_t AYRIA::IAT::Findfunction(std::string Module, std::string Function)
{
#ifdef _WIN32
    return PE_Findfunction(Module, Function);
#else
    return ELF_Findfunction(Module, Function);
#endif
}
size_t AYRIA::IAT::Findordinal(std::string Module, uint32_t Ordinal)
{
#ifdef _WIN32
    return PE_Findordinal(Module, Ordinal);
#else
    return ELF_Findordinal(Module, Ordinal);
#endif
}
