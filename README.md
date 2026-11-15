# SkyrimMouseRelease

SKSE plugin for Skyrim SE/AE (built for runtime 1.6.1170) that lets the mouse leave the game window while the pause menu is open, so it can move to a second monitor. When the menu closes, the game takes the mouse back.

Status: prototype. Fix history is in [FIXLOG.md](FIXLOG.md).

## How it works

- Skyrim reads the mouse through DirectInput in foreground + exclusive mode, which hides and traps the Windows cursor.
- The plugin listens for menu open and close events (`MenuOpenCloseEvent`). These arrive on several threads, so the event only sets a "wanted" flag.
- A hook on `BSWin32MouseDevice::Poll` (vtable slot 2, main thread, every frame) applies the flag:
  - **Release:** switches the game's mouse device (`dInputDevice`) to shared mode (`DISCL_NONEXCLUSIVE | DISCL_FOREGROUND`) and removes any cursor clip.
  - **Restore:** switches back to `iRestoreFlags` (default exclusive + foreground).
  - **While released:** puts Skyrim's menu cursor (`MenuCursor::cursorPosX/Y`) where the Windows cursor is, or on the nearest window edge while the mouse is outside, and sets the frame's queued mouse movement (`BSInputEventQueue`) to zero so the menu doesn't add it on top. It also removes any cursor clip that reappears and re-applies shared mode after a focus change or a device reset.
- Hooks on `Initialize` and `Reinitialize` (slots 1 and 9) log device resets and trigger a re-apply while released.
- The game window procedure is subclassed to log focus changes (`WM_ACTIVATEAPP`) and to hide the Windows cursor over the game while released (`WM_SETCURSOR`), so only one pointer shows: Skyrim's inside the window, Windows' outside it. On release it posts itself a `WM_SETCURSOR` so the Windows cursor is hidden before the mouse first moves.

SSE Display Tweaks must have `LockCursor = false`, or it clips the cursor to the game window again.

## Settings (`Data/SKSE/Plugins/SkyrimMouseRelease.ini`)

| Key | Default | Meaning |
|---|---|---|
| `bReleaseMouse` | `1` | `1` frees the mouse in watched menus; `0` only logs menu events (no hooks installed). |
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

## Known limits

- Zeroed movement applies to every handler of the queued event while released. Only the watched menus are open then, so nothing else reads it.
- Not yet confirmed: whether the menu still redraws its cursor on zero-movement frames, and whether a click outside the window while Skyrim is still active reaches the menu.
