# SkyrimMouseRelease

SKSE plugin for Skyrim SE/AE (built for runtime 1.6.1170) that lets the mouse leave the game window while a menu is open (by default the pause menu, main menu, character creator, loading screens, inventory, magic and skills), so it can move to a second monitor. When the last of those menus closes, the game takes the mouse back.

Status: all default menus tested and working on one setup (1.6.1170, 1920x1080, two monitors). The pause-menu-only version is saved as tag `v0.1-pause-menu` with its build in `backups/`. Not yet released. Fix history is in [FIXLOG.md](FIXLOG.md).

## How it works

- Skyrim reads the mouse through DirectInput in foreground + exclusive mode, which hides and traps the Windows cursor.
- The plugin listens for menu open and close events (`MenuOpenCloseEvent`) and keeps the set of watched menus that are open; the mouse is wanted free while that set is not empty. Events arrive on several threads, so they only update the set (under a lock) and a "wanted" flag. Watched menus already open when the listener starts (the main menu) are added at that point.
- A hook on `BSWin32MouseDevice::Poll` (vtable slot 2, once per frame; main thread while paused, a worker thread during play) applies the flag:
  - **Release:** moves the Windows cursor to where Skyrim's menu cursor already is (the game keeps that spot, but the Windows cursor gets centered during play), then switches the game's mouse device (`dInputDevice`) to shared mode (`DISCL_NONEXCLUSIVE | DISCL_FOREGROUND`) and removes any cursor clip.
  - **Restore:** if the Windows cursor is outside the game window, moves it to the nearest edge spot; then resets it to no shape (one-shot `WM_SETCURSOR`) and switches back to `iRestoreFlags` (default exclusive + foreground).
  - **Failures:** if freeing or restoring fails (`SetCooperativeLevel` refused), it waits 1, 2, 4, then 8 seconds between tries instead of retrying every frame; focus coming back retries at once.
  - **While released:** puts Skyrim's menu cursor (`MenuCursor::cursorPosX/Y`) where the Windows cursor is, or on the nearest window edge while the mouse is outside, and sets the frame's queued mouse movement (`BSInputEventQueue`) to zero so the menu doesn't add it on top. While the cursor is outside the window, mouse button presses and wheel turns are unlinked from that queue, so clicks and scrolls meant for another window do nothing in the menu (button releases are kept, so a drag that ends outside still ends). It also removes any cursor clip that reappears and re-applies shared mode after a focus change or a device reset.
- Hooks on `Initialize` and `Reinitialize` (slots 1 and 9) log device resets and trigger a re-apply while released.
- The game window procedure is subclassed to log focus changes (`WM_ACTIVATEAPP`) and to pick the Windows cursor shape over the game while released (`WM_SETCURSOR`), so only one pointer shows: no shape while Skyrim draws its own cursor (`Cursor Menu` open), the arrow when it doesn't (loading screens). On release, on restore, and when `Cursor Menu` opens or closes, it posts itself a `WM_SETCURSOR` so the shape is right before the mouse first moves.

SSE Display Tweaks must have `LockCursor = false`, or it clips the cursor to the game window again.

## Settings (`Data/SKSE/Plugins/SkyrimMouseRelease.ini`)

| Key | Default | Meaning |
|---|---|---|
| `bReleaseMouse` | `1` | `1` frees the mouse in watched menus; `0` turns the mod off (no hooks installed). Accepts `0`/`1`/`true`/`false`, any case. |
| `sMenus` | `Journal Menu, Main Menu, RaceSex Menu, Loading Menu, InventoryMenu, MagicMenu, StatsMenu` | Comma-separated menu names (case-sensitive) that free the mouse: pause menu, title screen, character creator, loading screens, items, magic, skills. Left out on purpose: the map (`MapMenu`), which scrolls when the cursor sits at the screen edge, and the Tab menu (`TweenMenu`), which the map opens from. |
| `bVerboseLog` | `0` | `1` logs every menu event, focus change, cursor position and device reset (debug level); `0` logs settings, watched menus, mode changes and problems. |
| `iRestoreFlags` | `5` | DirectInput flags restored on close (`1` exclusive, `2` non-exclusive, `4` foreground, `8` background). |

## Log

`Documents/My Games/Skyrim Special Edition/SKSE/SkyrimMouseRelease.log` is rewritten each launch. By default it lists loaded settings, watched menus opening and closing, the result codes of each mouse mode change, failures and retries, device resets while released, and how many clicks or scrolls were blocked outside the window. With `bVerboseLog=1` it adds every other menu event, focus changes and cursor positions.

## Build

Requires Visual Studio 2026 with the C++ workload (its bundled vcpkg is used) plus CMake and Ninja from `vcpkg fetch cmake` and `vcpkg fetch ninja`.

```powershell
git submodule update --init --recursive
./build.ps1
```

Output: `dist/SKSE/Plugins/` (DLL and INI) and `SkyrimMouseRelease.zip`, which installs in Vortex like any mod archive.

CommonLibSSE-NG comes from the maintained fork `alandtse/CommonLibVR` (branch `ng`) as a submodule in `extern/`. Only the AE runtime is enabled.

## Known limits

- Zeroed movement applies to every handler of the queued event while released. The character creator, inventory item view and skills menu still turn normally (test 8), so they don't rely on it.
- If the game window loses focus at the moment a menu closes, restoring exclusive mode fails (`Acquire=0x80070005`); the game rebuilds the mouse device on the next activation and recovers.
- The menu redraws its cursor only on mouse movement, so position changes show on the next move.
- Tested only on runtime 1.6.1170. The plugin uses Address Library and declares itself compatible with all AE runtimes, including 1.7.x, where some engine layouts moved (CommonLib accounts for them). Release decision: state 1.7.x as untested rather than block it.
- Gamepad use in freed menus is untested.
