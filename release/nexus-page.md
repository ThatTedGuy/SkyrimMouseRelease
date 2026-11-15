# Nexus page for Mouse Release

Fields for the Nexus upload form, then the description. The description is in BBCode (Nexus's format): copy everything between the two `----- copy -----` lines.

## Form fields

- **Mod name:** Mouse Release
- **Summary:** Use your other monitors while Skyrim's menus are open, without alt-tabbing.
- **Category:** User Interface
- **Version:** 1.0.0
- **Requirements:** SKSE64, Address Library for SKSE Plugins
- **AI disclosure:** AI-Generated Content, with the AI paragraph from the description.
- **Main file:** `SkyrimMouseRelease.zip`, named "Mouse Release 1.0.0".
- **Permissions:** GPL-3.0-or-later. Others may modify and reupload if they keep the license and publish their source.
- **Images:** real screenshots only.

----- copy -----
[size=5][b]Mouse Release[/b][/size]
Skyrim locks your mouse inside the game, even in menus. With this mod, opening a menu frees it: move to your other monitor, click and scroll there, then close the menu and keep playing. No alt-tabbing.

Works in the pause menu, main menu, character creation, loading screens, and the inventory, magic and skills menus. The map stays locked. Clicks and scrolls on your other monitor never reach the game.

[size=4][b]Requirements[/b][/size]
[list]
[*][url=https://skse.silverlock.org/]SKSE64[/url] and [url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url]
[*]Anniversary Edition runtime. Tested on 1.6.1170; 1.7.x is untested. Not for 1.5.97 or VR.
[*]Borderless or windowed mode
[*]SSE Display Tweaks users: set [i]LockCursor = false[/i]
[/list]

No ESP and nothing in your saves, so it's safe to add or remove anytime. Settings are in [i]Data\SKSE\Plugins\SkyrimMouseRelease.ini[/i].

[size=4][b]Bug reports[/b][/size]
Set [i]bVerboseLog=1[/i] in the INI, reproduce the problem, and attach [i]Documents\My Games\Skyrim Special Edition\SKSE\SkyrimMouseRelease.log[/i].

[size=4][b]AI disclosure[/b][/size]
The code was written by an AI assistant (Claude, by Anthropic). The idea, design decisions and all testing are mine. The source and a log of every test round are on [url=https://github.com/ThatTedGuy/SkyrimMouseRelease]GitHub[/url] (GPL-3.0).

[size=4][b]Credits[/b][/size]
The SKSE team, meh321 (Address Library), CommonLibSSE-NG contributors.
----- copy -----
