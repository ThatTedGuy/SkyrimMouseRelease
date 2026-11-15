# Nexus page for Mouse Release

Published 2026-11-15: https://www.nexusmods.com/skyrimspecialedition/mods/199209 (version 1.0.0). Updated to 1.0.1 and then 1.1.0 on 2026-11-15.

This file records what is on the live page, so it can be rebuilt or updated without guessing. The description is in BBCode (Nexus's format): copy everything between the two `----- copy -----` lines.

## Page settings as published

- **Mod name:** Mouse Release
- **Summary:** Open a menu and your mouse is free to use with your other monitors. Click, scroll, check a guide, etc. Close the menu and Skyrim takes the mouse back. No alt-tabbing needed.
- **Category:** User Interface
- **Tags:** User Interface, Skyrim Script Extender (SKSE), Quality of Life. Nexus adds AI-Generated Content and AI Media from the AI section.
- **Generative AI usage:** Mod content: AI-Generated. Media: AI-Generated (Nexus counts mod page descriptions as media). Explanation shown on the page:
  > The code and most of the mod page text were written by an AI assistant (Claude, by Anthropic). The idea, design decisions and all in-game testing were done by me. Screenshots are real in-game captures. Source and test log: github.com/ThatTedGuy/SkyrimMouseRelease
- **Image:** [nexus-image-wide.png](nexus-image-wide.png), a 16:9 crop of a real screen recording: the pause menu's SYSTEM tab, the game window's edge, and the cursor on the second monitor. Only cropped, nothing painted. Use real screenshots only.
- **Mod rewards:** opted in; the points are donated to charity through Nexus's store.

## Files

One Main file, `SkyrimMouseRelease.zip` (`SKSE/Plugins/SkyrimMouseRelease.dll` and `.ini`).

- **Display name:** Mouse Release
- **Upload type:** "Update existing file" on the previous version, with "Archive existing file" ticked, so mod managers offer it as an update.
- **File version:** 1.1.0 (1.0.1 archived with this upload), with "Update mod version to match this file's version" ticked.
- **Description:** The plugin and its settings file. One file for 1.5.97, 1.6 and 1.7. Needs SKSE64 for your game version (2.0.20 for 1.5.97) and Address Library's All in One file.
- **Downloads:** mod manager download on, primary file on. The requirements popup option stays greyed out (it seems to need file-to-file requirements).
- **Changelog:** 1.1.0: Frees the mouse in the console too. One file now works on 1.5.97 as well as 1.6 and 1.7. During play, the hidden cursor stays inside the game window, so scrolling can't reach your other screens. Items, the character and the skills sky can be turned with the mouse again in the vanilla menus. 1.0.1: Fixed a timing issue that could leave the Windows cursor showing after a loading screen. 1.0.0: Initial release.

## Requirements

Mod requirements (legacy), not file-to-file, so they don't need relinking when the other mods update:

- Address Library for SKSE Plugins (Nexus), note: Get the All in One file. It covers every game version, 1.5.97 included.
- Skyrim Script Extender (SKSE64) (Nexus), note: Use the build for your game version
- No external resources, no DLC. Don't tick "Anniversary Upgrade": the mod doesn't need the paid Creation Club bundle on any version.

Requirements belong to the whole mod, not to a file, so version details (SKSE 2.0.20 for 1.5.97) go in the notes and the description, not in a separate file.

## Permissions

- **Third-party content:** Yes (I have permission). CommonLibSSE-NG is GPL-3.0; spdlog and fmt are MIT. Their licenses are the permission.
- **Credits:** Built on CommonLibSSE-NG (GPL-3.0) by its contributors. Uses spdlog and fmt (MIT license). Thanks to the SKSE team and meh321 for Address Library.
- **Permission settings:** Write your own (custom). Nexus's recommended settings (no re-upload, ask before modifying) conflict with the GPL. Text:
  > Licensed under the GNU General Public License v3.0 or later (GPL-3.0-or-later). You may modify, convert, re-upload and reuse this mod, as long as your version keeps the same license and you publish its source code. Source and full license: https://github.com/ThatTedGuy/SkyrimMouseRelease

## Releasing an update

1. Change the version in `CMakeLists.txt` (`project(... VERSION x.y.z)`) and `vcpkg.json`, and in the README's header line.
2. Build with `./build.ps1`, test in game, and add a FIXLOG entry.
3. Commit, tag `vx.y.z`, push the commit and the tag.
4. On Nexus, upload the new zip as a Main file with the same version and a changelog line. Archive the old file or move it to Old.
5. Update the requirement notes, file description and description to match, and record them here so this file matches the live page. While preparing a release, list the page changes under a "Pending for x.y.z" heading above this section, and remove it once they are live.

----- copy -----
[size=5][b]Mouse Release[/b][/size]
Open a menu and your mouse is free to use with your other monitors. Click, scroll, check a guide, etc. Close the menu and Skyrim takes the mouse back. No alt-tabbing needed.

Works in the pause menu, main menu, character creation, loading screens, the console, and the inventory, magic and skills menus. The map stays locked. Clicks and scrolls on your other monitor never reach the game, and during play the mouse stays inside the game window.

[size=4][b]Requirements[/b][/size]
[list]
[*][url=https://skse.silverlock.org/]SKSE64[/url] for your game version (2.0.20 for 1.5.97) and the All in One file of [url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url]
[*]Skyrim SE 1.5.97 or AE 1.6.x / 1.7.x, one file for all. Tested on 1.5.97, 1.6.1170 and 1.7.104. Not for VR.
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
