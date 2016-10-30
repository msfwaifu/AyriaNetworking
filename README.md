# Ayria Networking Plugin

This plugin is intended to simplify network emulation by intercepting calls to the OS's network implementation and redirecting them to a [NetworkModule](https://github.com/AyriaPublic/Networkmodule_Template) that handles the data locally. A new instance of the virtual server is created for each hostname and connections are identified by a `size_t` socket.

- [x] Windows ws2_32 emulation for offline play.
- [ ] Linux network emulation for offline play.
- [x] Windows HTTP emulation.
- [x] Passthrough for unhandled hostnames.
- [x] Proxying IPv4 addresses from 192. to 240.

## Plugin loading

The plugin should, like all other plugins, be placed in the games `./Plugins/` directory where it gets loaded by the [Bootstrap](https://github.com/AyriaPublic/NativeBootstrap) module which is injected into the game by the desktop client. It requires a `Configuration.csv` file in `./Plugins/AyriaNetworking/Networkingmodules` along with the [NetworkModules](https://github.com/AyriaPublic/Networkmodule_Template) themselves. The `./Plugins/AyriaNetworking/Networkingstorage` directory is reserved for the [NetworkModules](https://github.com/AyriaPublic/Networkmodule_Template) data.

## Configuration.csv example

```
# Layout: Modulename,Hostname

# Exampleserver that redirects localhost lookups.
MyModule.dll,localhost
MyModule.dll,127.0.0.1

# End of file.
```
