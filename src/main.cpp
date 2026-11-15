// Plugin entry point: loads the settings, installs the hooks and watches menus open and close.

#include "MouseRelease.h"
#include "Settings.h"

#include <mutex>
#include <set>

namespace
{
	// Watched menus that are open right now; the mouse stays free while any is. A set rather than a
	// counter, so a repeated or missed event can't leave it off by one. Menu events arrive on
	// several threads, hence the lock.
	std::mutex            g_openMenusLock;
	std::set<std::string> g_openMenus;

	void SetWatchedMenuOpen(std::string_view a_name, bool a_open)
	{
		std::scoped_lock lock{ g_openMenusLock };
		if (a_open) {
			g_openMenus.emplace(a_name);
		} else {
			g_openMenus.erase(std::string{ a_name });
		}
		MouseRelease::SetWantFree(!g_openMenus.empty());
	}

	class MenuWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static MenuWatcher* Get()
		{
			static MenuWatcher instance;
			return &instance;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const std::string_view name = a_event->menuName.c_str();
			const bool             opening = a_event->opening;
			const bool             watched = Settings::Get().IsWatchedMenu(name);

			if (watched) {
				SKSE::log::info("{} {}", name, opening ? "opened" : "closed");
				SetWatchedMenuOpen(name, opening);
			} else {
				SKSE::log::debug("{} {}", name, opening ? "opened" : "closed");
			}

			if (name == RE::CursorMenu::MENU_NAME) {
				MouseRelease::SetGameCursorShown(opening);
			}

			return RE::BSEventNotifyControl::kContinue;
		}
	};

	void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
	{
		if (a_message->type != SKSE::MessagingInterface::kDataLoaded) {
			return;
		}

		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			SKSE::log::error("UI not available; menus are not watched");
			return;
		}
		ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::Get());
		SKSE::log::info("Watching menus");

		// Menus that opened before the watcher existed (the main menu, usually) sent no event.
		MouseRelease::SetGameCursorShown(ui->IsMenuOpen(RE::CursorMenu::MENU_NAME));
		for (const auto& menu : Settings::Get().menus) {
			if (ui->IsMenuOpen(menu)) {
				SKSE::log::info("{} was already open", menu);
				SetWatchedMenuOpen(menu, true);
			}
		}

		MouseRelease::InstallWindowHook();
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SKSE::log::info("SkyrimMouseRelease loaded");

	Settings::Get().Load();
	if (!Settings::Get().releaseMouse) {
		SKSE::log::info("bReleaseMouse is off; doing nothing");
		return true;
	}

	MouseRelease::InstallHooks();
	if (!SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage)) {
		SKSE::log::error("Could not register for SKSE messages");
		return false;
	}
	return true;
}
