Halo: Combat Evolved for the PS Vita, built from the decompilation of the Xbox game. **No game data is included**: you need your own Xbox copy of Halo: Combat Evolved.

## Install

1. Install `halo.vpk` with VitaShell. The bubble is called **Halo CE**.
2. From your Xbox disc, copy the `maps` folder to `ux0:data/haloce-vita/maps/` and `default.xbe` to `ux0:data/haloce-vita/default.xbe` (the loading screen's picture is read from it).
3. Optional: convert the disc's Bink movies to MP4 into `ux0:data/haloce-vita/movies/` (see the README).

Full instructions, controls and building: [README](README.md) and [port/vita/README.md](port/vita/README.md).

## What works

- The whole campaign from the menus: checkpoints, saves, Save and Quit, cinematics, movies (as MP4), the retail loading screen.
- Multiplayer maps solo (split screen with one player) and system link over Wi-Fi with other Vitas or the Linux/Windows builds.
- A settings panel for quality and controls: hold **Select + Start**.

## Performance

30 fps in cinematics and most of the campaign; the biggest fights (The Silent Cartographer's beach) run in the high teens to low twenties. The render on the first core is the limit; contributions welcome.

## Known issues

- The first load of each level is slow: the game writes its cache file to the memory card.

- Split screen needs a second player, and the Vita has one controller; ad hoc and online play are not done yet.
- 343 Guilty Spark (d20) hangs in the Vita3K emulator; not yet checked on hardware.
- Vita3K shows rainbow noise on some textures' smaller mip levels (an emulator quirk; not seen on hardware).

When you report a problem, attach `ux0:data/haloce-vita/halo.log` and `halo-prev.log`.
