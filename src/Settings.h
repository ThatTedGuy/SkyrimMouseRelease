#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct Settings
{
	bool                     releaseMouse{ true };
	std::vector<std::string> menus{ "Journal Menu", "Main Menu", "RaceSex Menu", "Loading Menu", "InventoryMenu", "MagicMenu", "StatsMenu" };
	std::uint32_t            restoreFlags{ 0x5 };  // DISCL_EXCLUSIVE | DISCL_FOREGROUND

	static Settings& Get();
	void             Load();
	bool             IsWatchedMenu(std::string_view a_menu) const;
};
