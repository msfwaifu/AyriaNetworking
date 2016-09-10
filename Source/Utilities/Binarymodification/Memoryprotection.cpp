/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-9-10
    Notes:
		Adds or removes protection from a range of pages.
*/

#include "Memoryprotection.h"

#ifdef _WIN32
#include <Windows.h>

void Protectrange(void *Address, const size_t Length, unsigned long Oldprotect)
{
    unsigned long Temp;
    VirtualProtect(Address, Length, Oldprotect, &Temp);
}
unsigned long Unprotectrange(void *Address, const size_t Length)
{
    unsigned long Oldprotect;
    VirtualProtect(Address, Length, PAGE_EXECUTE_READWRITE, &Oldprotect);
    return Oldprotect;
}

#else
#include <sys/mman.h>

void Protectrange(void *Address, const size_t Length, unsigned long Oldprotect)
{
    mprotect(Address, Length, Oldprotect);
}
unsigned long Unprotectrange(void *Address, const size_t Length)
{
    /*
        TODO(Convery):
        We need to parse /proc/self/maps to get the access.
        Implement this when needed.
    */

    mprotect(Address, Length, (PROT_READ | PROT_WRITE | PROT_EXEC));
    return (PROT_READ | PROT_EXEC);
}

#endif
