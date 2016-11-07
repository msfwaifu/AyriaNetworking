/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-10-20
    Notes:
        Adds or removes protection from a range of pages.
*/

#pragma once
#include <cstdint>

namespace AYRIA
{
    namespace Memprotect
    {
        void Protectrange(void *Address, const size_t Length, unsigned long Oldprotect);
        unsigned long Unprotectrange(void *Address, const size_t Length);
    }
}
