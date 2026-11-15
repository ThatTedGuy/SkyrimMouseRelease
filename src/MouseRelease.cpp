#include "MouseRelease.h"

#include "Settings.h"

#include <Windows.h>

#include <atomic>

namespace
{
	// DirectInput cooperative level flags (dinput.h).
	constexpr std::uint32_t kExclusive = 0x1;
	constexpr std::uint32_t kNonExclusive = 0x2;
	constexpr std::uint32_t kForeground = 0x4;

	std::atomic_bool g_wanted{ false };        // a watched menu is open
	std::atomic_bool g_released{ false };      // shared mode is active
	std::atomic_bool g_needsReapply{ false };  // focus or device reset since the last release

	// Main thread only.
	REX::W32::IDirectInputDevice8A* g_releasedDevice = nullptr;
	bool                            g_cursorInside = true;
	bool                            g_loggedCursorSpace = false;
	bool                            g_clipLogged = false;

	HWND    g_window = nullptr;
	WNDPROC g_originalWndProc = nullptr;

	HWND GameWindow()
	{
		auto* main = RE::Main::GetSingleton();
		return main ? reinterpret_cast<HWND>(main->wnd) : nullptr;
	}

	bool ApplyCooperativeLevel(RE::BSWin32MouseDevice* a_mouse, std::uint32_t a_flags, std::string_view a_reason)
	{
		auto* device = a_mouse ? a_mouse->dInputDevice : nullptr;
		auto  window = GameWindow();
		if (!device || !window) {
			SKSE::log::warn("{}: mouse device or game window not available", a_reason);
			return false;
		}

		// The cooperative level can only change while the device is unacquired.
		const auto unacquire = device->Unacquire();
		const auto cooperative = device->SetCooperativeLevel(reinterpret_cast<REX::W32::HWND>(window), a_flags);
		const auto acquire = device->Acquire();

		SKSE::log::info("{}: SetCooperativeLevel({:#x}): Unacquire={:#010x}, SetCooperativeLevel={:#010x}, Acquire={:#010x}",
			a_reason, a_flags,
			static_cast<std::uint32_t>(unacquire),
			static_cast<std::uint32_t>(cooperative),
			static_cast<std::uint32_t>(acquire));

		return cooperative >= 0;
	}

	bool IsCursorClipped()
	{
		RECT clip{};
		if (!::GetClipCursor(&clip)) {
			return false;
		}
		const RECT desktop{
			::GetSystemMetrics(SM_XVIRTUALSCREEN),
			::GetSystemMetrics(SM_YVIRTUALSCREEN),
			::GetSystemMetrics(SM_XVIRTUALSCREEN) + ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
			::GetSystemMetrics(SM_YVIRTUALSCREEN) + ::GetSystemMetrics(SM_CYVIRTUALSCREEN)
		};
		return clip.left > desktop.left || clip.top > desktop.top || clip.right < desktop.right || clip.bottom < desktop.bottom;
	}

	void Release(RE::BSWin32MouseDevice* a_mouse, std::string_view a_reason)
	{
		if (ApplyCooperativeLevel(a_mouse, kNonExclusive | kForeground, a_reason)) {
			::ClipCursor(nullptr);
			g_releasedDevice = a_mouse->dInputDevice;
			g_released = true;
			g_clipLogged = false;
		}
	}

	void Restore(RE::BSWin32MouseDevice* a_mouse)
	{
		if (!ApplyCooperativeLevel(a_mouse, Settings::Get().restoreFlags, "Restore")) {
			SKSE::log::error("Restore failed; trying exclusive + foreground");
			ApplyCooperativeLevel(a_mouse, kExclusive | kForeground, "Restore fallback");
		}
		g_releasedDevice = nullptr;
		g_released = false;
		g_cursorInside = true;
	}

	// Puts Skyrim's menu cursor where the Windows cursor is, while the Windows cursor is over the game.
	void SyncMenuCursor()
	{
		POINT point{};
		RECT  client{};
		if (!g_window || !::GetCursorPos(&point) || !::ScreenToClient(g_window, &point) || !::GetClientRect(g_window, &client)) {
			return;
		}

		const bool inside = point.x >= 0 && point.y >= 0 && point.x < client.right && point.y < client.bottom;
		if (inside != g_cursorInside) {
			g_cursorInside = inside;
			SKSE::log::info("Cursor {} the game window", inside ? "entered" : "left");
		}
		if (!inside || client.right <= 0 || client.bottom <= 0) {
			return;
		}

		auto* menuCursor = RE::MenuCursor::GetSingleton();
		if (!menuCursor) {
			return;
		}

		const float width = menuCursor->screenWidthX > 0.0f ? menuCursor->screenWidthX : static_cast<float>(client.right);
		const float height = menuCursor->screenWidthY > 0.0f ? menuCursor->screenWidthY : static_cast<float>(client.bottom);

		if (!g_loggedCursorSpace) {
			g_loggedCursorSpace = true;
			SKSE::log::info("Menu cursor space: screen {}x{}, safe zone {}x{}, window client {}x{}",
				menuCursor->screenWidthX, menuCursor->screenWidthY, menuCursor->safeZoneX, menuCursor->safeZoneY,
				client.right, client.bottom);
		}

		menuCursor->cursorPosX = static_cast<float>(point.x) * width / static_cast<float>(client.right);
		menuCursor->cursorPosY = static_cast<float>(point.y) * height / static_cast<float>(client.bottom);
	}

	// Runs on the main thread after the game reads the mouse each frame.
	void Reconcile(RE::BSWin32MouseDevice* a_mouse)
	{
		const bool wanted = g_wanted.load();
		const bool released = g_released.load();

		if (wanted && !released) {
			Release(a_mouse, "Release");
			return;
		}
		if (!wanted && released) {
			Restore(a_mouse);
			return;
		}
		if (!released) {
			return;
		}

		if (a_mouse->dInputDevice != g_releasedDevice) {
			SKSE::log::info("Mouse device changed while released");
			g_needsReapply = true;
		}
		if (g_needsReapply.exchange(false)) {
			Release(a_mouse, "Reapply");
		}
		if (IsCursorClipped()) {
			if (!g_clipLogged) {
				g_clipLogged = true;
				SKSE::log::info("Cursor was clipped while released; removing the clip");
			}
			::ClipCursor(nullptr);
		}
		SyncMenuCursor();
	}

	struct PollHook
	{
		static void Thunk(RE::BSWin32MouseDevice* a_this, float a_timeDelta)
		{
			func(a_this, a_timeDelta);
			Reconcile(a_this);
		}
		static inline REL::Relocation<decltype(Thunk)> func;
	};

	struct InitializeHook
	{
		static void Thunk(RE::BSWin32MouseDevice* a_this)
		{
			func(a_this);
			SKSE::log::info("Mouse device initialized{}", g_released ? " while released" : "");
			if (g_released) {
				g_needsReapply = true;
			}
		}
		static inline REL::Relocation<decltype(Thunk)> func;
	};

	struct ReinitializeHook
	{
		static void Thunk(RE::BSWin32MouseDevice* a_this)
		{
			func(a_this);
			SKSE::log::info("Mouse device reinitialized{}", g_released ? " while released" : "");
			if (g_released) {
				g_needsReapply = true;
			}
		}
		static inline REL::Relocation<decltype(Thunk)> func;
	};

	LRESULT CALLBACK WindowProc(HWND a_window, UINT a_message, WPARAM a_wParam, LPARAM a_lParam)
	{
		switch (a_message) {
		case WM_ACTIVATEAPP:
			SKSE::log::info("Game window {}", a_wParam ? "activated" : "deactivated");
			if (a_wParam && g_wanted) {
				g_needsReapply = true;
			}
			break;
		case WM_SETCURSOR:
			// Over the game, only Skyrim's menu cursor should show.
			if (g_released && LOWORD(a_lParam) == HTCLIENT) {
				::SetCursor(nullptr);
				return TRUE;
			}
			break;
		default:
			break;
		}
		return ::CallWindowProcW(g_originalWndProc, a_window, a_message, a_wParam, a_lParam);
	}
}

namespace MouseRelease
{
	void InstallHooks()
	{
		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_BSWin32MouseDevice[0] };
		InitializeHook::func = vtable.write_vfunc(0x1, InitializeHook::Thunk);
		PollHook::func = vtable.write_vfunc(0x2, PollHook::Thunk);
		ReinitializeHook::func = vtable.write_vfunc(0x9, ReinitializeHook::Thunk);
		SKSE::log::info("Mouse device hooks installed");
	}

	void InstallWindowHook()
	{
		g_window = GameWindow();
		if (!g_window) {
			SKSE::log::error("Game window not found; focus changes are not tracked");
			return;
		}
		g_originalWndProc = reinterpret_cast<WNDPROC>(
			::SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc)));
		if (!g_originalWndProc) {
			SKSE::log::error("Could not hook the game window (error {})", ::GetLastError());
			return;
		}
		SKSE::log::info("Game window hooked");
	}

	void SetWanted(bool a_wanted)
	{
		g_wanted = a_wanted;
	}
}
