# Test installs

Vanilla Skyrim installs for testing the plugin on game versions other than the main setup's (1.6.1170 through Runtime Swapper, with the Gate To Sovngarde collection). Each version gets its own folder under `C:\SkyrimTest`, with only SKSE, Address Library and this plugin. The main install, Vortex and Runtime Swapper are never touched.

| Version | Folder | SKSE | Address Library file |
|---|---|---|---|
| 1.5.97 | `C:\SkyrimTest\1.5.97` | 2.0.20 | All in One (1.7.104.0), the `1-5-97` .bin |
| 1.7.104 | `C:\SkyrimTest\1.7.104` | the build for 1.7.104 | All in One (1.7.104.0), the `1-7-104` .bin |

Run the scripts from the project folder, in a normal (not administrator) PowerShell window. Windows blocks scripts by default, so first allow them for that window only:

```powershell
cd C:\Users\Ted\Projects\SkyrimMouseRelease
Set-ExecutionPolicy -Scope Process Bypass -Force
```

## Setting one up

1. `./testing/Setup-TestInstall.ps1 -Version 1.5.97`. It opens the Steam console and prints three `download_depot` commands. Paste them one at a time; each takes a while (about 15 GB in all).
2. When all three say "Depot download complete", run the script again. It moves the files from Steam's `steamapps\content` folder into the test folder.
3. Extract SKSE into the test folder and put the Address Library `.bin` for that version into `Data\SKSE\Plugins`. Run the script again until it says "Ready".
4. Do one version at a time: both downloads use the same Steam folders.

If Steam answers "Manifest not available" for 1.5.97, use the Skyrim Downgrade Patcher on Nexus to make the 1.5.97 copy instead.

## Running a test

`./testing/Run-Test.ps1 -Version 1.5.97`, with Vortex and Skyrim closed. It:

1. Copies the newest build from `dist/` into the test install.
2. Renames the shared profile folders (`Documents\My Games\Skyrim Special Edition` and `AppData\Local\Skyrim Special Edition`) to `Skyrim Special Edition.MouseReleaseHold`, so the test game can't change the main setup's INIs, saves or load order.
3. Starts the game in a borderless window at the main screen's size and waits for it to close.
4. Moves the test game's profile folders to `C:\SkyrimTest\runs\<version>-<time>\` and renames the originals back, even if the game crashes.
5. Prints where the plugin log is and whether it has warnings.

If the script was stopped before restoring (PowerShell closed, PC restarted), run `./testing/Run-Test.ps1 -RestoreOnly` before starting Vortex or the main game.

## What to test

From the main menu, open the console (`~`) and type `coc riverwood` to skip the intro.

1. **Main menu:** move to the other monitor, click there, come back, click a menu item.
2. **Console:** open it in play, click an NPC (its ID shows at the top), move to the other monitor and click there (nothing happens in the game), close it. The mouse is locked again, no Windows arrow, and the camera turns.
3. **Console over a menu:** open the inventory, open the console, close the console, close the inventory.
4. **Alt-tab** with the console open, then click back in.
5. **Loading screen:** `coc whiterun` with the mouse on the other monitor. No arrow in play afterwards.
6. **Pause, inventory, magic, skills:** each frees the mouse, and items, spells and constellations still turn.
7. **Character creator:** `showracemenu`, turn the character, move out and back, close.
8. Quit from the pause menu.

The log should have no warning (`[W]`) or error (`[E]`) lines.
