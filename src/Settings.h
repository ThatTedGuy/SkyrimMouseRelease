#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Values from Data/SKSE/Plugins/SkyrimMouseRelease.ini, read once at startup. The defaults below
// apply when the file or a key is missing.
struct Settings
{
	// Off: the plugin installs nothing.
	bool releaseMouse{ true };

	// Menus that free the mouse, by Skyrim's internal menu name (case-sensitive).
	std::vector<std::string> menus{ "Journal Menu", "Main Menu", "RaceSex Menu", "Loading Menu", "InventoryMenu", "MagicMenu", "StatsMenu" };

	// DirectInput flags given back to the game when the last watched menu closes.
	// 0x5 = exclusive + foreground, which is what Skyrim uses.
	std::uint32_t restoreFlags{ 0x5 };

	// On: also log every menu event, focus change and cursor position.
	bool verboseLog{ false };

	static Settings& Get();
	void             Load();
	bool             IsWatchedMenu(std::string_view a_menu) const;
};
