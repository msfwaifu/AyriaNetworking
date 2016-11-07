/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-10-20
    Notes:
        Two types of common hooks.
*/

#include "HOOKs.h"
#include "Memprotect.h"
#include <STDInclude.h>

// Create and remove a hook at [location].
#ifdef ENVIRONMENT64
bool AYRIA::HOOK::Stomphook::Installhook(void *Location, void *Target)
{
    s_Location = (void *)Location;
    s_Target = (void *)Target;

    auto Protection = AYRIA::Memprotect::Unprotectrange(Location, 12);
    {
        // Save the text data before we overwrite it.
        std::memcpy(s_Textdata, Location, 12);

        // movabs rax, Target;
        // jmp rax;
        *(uint8_t *)(size_t(Location) + 0) = 0x48;
        *(uint8_t *)(size_t(Location) + 1) = 0xB8;
        *(uint64_t *)(size_t(Location) + 2) = uint64_t(Target);
        *(uint8_t *)(size_t(Location) + 10) = 0xFF;
        *(uint8_t *)(size_t(Location) + 11) = 0xE0;
    }
    AYRIA::Memprotect::Protectrange(Location, 12, Protection);

    return true;
}
bool AYRIA::HOOK::Stomphook::Removehook()
{
    auto Protection = AYRIA::Memprotect::Unprotectrange(s_Location, 12);
    {
        // Restore the text data.
        std::memcpy(s_Location, s_Textdata, 12);
    }
    AYRIA::Memprotect::Protectrange(s_Location, 12, Protection);

    return true;
}
#else
bool AYRIA::HOOK::Stomphook::Installhook(void *Location, void *Target)
{
    s_Location = (void *)Location;
    s_Target = (void *)Target;

    auto Protection = AYRIA::Memprotect::Unprotectrange(Location, 5);
    {
        // Save the text data before we overwrite it.
        std::memcpy(s_Textdata, Location, 5);

        *(uint8_t *)(size_t(Location) + 0) = 0xE9;
        *(uint32_t *)(size_t(Location) + 1) = uint32_t(Target) - uint32_t(Location) - 5;
    }
    AYRIA::Memprotect::Protectrange(Location, 5, Protection);

    return true;
}
bool AYRIA::HOOK::Stomphook::Removehook()
{
    auto Protection = AYRIA::Memprotect::Unprotectrange(s_Location, 5);
    {
        // Restore the text data.
        std::memcpy(s_Location, s_Textdata, 5);
    }
    AYRIA::Memprotect::Protectrange(s_Location, 5, Protection);

    return true;
}
#endif

// Create and remove a hook at [location].
#ifdef ENVIRONMENT64
bool AYRIA::HOOK::Callhook::Installhook(void *Location, void *Target)
{
    s_Location = (void *)Location;
    s_Target = (void *)Target;

    auto Protection = AYRIA::Memprotect::Unprotectrange(Location, 12);
    {
        // Save the text data before we overwrite it.
        std::memcpy(s_Textdata, Location, 12);

        // movabs rax, Target;
        // jmp rax;
        *(uint8_t *)(size_t(Location) + 0) = 0x48;
        *(uint8_t *)(size_t(Location) + 1) = 0xB8;
        *(uint64_t *)(size_t(Location) + 2) = uint64_t(Target);
        *(uint8_t *)(size_t(Location) + 10) = 0xFF;
        *(uint8_t *)(size_t(Location) + 11) = 0xD0;
    }
    AYRIA::Memprotect::Protectrange(Location, 12, Protection);

    return true;
}
bool AYRIA::HOOK::Callhook::Removehook()
{
    auto Protection = AYRIA::Memprotect::Unprotectrange(s_Location, 12);
    {
        // Restore the text data.
        std::memcpy(s_Location, s_Textdata, 12);
    }
    AYRIA::Memprotect::Protectrange(s_Location, 12, Protection);

    return true;
}
#else
bool AYRIA::HOOK::Callhook::Installhook(void *Location, void *Target)
{
    s_Location = (void *)Location;
    s_Target = (void *)Target;

    auto Protection = AYRIA::Memprotect::Unprotectrange(Location, 5);
    {
        // Save the text data before we overwrite it.
        std::memcpy(s_Textdata, Location, 5);

        *(uint8_t *)(size_t(Location) + 0) = 0xE8;
        *(uint32_t *)(size_t(Location) + 1) = uint32_t(Target) - uint32_t(Location) - 5;
    }
    AYRIA::Memprotect::Protectrange(Location, 5, Protection);

    return true;
}
bool AYRIA::HOOK::Callhook::Removehook()
{
    auto Protection = AYRIA::Memprotect::Unprotectrange(s_Location, 5);
    {
        // Restore the text data.
        std::memcpy(s_Location, s_Textdata, 5);
    }
    AYRIA::Memprotect::Protectrange(s_Location, 5, Protection);

    return true;
}
#endif
