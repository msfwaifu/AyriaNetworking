Ayrias Networking Plugin
---

This plugins purpose is to simplify network emulation with plenty of features:

* Local emulation of servers, no internet connection needed.
* Virtual LAN IP negotiation and proxies for internal IPs.
* Only emulates userspecified hostnames.

Extensionloading
--

This plugin is intended to be loaded via Ayrias Bootstrap module (http://git.ayria.se/ayria-core/NativeBootstrap) and should be loaded from the users ./Plugins/ directory. 
To function it requires two directories of its own: ./Plugins/Networkingmodules where the emulated servers are stored along with configurationfiles;
./Plugins/Networkingstorage where the files used by the modules get stored.
