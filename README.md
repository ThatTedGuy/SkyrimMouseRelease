# Mouse Release

`SkyrimMouseRelease.dll` · by ThatTedGuy · version 1.0.1 · [Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/199209)

An SKSE plugin for Skyrim Special Edition / Anniversary Edition. While you're in a menu, your mouse can leave the game window and work on your other monitors. When you go back to playing, the game takes the mouse back.

The mouse is freed in the pause menu, main menu, character creator, loading screens, inventory, magic and skills menus, and the console. The map and the Tab menu stay locked, because the map scrolls when the cursor touches the screen edge.

## Requirements

- Skyrim SE 1.5.97 or AE 1.6.x / 1.7.x. One DLL works on all of them. Tested on 1.6.1170; 1.5.97 and 1.7.104 tests are pending.
- [SKSE64](https://skse.silverlock.org/) for your game version (2.0.20 for 1.5.97)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444): the "All in One" file, which covers every game version
- If you use SSE Display Tweaks, set `LockCursor = false` in its INI, or it keeps the cursor inside the window in menus too. The plugin keeps it inside during play itself (`bLockCursorInPlay`).

## Settings

`Data/SKSE/Plugins/SkyrimMouseRelease.ini`, read when the game starts.

| Setting | Default | What it does |
|---|---|---|
| `bReleaseMouse` | `1` | `0` turns the mod off. |
| `sMenus` | pause, main, character creator, loading, inventory, magic, skills, console | Which menus free the mouse, by Skyrim's internal names, comma-separated. |
| `bLockCursorInPlay` | `1` | During play, keeps the hidden cursor inside the game window, so scrolling can't reach programs on other screens. Released in the menus and on alt-tab. |
| `bVerboseLog` | `0` | `1` writes much more detail to the log, for bug reports. |
| `iRestoreFlags` | `5` | Advanced. How the mouse is handed back to the game. Leave it unless the log says handing back failed. |

## Log

`Documents/My Games/Skyrim Special Edition/SKSE/SkyrimMouseRelease.log`, started fresh each launch. Include it with bug reports.

## How it works

Skyrim reads the mouse through DirectInput in exclusive mode, which hides the Windows cursor and keeps it in the game. While one of the menus above is open, the plugin switches the game's mouse to shared mode, so the Windows cursor can move anywhere. When the last of those menus closes, it switches back.

Shared mode on its own would show two cursors that drift apart. So every frame while the mouse is free, the plugin puts Skyrim's cursor where the Windows cursor is, cancels the game's own mouse movement, and ignores clicks and scrolling while the cursor is on another screen. It also hides whichever cursor shouldn't show, and frees the mouse again whenever the game rebuilds its mouse device (which it does every time the window gets focus back).

The code is in [src/](src): [main.cpp](src/main.cpp) watches menus, [MouseRelease.cpp](src/MouseRelease.cpp) does all the mouse work (its top comment is a more detailed overview), and [Settings.cpp](src/Settings.cpp) reads the INI. [FIXLOG.md](FIXLOG.md) records every round of fixes and why. [testing/](testing/README.md) sets up vanilla test installs for other game versions.

## Building

Needs Visual Studio 2026 with the C++ workload, plus CMake and Ninja (`vcpkg fetch cmake`, `vcpkg fetch ninja`, using the vcpkg that comes with Visual Studio).

```powershell
git submodule update --init --recursive
./build.ps1
```

This produces `dist/SKSE/Plugins/` (the DLL and INI) and `SkyrimMouseRelease.zip`, which installs like any mod. The plugin is built on [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng), included as a submodule in `extern/`, with SE and AE support on and VR off, so one DLL loads on every SE and AE runtime.

## Known limits

- Tested on one PC: runtime 1.6.1170, 1920x1080, two monitors.
- Gamepad use in these menus is untested.
- If the game loses focus at the exact moment a menu closes, handing the mouse back can fail. The game recovers by itself when you click back in.

## AI disclosure

The code was written by an AI assistant (Claude, by Anthropic). The idea, the design decisions and all in-game testing are mine. [FIXLOG.md](FIXLOG.md) records each round of testing and fixes.

## License

Copyright (C) 2026 ThatTedGuy. Licensed under the [GNU General Public License v3.0 or later](LICENSE), as required by [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng), which this plugin is built on.
