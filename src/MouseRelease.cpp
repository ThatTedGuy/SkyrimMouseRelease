#include "MouseRelease.h"

#include "Settings.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>

namespace
{
	// DirectInput cooperative level flags (dinput.h).
	constexpr std::uint32_t kExclusive = 0x1;
	constexpr std::uint32_t kNonExclusive = 0x2;
	constexpr std::uint32_t kForeground = 0x4;

	std::atomic_bool g_wanted{ false };        // a watched menu is open
	std::atomic_bool g_released{ false };      // shared mode is active
	std::atomic_bool g_needsReapply{ false };  // focus or device reset since the last release
	std::atomic_bool g_hideCursorOnce{ false };  // next WM_SETCURSOR hides the cursor even if not released
	std::atomic_bool g_gameCursorShown{ false };  // Skyrim's "Cursor Menu" is open and draws a cursor
	std::atomic_bool g_retryNow{ false };         // focus came back: retry a failed switch at once

	using Clock = std::chrono::steady_clock;

	// Used only from the Poll hook, which runs once per frame: on the main thread while the pause menu
	// is open, but on a worker thread during play (the log shows Restore there).
	REX::W32::IDirectInputDevice8A* g_releasedDevice = nullptr;
	bool                            g_cursorInside = true;
	bool                            g_loggedCursorSpace = false;
	bool                            g_clipLogged = false;
	bool                            g_loggedZeroed = false;
	Clock::time_point               g_nextTry{};       // after a failed switch, wait until then
	int                             g_failures = 0;    // failed switches in a row
	int                             g_blockedInputs = 0;  // clicks and scrolls blocked outside the window this time

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

	bool IsCursorOverGame()
	{
		POINT point{};
		return g_window && ::GetCursorPos(&point) && ::WindowFromPoint(point) == g_window;
	}

	// The Windows cursor in game-window client coordinates.
	bool GetCursorInClient(POINT& a_point, RECT& a_client)
	{
		return g_window && ::GetCursorPos(&a_point) && ::ScreenToClient(g_window, &a_point) &&
		       ::GetClientRect(g_window, &a_client) && a_client.right > 0 && a_client.bottom > 0;
	}

	bool IsInside(const POINT& a_point, const RECT& a_client)
	{
		return a_point.x >= 0 && a_point.y >= 0 && a_point.x < a_client.right && a_point.y < a_client.bottom;
	}

	void LogCursorPosition(std::string_view a_when)
	{
		POINT point{};
		RECT  client{};
		if (!GetCursorInClient(point, client)) {
			return;
		}
		auto* menuCursor = RE::MenuCursor::GetSingleton();
		SKSE::log::debug("{}: Windows cursor at {}, {} in a {}x{} window; menu cursor at {}, {}",
			a_when, point.x, point.y, client.right, client.bottom,
			menuCursor ? menuCursor->cursorPosX : -1.0f, menuCursor ? menuCursor->cursorPosY : -1.0f);
	}

	// Exclusive mode leaves the Windows cursor wherever it is. If that is another screen, it stays
	// visible there during play, so bring it to the edge spot where Skyrim's cursor was pinned.
	void BringCursorIntoWindow()
	{
		POINT point{};
		RECT  client{};
		if (!GetCursorInClient(point, client) || IsInside(point, client)) {
			return;
		}
		point.x = std::clamp(point.x, 0L, client.right - 1);
		point.y = std::clamp(point.y, 0L, client.bottom - 1);
		::ClientToScreen(g_window, &point);
		::SetCursorPos(point.x, point.y);
		SKSE::log::debug("Moved the Windows cursor back into the game window");
	}

	// Asks the game window to set the cursor shape again; the WM_SETCURSOR handler decides which.
	bool RefreshCursorShape()
	{
		return IsCursorOverGame() &&
		       ::PostMessageW(g_window, WM_SETCURSOR, reinterpret_cast<WPARAM>(g_window), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
	}

	// The menu keeps its cursor where it was when it last closed, but during play something centers
	// the Windows cursor. The menu only redraws its cursor on movement, so without this the first
	// move after opening jumps it to the center. Moving the Windows cursor to the menu's spot keeps
	// the game's start position.
	void MoveCursorToMenuCursor()
	{
		POINT point{};
		RECT  client{};
		auto* menuCursor = RE::MenuCursor::GetSingleton();
		if (!menuCursor || menuCursor->screenWidthX <= 0.0f || menuCursor->screenWidthY <= 0.0f || !GetCursorInClient(point, client)) {
			return;
		}
		POINT target{
			std::clamp(static_cast<LONG>(menuCursor->cursorPosX * static_cast<float>(client.right) / menuCursor->screenWidthX), 0L, client.right - 1),
			std::clamp(static_cast<LONG>(menuCursor->cursorPosY * static_cast<float>(client.bottom) / menuCursor->screenWidthY), 0L, client.bottom - 1)
		};
		::ClientToScreen(g_window, &target);
		::SetCursorPos(target.x, target.y);
	}

	bool Release(RE::BSWin32MouseDevice* a_mouse, std::string_view a_reason)
	{
		const bool opening = a_reason == "Release";
		if (opening) {
			LogCursorPosition("Menu opened");
		}
		if (!ApplyCooperativeLevel(a_mouse, kNonExclusive | kForeground, a_reason)) {
			return false;
		}
		if (opening) {
			MoveCursorToMenuCursor();
		}
		::ClipCursor(nullptr);
		g_releasedDevice = a_mouse->dInputDevice;
		g_released = true;
		g_clipLogged = false;
		g_blockedInputs = 0;
		// Shared mode shows the Windows cursor in its last shape (often the loading wheel) until the
		// mouse moves. Set the right shape now; the message runs on the window's own thread.
		RefreshCursorShape();
		return true;
	}

	bool Restore(RE::BSWin32MouseDevice* a_mouse)
	{
		LogCursorPosition("Menu closed");
		BringCursorIntoWindow();
		// The cursor may still carry a shape from another screen or a loading screen, and with the game
		// holding the mouse no message will reset it, so set no shape (Skyrim's normal state in play).
		g_hideCursorOnce = true;  // set before posting: the window thread may handle it at once
		if (!RefreshCursorShape()) {
			g_hideCursorOnce = false;
		}
		if (!ApplyCooperativeLevel(a_mouse, Settings::Get().restoreFlags, "Restore") &&
			!ApplyCooperativeLevel(a_mouse, kExclusive | kForeground, "Restore fallback")) {
			return false;  // still released; Reconcile tries again later
		}
		if (g_blockedInputs > 0) {
			SKSE::log::info("Blocked {} clicks or scrolls outside the game window", g_blockedInputs);
		}
		g_releasedDevice = nullptr;
		g_released = false;
		g_cursorInside = true;
		return true;
	}

	// After a failed switch, wait before trying again (1, 2, 4, then every 8 seconds), so a device that
	// keeps refusing doesn't move the cursor and fill the log every frame. Focus coming back resets it.
	void NoteSwitchResult(bool a_ok, std::string_view a_what)
	{
		if (a_ok) {
			g_failures = 0;
			g_nextTry = {};
			return;
		}
		++g_failures;
		const auto delay = std::chrono::seconds(1 << (std::min)(g_failures - 1, 3));  // parentheses: windows.h defines min
		g_nextTry = Clock::now() + delay;
		SKSE::log::warn("{} failed ({} in a row); trying again in {} s", a_what, g_failures, delay.count());
	}

	// Puts Skyrim's menu cursor where the Windows cursor is. Outside the window it sits on the nearest
	// edge, away from the menu's buttons, so a click meant for another window presses nothing.
	void SyncMenuCursor()
	{
		POINT point{};
		RECT  client{};
		if (!GetCursorInClient(point, client)) {
			return;
		}

		const bool inside = IsInside(point, client);
		if (inside != g_cursorInside) {
			g_cursorInside = inside;
			SKSE::log::debug("Cursor {} the game window", inside ? "entered" : "left");
		}
		point.x = std::clamp(point.x, 0L, client.right - 1);
		point.y = std::clamp(point.y, 0L, client.bottom - 1);

		auto* menuCursor = RE::MenuCursor::GetSingleton();
		if (!menuCursor) {
			return;
		}

		const float width = menuCursor->screenWidthX > 0.0f ? menuCursor->screenWidthX : static_cast<float>(client.right);
		const float height = menuCursor->screenWidthY > 0.0f ? menuCursor->screenWidthY : static_cast<float>(client.bottom);

		if (!g_loggedCursorSpace) {
			g_loggedCursorSpace = true;
			SKSE::log::debug("Menu cursor space: screen {}x{}, safe zone {}x{}, window client {}x{}",
				menuCursor->screenWidthX, menuCursor->screenWidthY, menuCursor->safeZoneX, menuCursor->safeZoneY,
				client.right, client.bottom);
		}

		menuCursor->cursorPosX = static_cast<float>(point.x) * width / static_cast<float>(client.right);
		menuCursor->cursorPosY = static_cast<float>(point.y) * height / static_cast<float>(client.bottom);
	}

	// Poll has just queued this frame's mouse movement. Left alone, the menu adds it on top of the
	// position set above and overshoots by one frame of motion. Only runs while released, when the
	// position comes from the Windows cursor instead.
	void ZeroQueuedMouseMovement()
	{
		auto* queue = RE::BSInputEventQueue::GetSingleton();
		if (!queue) {
			return;
		}
		for (auto* event = queue->GetQueueHead(); event; event = event->next) {
			if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kMouseMove) {
				continue;
			}
			auto* move = static_cast<RE::MouseMoveEvent*>(event);
			if ((move->mouseInputX != 0 || move->mouseInputY != 0) && !g_loggedZeroed) {
				g_loggedZeroed = true;
				SKSE::log::debug("Zeroing the game's mouse movement while released (first: {}, {})", move->mouseInputX, move->mouseInputY);
			}
			move->mouseInputX = 0;
			move->mouseInputY = 0;
		}
	}

	// While the cursor is on another screen and Skyrim is still the active window, shared-mode
	// DirectInput still hands Skyrim the mouse buttons and wheel. Take presses and wheel turns out of
	// the queue then, so a click or scroll meant for another window does nothing in the menu.
	// Releases are kept, so a drag that ends outside the window still ends in the menu.
	void BlockMouseInputOutside()
	{
		if (g_cursorInside) {
			return;
		}
		auto* queue = RE::BSInputEventQueue::GetSingleton();
		if (!queue) {
			return;
		}
		auto&            head = queue->GetQueueHead();
		auto&            tail = queue->GetQueueTail();
		RE::InputEvent*  previous = nullptr;
		RE::InputEvent** link = &head;
		while (*link) {
			auto* event = *link;
			bool  block = false;
			if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton && event->GetDevice() == RE::INPUT_DEVICE::kMouse) {
				const auto* button = static_cast<RE::ButtonEvent*>(event);
				const auto  id = button->GetIDCode();
				const bool  wheel = id == RE::BSWin32MouseDevice::Keys::kWheelUp || id == RE::BSWin32MouseDevice::Keys::kWheelDown;
				block = wheel || button->IsDown();
			}
			if (!block) {
				previous = event;
				link = &event->next;
				continue;
			}
			*link = event->next;  // unlink; the event object belongs to the queue's own pool
			if (tail == event) {
				tail = previous;
			}
			if (g_blockedInputs++ == 0) {
				SKSE::log::info("Blocking clicks and scrolls while the cursor is outside the game window");
			}
		}
	}

	// Runs from the Poll hook after the game reads the mouse each frame.
	void Reconcile(RE::BSWin32MouseDevice* a_mouse)
	{
		const bool wanted = g_wanted.load();
		const bool released = g_released.load();

		if (g_retryNow.exchange(false)) {
			g_nextTry = {};
		}
		const bool mayTry = Clock::now() >= g_nextTry;

		if (wanted && !released) {
			if (mayTry) {
				NoteSwitchResult(Release(a_mouse, "Release"), "Freeing the mouse");
			}
			return;
		}
		if (!wanted && released) {
			if (mayTry) {
				NoteSwitchResult(Restore(a_mouse), "Giving the mouse back to the game");
			}
			return;
		}
		if (!released) {
			return;
		}

		if (a_mouse->dInputDevice != g_releasedDevice) {
			SKSE::log::info("Mouse device changed while released");
			g_releasedDevice = a_mouse->dInputDevice;
			g_needsReapply = true;
		}
		if (mayTry && g_needsReapply.exchange(false)) {
			const bool ok = Release(a_mouse, "Reapply");
			if (!ok) {
				g_needsReapply = true;
			}
			NoteSwitchResult(ok, "Freeing the mouse again");
		}
		if (IsCursorClipped()) {
			if (!g_clipLogged) {
				g_clipLogged = true;
				SKSE::log::info("Cursor was clipped while released; removing the clip");
			}
			::ClipCursor(nullptr);
		}
		SyncMenuCursor();
		ZeroQueuedMouseMovement();
		BlockMouseInputOutside();
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
			if (g_released) {
				SKSE::log::info("Mouse device initialized while released");
				g_needsReapply = true;
			} else {
				SKSE::log::debug("Mouse device initialized");
			}
		}
		static inline REL::Relocation<decltype(Thunk)> func;
	};

	struct ReinitializeHook
	{
		static void Thunk(RE::BSWin32MouseDevice* a_this)
		{
			func(a_this);
			if (g_released) {
				SKSE::log::info("Mouse device reinitialized while released");
				g_needsReapply = true;
			} else {
				SKSE::log::debug("Mouse device reinitialized");
			}
		}
		static inline REL::Relocation<decltype(Thunk)> func;
	};

	LRESULT CALLBACK WindowProc(HWND a_window, UINT a_message, WPARAM a_wParam, LPARAM a_lParam)
	{
		switch (a_message) {
		case WM_ACTIVATEAPP:
			SKSE::log::debug("Game window {}", a_wParam ? "activated" : "deactivated");
			if (a_wParam) {
				g_retryNow = true;
				if (g_wanted) {
					g_needsReapply = true;
				}
			}
			break;
		case WM_SETCURSOR:
			{
				// Over the game, only one cursor should show: Skyrim's when it draws one, otherwise
				// (loading screens) the Windows arrow. The one-shot flag hides it when the game takes the
				// mouse back.
				const bool hideOnce = g_hideCursorOnce.exchange(false);
				if (LOWORD(a_lParam) == HTCLIENT) {
					if (hideOnce || (g_released && g_gameCursorShown)) {
						::SetCursor(nullptr);
						return TRUE;
					}
					if (g_released) {
						::SetCursor(::LoadCursorA(nullptr, IDC_ARROW));
						return TRUE;
					}
				}
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

	void SetGameCursorShown(bool a_shown)
	{
		g_gameCursorShown = a_shown;
		if (g_released) {
			RefreshCursorShape();
		}
	}
}
