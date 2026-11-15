# SkyrimMouseRelease

SKSE plugin for Skyrim SE/AE (built for runtime 1.6.1170) that lets the mouse leave the game window while the pause menu is open, so it can move to a second monitor. When the menu closes, the game takes the mouse back.

Status: prototype, not yet tested in game.

## How it works

- Skyrim reads the mouse through DirectInput in exclusive mode, which hides and traps the Windows cursor.
- The plugin listens for menu open and close events (`MenuOpenCloseEvent`).
- When a watched menu opens, it switches the game's mouse device (`BSWin32MouseDevice::dInputDevice`) to shared mode (`DISCL_NONEXCLUSIVE | DISCL_FOREGROUND`) and removes any cursor clip (`ClipCursor(nullptr)`).
- When the menu closes, it switches back to `iRestoreFlags` (default exclusive + foreground).
- Device changes run on the main thread through the SKSE task interface.

SSE Display Tweaks must have `LockCursor = false`, or it clips the cursor to the game window again.

## Settings (`Data/SKSE/Plugins/SkyrimMouseRelease.ini`)

| Key | Default | Meaning |
|---|---|---|
| `bReleaseMouse` | `0` | `1` frees the mouse in watched menus; `0` only logs menu events. |
| `sMenus` | `Journal Menu` | Comma-separated menu names that free the mouse. The pause menu is `Journal Menu`. |
| `iRestoreFlags` | `5` | DirectInput flags restored on close (`1` exclusive, `2` non-exclusive, `4` foreground, `8` background). |

## Log

`Documents/My Games/Skyrim Special Edition/SKSE/SkyrimMouseRelease.log` lists loaded settings, every menu open and close, and the result codes of each mouse mode change.

## Build

Requires Visual Studio 2026 with the C++ workload (its bundled vcpkg is used) plus CMake and Ninja from `vcpkg fetch cmake` and `vcpkg fetch ninja`.

```powershell
git submodule update --init --recursive
./build.ps1
```

Output: `dist/SKSE/Plugins/` (DLL and INI) and `SkyrimMouseRelease.zip`, which installs in Vortex like any mod archive.

CommonLibSSE-NG comes from the maintained fork `alandtse/CommonLibVR` (branch `ng`) as a submodule in `extern/`. Only the AE runtime is enabled.

## Known unknowns

- Two cursors may show in the menu (Skyrim's and Windows'), not lined up.
- The game may re-take exclusive mode on its own (for example on focus changes).
