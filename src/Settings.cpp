#include "Settings.h"

#include "SKSE/SKSE.h"

#include <algorithm>
#include <charconv>
#include <fstream>

namespace
{
	constexpr auto kIniPath = "Data/SKSE/Plugins/SkyrimMouseRelease.ini";

	std::string Trim(std::string_view a_text)
	{
		const auto first = a_text.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos) {
			return {};
		}
		const auto last = a_text.find_last_not_of(" \t\r\n");
		return std::string(a_text.substr(first, last - first + 1));
	}

	std::vector<std::string> SplitList(std::string_view a_text)
	{
		std::vector<std::string> items;
		std::size_t              start = 0;
		while (start <= a_text.size()) {
			const auto end = a_text.find(',', start);
			auto       item = Trim(a_text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
			if (!item.empty()) {
				items.push_back(std::move(item));
			}
			if (end == std::string_view::npos) {
				break;
			}
			start = end + 1;
		}
		return items;
	}
}

Settings& Settings::Get()
{
	static Settings instance;
	return instance;
}

void Settings::Load()
{
	std::ifstream file(kIniPath);
	if (!file) {
		SKSE::log::warn("{} not found, using defaults", kIniPath);
	}

	std::string line;
	while (std::getline(file, line)) {
		const auto text = Trim(line);
		if (text.empty() || text.front() == ';' || text.front() == '#' || text.front() == '[') {
			continue;
		}
		const auto eq = text.find('=');
		if (eq == std::string::npos) {
			continue;
		}
		const auto key = Trim(std::string_view(text).substr(0, eq));
		const auto value = Trim(std::string_view(text).substr(eq + 1));

		if (key == "bReleaseMouse") {
			releaseMouse = value == "1" || value == "true";
		} else if (key == "sMenus") {
			menus = SplitList(value);
		} else if (key == "iRestoreFlags") {
			std::uint32_t parsed{};
			if (std::from_chars(value.data(), value.data() + value.size(), parsed).ec == std::errc{}) {
				restoreFlags = parsed;
			}
		}
	}

	std::string menuList;
	for (const auto& menu : menus) {
		menuList += menuList.empty() ? menu : ", " + menu;
	}
	SKSE::log::info("Settings: bReleaseMouse={}, sMenus=[{}], iRestoreFlags={}", releaseMouse, menuList, restoreFlags);
}

bool Settings::IsWatchedMenu(std::string_view a_menu) const
{
	return std::ranges::any_of(menus, [&](const std::string& m) { return m == a_menu; });
}
