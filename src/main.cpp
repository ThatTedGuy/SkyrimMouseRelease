#include "MouseRelease.h"
#include "Settings.h"

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <mutex>
#include <set>

namespace
{
	// Watched menus that are open right now. A set rather than a counter, so a repeated or missed
	// event can't leave it off by one; menu events arrive on several threads, hence the lock.
	std::mutex            g_openLock;
	std::set<std::string> g_openMenus;

	void SetMenuOpen(std::string_view a_name, bool a_open)
	{
		std::scoped_lock lock{ g_openLock };
		if (a_open) {
			g_openMenus.emplace(a_name);
		} else {
			g_openMenus.erase(std::string{ a_name });
		}
		MouseRelease::SetWanted(!g_openMenus.empty());
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
			const auto&            settings = Settings::Get();
			const bool             watched = settings.IsWatchedMenu(name);

			SKSE::log::info("{} {}{}", name, opening ? "opened" : "closed", watched ? " (watched)" : "");

			if (name == RE::CursorMenu::MENU_NAME) {
				MouseRelease::SetGameCursorShown(opening);
			}

			if (watched && settings.releaseMouse) {
				// Only sets a flag; the switch itself happens in the mouse poll.
				SetMenuOpen(name, opening);
			}

			return RE::BSEventNotifyControl::kContinue;
		}
	};

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		if (a_message->type != SKSE::MessagingInterface::kDataLoaded) {
			return;
		}
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::Get());
			SKSE::log::info("Watching menu events");
			// A watched menu (the main menu, say) may have opened before this sink existed.
			MouseRelease::SetGameCursorShown(ui->IsMenuOpen(RE::CursorMenu::MENU_NAME));
			if (Settings::Get().releaseMouse) {
				for (const auto& menu : Settings::Get().menus) {
					if (ui->IsMenuOpen(menu)) {
						SKSE::log::info("{} was already open", menu);
						SetMenuOpen(menu, true);
					}
				}
			}
		} else {
			SKSE::log::error("UI not available; menu events are not watched");
		}
		if (Settings::Get().releaseMouse) {
			MouseRelease::InstallWindowHook();
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SKSE::log::info("SkyrimMouseRelease loaded");

	Settings::Get().Load();
	if (Settings::Get().releaseMouse) {
		MouseRelease::InstallHooks();
	}

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		SKSE::log::error("Could not register for SKSE messages");
		return false;
	}
	return true;
}
