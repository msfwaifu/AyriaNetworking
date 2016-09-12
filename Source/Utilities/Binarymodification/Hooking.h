/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: LGPL 3.0
    Started: 2016-9-10
    Notes:
        Some basic hooking support.
*/

#pragma once
#include <Configuration\All.h>
#include <functional>

#define EXTENDEDHOOKDECL(Basehook)                              \
template <typename Functionsignature, typename ...Arguments>    \
struct Basehook ##Ex : public Basehook                          \
{                                                               \
    std::function<Functionsignature> Originalfunction;          \
    void Setfunctionaddress(void *Address = nullptr)      \
    {                                                           \
        if (!Address) Address = s_Location;                     \
        Originalfunction = *(Functionsignature *)Address;       \
    }                                                           \
}                                                               \

// The base interface for hooks.
struct IHook
{
    // Saved information about the placement.
    uint8_t s_Textdata[20];
    void *s_Location;
    void *s_Target;

    // Create and remove a hook at [location].
    virtual bool Installhook(void *Location, void *Target) = 0;
    virtual bool Removehook() = 0;
    virtual bool Reinstall() { return Installhook(s_Location, s_Target); };
};

// A 'dumb' hook that just inserts a jump at the address.
struct Stomphook : public IHook
{
    // Create and remove a hook at [location].
    virtual bool Installhook(void *Location, void *Target) override;
    virtual bool Removehook() override;
};
EXTENDEDHOOKDECL(Stomphook);
