# Nexus page for Mouse Release

Everything to fill in on the Nexus upload form. The description is in BBCode (Nexus's format): copy everything between the two `----- copy -----` lines into the description box.

## Form fields

- **Mod name:** Mouse Release
- **Summary** (shown under the title, short): Frees your mouse while Skyrim's menus are open, so you can use your other monitors without alt-tabbing. Clicks and scrolls on the other screen never reach the game.
- **Category:** User Interface (or Utilities)
- **Version:** 1.0.0
- **Requirements:** SKSE64; Address Library for SKSE Plugins (mod 32444)
- **AI disclosure:** AI-Generated Content. Explanation: use the "AI disclosure" paragraph from the description.
- **Main file:** `SkyrimMouseRelease.zip` from the project folder, named "Mouse Release 1.0.0".
- **Permissions:** the source is GPL-3.0-or-later, so others may modify and reupload as long as they keep the same license and publish their source. Set permissions to match, and link the source.
- **Images:** real in-game screenshots only (for example the pause menu with the cursor next to it, or a desktop shot with both monitors).

----- copy -----
[size=5][b]Mouse Release[/b][/size]
Use your other monitors while Skyrim is paused.

Skyrim keeps the mouse locked inside the game, even in menus. With this mod, opening the pause menu (and a few others) frees the mouse: move it to your second monitor, click, scroll, check a guide, change a song. Close the menu and the game takes the mouse back as usual. No alt-tabbing.

[size=4][b]Where the mouse is freed[/b][/size]
[list]
[*]Pause menu
[*]Main menu
[*]Character creation
[*]Loading screens
[*]Inventory, magic and skills menus
[/list]
The map and the Tab menu stay locked, because the map scrolls when the cursor touches the screen edge. You can change the list in the INI.

[size=4][b]What it takes care of[/b][/size]
[list]
[*]One cursor at a time: Skyrim's inside the game, the Windows cursor outside it.
[*]Clicks and scrolling on your other monitor never reach the game's menu.
[*]Clicking back into the game works normally, as often as you like.
[*]Reopening a menu, the cursor starts where you left it.
[/list]

[size=4][b]Requirements[/b][/size]
[list]
[*][url=https://skse.silverlock.org/]SKSE64[/url]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url]
[*]Skyrim Anniversary Edition runtime. Tested on 1.6.1170. 1.7.x should work but hasn't been tested. Not for 1.5.97 or VR.
[*]Borderless or windowed mode. Tested in borderless; exclusive fullscreen hasn't been tested.
[/list]
[b]SSE Display Tweaks users:[/b] set [i]LockCursor = false[/i] in SSEDisplayTweaks.ini, or it keeps the cursor inside the game window.

[size=4][b]Installation[/b][/size]
Install with your mod manager. There's no plugin file (ESP), so load order doesn't matter, and it doesn't touch your saves: add or remove it at any time.

[size=4][b]Settings[/b][/size]
[i]Data\SKSE\Plugins\SkyrimMouseRelease.ini[/i]
[list]
[*][b]bReleaseMouse[/b]: 0 turns the mod off.
[*][b]sMenus[/b]: which menus free the mouse.
[*][b]bVerboseLog[/b]: 1 writes a detailed log for bug reports.
[/list]

[size=4][b]Known limits[/b][/size]
[list]
[*]Gamepad use in these menus hasn't been tested.
[*]If the game loses focus at the exact moment a menu closes, handing the mouse back can fail; the game recovers as soon as you click back in.
[/list]

[size=4][b]Bug reports[/b][/size]
Set bVerboseLog=1 in the INI, reproduce the problem, and attach [i]Documents\My Games\Skyrim Special Edition\SKSE\SkyrimMouseRelease.log[/i] along with your game version and display mode.

[size=4][b]AI disclosure[/b][/size]
The code was written by an AI assistant (Claude, by Anthropic). The idea, the design decisions and all in-game testing are mine. The source and a log of every test and fix round are public on GitHub.

[size=4][b]Source code[/b][/size]
GPL-3.0-or-later: [url=GITHUB_LINK]GitHub[/url]

[size=4][b]Credits[/b][/size]
[list]
[*]The SKSE team
[*]meh321 for Address Library
[*]CommonLibSSE-NG and its contributors
[/list]
----- copy -----
