#include "MouseRelease.h"
#include "Settings.h"

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

namespace
{
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

			if (watched && settings.releaseMouse) {
				// Menu events can arrive off the main thread; the input device is polled on it.
				SKSE::GetTaskInterface()->AddTask([opening]() {
					if (opening) {
						MouseRelease::Release();
					} else {
						MouseRelease::Restore();
					}
				});
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
		} else {
			SKSE::log::error("UI not available; menu events are not watched");
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SKSE::log::info("SkyrimMouseRelease loaded");

	Settings::Get().Load();

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		SKSE::log::error("Could not register for SKSE messages");
		return false;
	}
	return true;
}
