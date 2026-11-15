#include "MouseRelease.h"

#include "Settings.h"

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <Windows.h>

namespace
{
	// DirectInput cooperative level flags (dinput.h).
	constexpr std::uint32_t kExclusive = 0x1;
	constexpr std::uint32_t kNonExclusive = 0x2;
	constexpr std::uint32_t kForeground = 0x4;

	bool g_released = false;

	REX::W32::IDirectInputDevice8A* GetMouseDevice()
	{
		auto* manager = RE::BSInputDeviceManager::GetSingleton();
		auto* mouse = manager ? manager->GetMouse() : nullptr;
		return mouse ? mouse->dInputDevice : nullptr;
	}

	REX::W32::HWND GetGameWindow()
	{
		auto* main = RE::Main::GetSingleton();
		return main ? main->wnd : nullptr;
	}

	bool SetCooperativeLevel(std::uint32_t a_flags)
	{
		auto* device = GetMouseDevice();
		auto  window = GetGameWindow();
		if (!device || !window) {
			SKSE::log::warn("Mouse device or game window not available (device={}, window={})",
				static_cast<const void*>(device), static_cast<const void*>(window));
			return false;
		}

		// The cooperative level can only change while the device is unacquired.
		const auto unacquire = device->Unacquire();
		const auto cooperative = device->SetCooperativeLevel(window, a_flags);
		const auto acquire = device->Acquire();

		SKSE::log::info("SetCooperativeLevel({:#x}): Unacquire={:#010x}, SetCooperativeLevel={:#010x}, Acquire={:#010x}",
			a_flags,
			static_cast<std::uint32_t>(unacquire),
			static_cast<std::uint32_t>(cooperative),
			static_cast<std::uint32_t>(acquire));

		return cooperative >= 0;
	}
}

namespace MouseRelease
{
	void Release()
	{
		if (g_released) {
			return;
		}
		if (SetCooperativeLevel(kNonExclusive | kForeground)) {
			::ClipCursor(nullptr);
			g_released = true;
			SKSE::log::info("Mouse released");
		}
	}

	void Restore()
	{
		if (!g_released) {
			return;
		}
		const auto flags = Settings::Get().restoreFlags;
		if (!SetCooperativeLevel(flags)) {
			SKSE::log::error("Restoring the mouse failed; trying exclusive + foreground");
			SetCooperativeLevel(kExclusive | kForeground);
		}
		g_released = false;
		SKSE::log::info("Mouse restored");
	}
}
