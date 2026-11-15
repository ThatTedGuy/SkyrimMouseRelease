// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ThatTedGuy

// Frees the mouse while a watched menu is open.
//
// Skyrim reads the mouse through DirectInput in exclusive mode, which hides the Windows cursor and
// keeps it from leaving the game. While a watched menu is open, the plugin switches the game's
// mouse device to shared mode, so the Windows cursor can move to other screens and click there.
//
// Shared mode alone would show two cursors that drift apart, so every frame while the mouse is free:
//  - Skyrim's menu cursor is placed where the Windows cursor is (or on the nearest window edge when
//    the Windows cursor is on another screen), and the game's own mouse movement is set to zero so
//    the menu doesn't add it on top (except while dragging with the left button, which turns items).
//    The zeroing happens as the game hands input to its handlers (see MovementFilter), not when the
//    mouse is read, so mods that read the movement before that hand-out (Wheeler) still see it;
//  - clicks and wheel turns are dropped while the cursor is outside the window, so input meant for
//    another program does nothing in the menu;
//  - over the game window, only one cursor shows (see WindowProc).
//
// All switching happens in a hook on the mouse device's Poll, which the game calls once per frame
// (on the main thread while paused, on a worker thread during play). Menu events and window
// messages arrive on other threads. They set flags that the Poll hook acts on, and the window
// thread also sets the cursor shape and releases the play lock; it never switches the device.

#include "MouseRelease.h"

#include "Settings.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>

namespace
{
	using Clock = std::chrono::steady_clock;

	// DirectInput cooperative level flags (dinput.h).
	constexpr std::uint32_t kExclusive = 0x1;
	constexpr std::uint32_t kNonExclusive = 0x2;
	constexpr std::uint32_t kForeground = 0x4;
	constexpr std::uint32_t kGameDefaultFlags = kExclusive | kForeground;
	constexpr std::uint32_t kFreeFlags = kNonExclusive | kForeground;

	// BSWin32MouseDevice virtual function slots.
	constexpr std::size_t kInitializeSlot = 0x1;
	constexpr std::size_t kPollSlot = 0x2;
	constexpr std::size_t kReinitializeSlot = 0x9;

	// Set from any thread.
	std::atomic_bool g_wantFree{ false };         // a watched menu is open
	std::atomic_bool g_isFree{ false };           // the mouse device is in shared mode
	std::atomic_bool g_refreeNeeded{ false };     // the game reset the device or regained focus while free
	std::atomic_bool g_retryNow{ false };         // focus came back: retry a failed switch without waiting
	std::atomic_bool g_gameCursorShown{ false };  // Skyrim draws its own cursor ("Cursor Menu" is open)
	std::atomic_bool g_lockedInPlay{ false };     // we confined the cursor to the window for play

	// Posted to the game window after the mouse goes back to the game: hide the Windows cursor.
	// Registered with Windows, so no other message the game window gets can have the same number.
	UINT g_hideCursorMessage = 0;

	// Used only from the Poll hook (one call per frame, so never concurrently).
	REX::W32::IDirectInputDevice8A* g_freeDevice = nullptr;  // the device we switched, to notice a new one
	bool                            g_cursorInside = true;
	bool                            g_dragging = false;  // left button pressed inside the window, not yet released
	bool                            g_clipLogged = false;
	int                             g_blockedInputs = 0;  // clicks and scrolls dropped during this menu visit
	int                             g_failures = 0;       // failed switches in a row
	Clock::time_point               g_nextTry{};          // after a failure, don't try again before this
	Clock::time_point               g_returnedAt{};       // when the mouse last went back to the game
	Clock::time_point               g_nextCursorCheck{};  // throttles LogVisibleCursor
	bool                            g_visibleCursorLogged = false;

	// Set once when the window is hooked.
	HWND    g_window = nullptr;
	WNDPROC g_originalWndProc = nullptr;

	// ---------------------------------------------------------------------------------------------
	// Window and cursor helpers
	// ---------------------------------------------------------------------------------------------

	HWND GameWindow()
	{
		auto* main = RE::Main::GetSingleton();
		return main ? reinterpret_cast<HWND>(main->wnd) : nullptr;
	}

	// The Windows cursor in game-window client coordinates, and the client area's size.
	bool GetCursorInClient(POINT& a_point, RECT& a_client)
	{
		return g_window && ::GetCursorPos(&a_point) && ::ScreenToClient(g_window, &a_point) &&
		       ::GetClientRect(g_window, &a_client) && a_client.right > 0 && a_client.bottom > 0;
	}

	bool IsInside(const POINT& a_point, const RECT& a_client)
	{
		return a_point.x >= 0 && a_point.y >= 0 && a_point.x < a_client.right && a_point.y < a_client.bottom;
	}

	POINT ClampToClient(POINT a_point, const RECT& a_client)
	{
		a_point.x = std::clamp(a_point.x, 0L, a_client.right - 1);
		a_point.y = std::clamp(a_point.y, 0L, a_client.bottom - 1);
		return a_point;
	}

	void SetCursorToClientPoint(POINT a_point)
	{
		::ClientToScreen(g_window, &a_point);
		::SetCursorPos(a_point.x, a_point.y);
	}

	bool IsCursorOverGame()
	{
		POINT point{};
		return g_window && ::GetCursorPos(&point) && ::WindowFromPoint(point) == g_window;
	}

	// Some other program or mod has confined the cursor to part of the desktop.
	bool IsCursorClipped()
	{
		RECT clip{};
		if (!::GetClipCursor(&clip)) {
			return false;
		}
		const LONG left = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
		const LONG top = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
		const LONG right = left + ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
		const LONG bottom = top + ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
		return clip.left > left || clip.top > top || clip.right < right || clip.bottom < bottom;
	}

	// Has the game window pick the cursor shape again now, instead of on the next mouse move.
	// WindowProc decides which shape. Returns false if the cursor isn't over the game.
	bool RefreshCursorShape()
	{
		return IsCursorOverGame() &&
		       ::PostMessageW(g_window, WM_SETCURSOR, reinterpret_cast<WPARAM>(g_window), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
	}

	// Hides the Windows cursor, as during normal play. Its own message rather than a WM_SETCURSOR,
	// so a real mouse message can't be handled in its place.
	void HideCursor()
	{
		if (g_hideCursorMessage != 0 && IsCursorOverGame()) {
			::PostMessageW(g_window, g_hideCursorMessage, 0, 0);
		}
	}

	void LogCursorPosition(std::string_view a_when)
	{
		POINT point{};
		RECT  client{};
		if (!GetCursorInClient(point, client)) {
			return;
		}
		const auto* menuCursor = RE::MenuCursor::GetSingleton();
		SKSE::log::debug("{}: Windows cursor at {}, {} in a {}x{} window; menu cursor at {}, {}",
			a_when, point.x, point.y, client.right, client.bottom,
			menuCursor ? menuCursor->cursorPosX : -1.0f, menuCursor ? menuCursor->cursorPosY : -1.0f);
	}

	// ---------------------------------------------------------------------------------------------
	// Switching the mouse between the game and Windows
	// ---------------------------------------------------------------------------------------------

	bool SetCooperativeLevel(RE::BSWin32MouseDevice* a_mouse, std::uint32_t a_flags, std::string_view a_label)
	{
		auto* device = a_mouse ? a_mouse->dInputDevice : nullptr;
		auto  window = GameWindow();
		if (!device || !window) {
			SKSE::log::warn("{}: mouse device or game window not available", a_label);
			return false;
		}

		// The level can only change while the device is unacquired. Acquire may fail if the window
		// isn't in front; the game acquires the device again itself when it gets focus.
		const auto unacquire = device->Unacquire();
		const auto cooperative = device->SetCooperativeLevel(reinterpret_cast<REX::W32::HWND>(window), a_flags);
		const auto acquire = device->Acquire();

		SKSE::log::info("{}: SetCooperativeLevel({:#x}) Unacquire={:#010x} SetCooperativeLevel={:#010x} Acquire={:#010x}",
			a_label, a_flags,
			static_cast<std::uint32_t>(unacquire),
			static_cast<std::uint32_t>(cooperative),
			static_cast<std::uint32_t>(acquire));

		return cooperative >= 0;
	}

	// The menu remembers where its cursor was when it last closed, but during play the Windows
	// cursor gets moved to the screen center. Start the Windows cursor at the menu's spot, or the
	// menu cursor would jump to the center on the first move.
	void MoveWindowsCursorToMenuCursor()
	{
		POINT point{};
		RECT  client{};
		const auto* menuCursor = RE::MenuCursor::GetSingleton();
		if (!menuCursor || menuCursor->screenWidthX <= 0.0f || menuCursor->screenWidthY <= 0.0f || !GetCursorInClient(point, client)) {
			return;
		}
		const POINT target{
			static_cast<LONG>(menuCursor->cursorPosX * static_cast<float>(client.right) / menuCursor->screenWidthX),
			static_cast<LONG>(menuCursor->cursorPosY * static_cast<float>(client.bottom) / menuCursor->screenWidthY)
		};
		SetCursorToClientPoint(ClampToClient(target, client));
	}

	// Exclusive mode leaves the Windows cursor where it is. If it's on another screen, bring it to the
	// nearest window edge, where Skyrim's cursor already sits, so it isn't left visible over there.
	void BringCursorIntoWindow()
	{
		POINT point{};
		RECT  client{};
		if (!GetCursorInClient(point, client) || IsInside(point, client)) {
			return;
		}
		SetCursorToClientPoint(ClampToClient(point, client));
		SKSE::log::debug("Moved the Windows cursor back into the game window");
	}

	// a_menuOpened is false when the game reset the device or regained focus while the mouse was
	// already free; then the cursor stays where it is.
	bool FreeMouse(RE::BSWin32MouseDevice* a_mouse, bool a_menuOpened)
	{
		if (a_menuOpened) {
			LogCursorPosition("Menu opened");
			// This switch covers any earlier reset. Cleared before switching, so a reset during the
			// switch still asks for another one.
			g_refreeNeeded = false;
		}
		if (!SetCooperativeLevel(a_mouse, kFreeFlags, a_menuOpened ? "Free" : "Free again")) {
			return false;
		}
		if (a_menuOpened) {
			MoveWindowsCursorToMenuCursor();
			g_blockedInputs = 0;
		}
		::ClipCursor(nullptr);
		g_lockedInPlay = false;
		g_dragging = false;  // a release may have been lost with the old device or focus
		g_freeDevice = a_mouse->dInputDevice;
		g_isFree = true;
		g_clipLogged = false;
		// The Windows cursor would keep its last shape (often the loading wheel) until it moves.
		RefreshCursorShape();
		return true;
	}

	bool ReturnMouse(RE::BSWin32MouseDevice* a_mouse)
	{
		LogCursorPosition("Menu closed");
		BringCursorIntoWindow();
		g_refreeNeeded = false;  // a reset while free means nothing once the mouse is back with the game

		if (!SetCooperativeLevel(a_mouse, Settings::Get().restoreFlags, "Return") &&
			!SetCooperativeLevel(a_mouse, kGameDefaultFlags, "Return with default flags")) {
			return false;  // still free; retried later
		}
		if (g_blockedInputs > 0) {
			SKSE::log::info("Blocked {} clicks or scrolls outside the game window", g_blockedInputs);
		}
		g_freeDevice = nullptr;
		g_isFree = false;
		g_cursorInside = true;
		g_dragging = false;
		g_returnedAt = Clock::now();

		// The cursor may still carry a shape from another screen or a loading screen, and once the
		// game holds the mouse nothing resets it. Only after g_isFree is cleared: until then, mouse
		// messages handled on the window thread still pick the free-mouse shape.
		HideCursor();
		return true;
	}

	// After a failed switch, wait 1, 2, 4, then 8 seconds between tries, so a device that keeps
	// refusing doesn't move the cursor and fill the log every frame. Focus coming back skips the wait.
	void RecordAttempt(bool a_succeeded, std::string_view a_what)
	{
		if (a_succeeded) {
			g_failures = 0;
			g_nextTry = {};
			return;
		}
		++g_failures;
		const auto delay = std::chrono::seconds(1 << (std::min)(g_failures - 1, 3));  // (std::min): windows.h defines a min macro
		g_nextTry = Clock::now() + delay;
		SKSE::log::warn("{} failed ({} in a row); trying again in {} s", a_what, g_failures, delay.count());
	}

	// ---------------------------------------------------------------------------------------------
	// Per-frame work while the mouse is free
	// ---------------------------------------------------------------------------------------------

	// Puts Skyrim's menu cursor where the Windows cursor is. Outside the window it sits on the
	// nearest edge, away from the menu's buttons.
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

		auto* menuCursor = RE::MenuCursor::GetSingleton();
		if (!menuCursor) {
			return;
		}
		// The menu works in its own coordinate space (screenWidthX/Y), not window pixels.
		const float width = menuCursor->screenWidthX > 0.0f ? menuCursor->screenWidthX : static_cast<float>(client.right);
		const float height = menuCursor->screenWidthY > 0.0f ? menuCursor->screenWidthY : static_cast<float>(client.bottom);
		const POINT edge = ClampToClient(point, client);
		menuCursor->cursorPosX = static_cast<float>(edge.x) * width / static_cast<float>(client.right);
		menuCursor->cursorPosY = static_cast<float>(edge.y) * height / static_cast<float>(client.bottom);
	}

	bool IsMouseClickOrWheel(const RE::InputEvent& a_event)
	{
		if (a_event.GetEventType() != RE::INPUT_EVENT_TYPE::kButton || a_event.GetDevice() != RE::INPUT_DEVICE::kMouse) {
			return false;
		}
		const auto& button = static_cast<const RE::ButtonEvent&>(a_event);
		const auto  id = button.GetIDCode();
		const bool  wheel = id == RE::BSWin32MouseDevice::Keys::kWheelUp || id == RE::BSWin32MouseDevice::Keys::kWheelDown;
		// Button releases pass, so a drag that ends outside the window still ends in the menu.
		return wheel || button.IsDown();
	}

	bool IsLeftButton(const RE::InputEvent& a_event)
	{
		return a_event.GetEventType() == RE::INPUT_EVENT_TYPE::kButton && a_event.GetDevice() == RE::INPUT_DEVICE::kMouse &&
		       static_cast<const RE::ButtonEvent&>(a_event).GetIDCode() == RE::BSWin32MouseDevice::Keys::kLeftButton;
	}

	// Poll has just queued this frame's input events. While the cursor is outside the window, clicks
	// and wheel turns are unlinked from the queue: Skyrim still receives them while it is the active
	// window, but they were meant for whatever is under the cursor on the other screen.
	void FilterQueuedMouseInput()
	{
		auto* queue = RE::BSInputEventQueue::GetSingleton();
		if (!queue) {
			return;
		}

		auto&            tail = queue->GetQueueTail();
		RE::InputEvent*  previous = nullptr;
		RE::InputEvent** link = &queue->GetQueueHead();
		while (auto* event = *link) {
			if (!g_cursorInside && IsMouseClickOrWheel(*event)) {
				*link = event->next;  // the event object stays in the queue's own pool
				if (tail == event) {
					tail = previous;
				}
				if (g_blockedInputs++ == 0) {
					SKSE::log::info("Blocking clicks and scrolls while the cursor is outside the game window");
				}
				continue;
			}

			previous = event;
			link = &event->next;
		}
	}

	// The game hands each frame's input events to its handlers right after Poll. This handler goes
	// first in that list and sets mouse movement to zero while the mouse is free: the menu would add
	// it to the position SyncMenuCursor set and overshoot by a frame of motion, which shows as
	// jitter. Except during a left-button drag that started in the window: vanilla menus turn the
	// item, the character (character creator) and the skills sky from that movement.
	// Doing this here rather than in Poll matters for mods that read the movement just before the
	// hand-out: Wheeler aims its slots with it, and would otherwise only ever see zero.
	class MovementFilter final : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
		{
			if (!a_events || !g_isFree) {
				return RE::BSEventNotifyControl::kContinue;
			}
			for (auto* event = *a_events; event; event = event->next) {
				if (IsLeftButton(*event)) {
					const auto& button = static_cast<const RE::ButtonEvent&>(*event);
					if (button.IsDown() && g_cursorInside) {
						g_dragging = true;
					} else if (!button.IsPressed()) {
						g_dragging = false;
					}
				}
				if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove && !g_dragging) {
					auto* move = static_cast<RE::MouseMoveEvent*>(event);
					move->mouseInputX = 0;
					move->mouseInputY = 0;
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	// During play the Windows cursor is present but has no shape. If it shows over the active game
	// window, log that once, with how long after the last return, so a bug report says when and what.
	void LogVisibleCursor()
	{
		const auto now = Clock::now();
		if (now < g_nextCursorCheck) {
			return;
		}
		g_nextCursorCheck = now + std::chrono::milliseconds(250);

		CURSORINFO info{ sizeof(CURSORINFO) };
		const bool visible = ::GetCursorInfo(&info) && (info.flags & CURSOR_SHOWING) && info.hCursor &&
		                     ::GetForegroundWindow() == g_window && IsCursorOverGame();
		if (visible && !g_visibleCursorLogged) {
			const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_returnedAt);
			SKSE::log::warn("Windows cursor showing over the game during play (shape {:#x}), {} ms after the mouse went back to the game",
				reinterpret_cast<std::uintptr_t>(info.hCursor), since.count());
		} else if (!visible && g_visibleCursorLogged) {
			SKSE::log::info("Windows cursor hidden again");
		}
		g_visibleCursorLogged = visible;
	}

	// During play the Windows cursor is hidden but still follows the mouse. Some runtimes (vanilla
	// 1.5.97) don't hold it anywhere, so it can drift onto another screen, where Windows sends it the
	// wheel turns. Confine it to the game window while the game is in front. FreeMouse and focus loss
	// (WindowProc) release it.
	void LockCursorInPlay()
	{
		if (!Settings::Get().lockCursorInPlay || !g_window || ::GetForegroundWindow() != g_window) {
			return;
		}
		RECT client{};
		if (!::GetClientRect(g_window, &client) || client.right <= 0 || client.bottom <= 0) {
			return;
		}
		POINT topLeft{ 0, 0 };
		POINT bottomRight{ client.right, client.bottom };
		::ClientToScreen(g_window, &topLeft);
		::ClientToScreen(g_window, &bottomRight);
		const RECT target{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };

		RECT current{};
		if (::GetClipCursor(&current) && ::EqualRect(&current, &target)) {
			return;
		}
		::ClipCursor(&target);
		if (!g_lockedInPlay.exchange(true)) {
			SKSE::log::debug("Cursor held in the game window for play");
		}
		// Focus may have left between the check above and the clip; don't trap the cursor then.
		if (::GetForegroundWindow() != g_window && g_lockedInPlay.exchange(false)) {
			::ClipCursor(nullptr);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Per-frame update, called from the Poll hook
	// ---------------------------------------------------------------------------------------------

	void UpdateMouseMode(RE::BSWin32MouseDevice* a_mouse)
	{
		const bool wantFree = g_wantFree;
		const bool isFree = g_isFree;

		if (g_retryNow.exchange(false)) {
			g_nextTry = {};
		}
		const bool mayTry = Clock::now() >= g_nextTry;

		if (wantFree && !isFree) {
			if (mayTry) {
				RecordAttempt(FreeMouse(a_mouse, true), "Freeing the mouse");
			}
			return;
		}
		if (!wantFree && isFree) {
			if (mayTry) {
				RecordAttempt(ReturnMouse(a_mouse), "Giving the mouse back to the game");
			}
			return;
		}
		if (!isFree) {
			LockCursorInPlay();
			LogVisibleCursor();
			return;
		}

		// The game builds a new mouse device whenever its window gets focus back, in exclusive mode.
		if (a_mouse->dInputDevice != g_freeDevice) {
			SKSE::log::info("Mouse device changed while free");
			g_freeDevice = a_mouse->dInputDevice;
			g_refreeNeeded = true;
		}
		if (mayTry && g_refreeNeeded.exchange(false)) {
			const bool freed = FreeMouse(a_mouse, false);
			if (!freed) {
				g_refreeNeeded = true;
			}
			RecordAttempt(freed, "Freeing the mouse again");
		}

		if (IsCursorClipped()) {
			if (!g_clipLogged) {
				g_clipLogged = true;
				SKSE::log::info("Cursor was confined to the window while free; removing that");
			}
			::ClipCursor(nullptr);
		}

		SyncMenuCursor();
		FilterQueuedMouseInput();
	}

	// ---------------------------------------------------------------------------------------------
	// Hooks
	// ---------------------------------------------------------------------------------------------

	struct PollHook
	{
		static void Thunk(RE::BSWin32MouseDevice* a_this, float a_timeDelta)
		{
			func(a_this, a_timeDelta);
			UpdateMouseMode(a_this);
		}
		static inline REL::Relocation<decltype(Thunk)> func;
	};

	// Initialize and Reinitialize both rebuild the device in exclusive mode.
	template <std::size_t Slot>
	struct DeviceResetHook
	{
		static void Thunk(RE::BSWin32MouseDevice* a_this)
		{
			func(a_this);
			if (g_isFree) {
				SKSE::log::info("Mouse device reset while free");
				g_refreeNeeded = true;
			} else {
				SKSE::log::debug("Mouse device reset");
			}
		}
		static inline REL::Relocation<decltype(Thunk)> func;
	};

	using InitializeHook = DeviceResetHook<kInitializeSlot>;
	using ReinitializeHook = DeviceResetHook<kReinitializeSlot>;

	// Subclasses the game window for two messages:
	//  - WM_ACTIVATEAPP: focus came back, so free the mouse again and retry anything that failed;
	//    focus left, so release the play lock.
	//  - WM_SETCURSOR: over the game, show exactly one cursor. While free, that's Skyrim's own if it
	//    draws one (no Windows cursor), otherwise the Windows arrow (loading screens).
	// It also handles g_hideCursorMessage, posted after the mouse goes back to the game.
	LRESULT CALLBACK WindowProc(HWND a_window, UINT a_message, WPARAM a_wParam, LPARAM a_lParam)
	{
		if (a_message == g_hideCursorMessage && g_hideCursorMessage != 0) {
			if (!g_isFree) {  // a menu may have opened again since this was posted
				::SetCursor(nullptr);
			}
			return 0;
		}

		switch (a_message) {
		case WM_ACTIVATEAPP:
			SKSE::log::debug("Game window {}", a_wParam ? "activated" : "deactivated");
			if (a_wParam) {
				g_retryNow = true;  // also covers a menu whose first switch hasn't happened yet
				if (g_isFree) {
					g_refreeNeeded = true;
				}
			} else if (g_lockedInPlay.exchange(false)) {
				::ClipCursor(nullptr);  // the play lock must not follow the user to other programs
			}
			break;

		case WM_SETCURSOR:
			if (g_isFree && LOWORD(a_lParam) == HTCLIENT) {
				::SetCursor(g_gameCursorShown ? nullptr : ::LoadCursorA(nullptr, IDC_ARROW));
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
		// Before the hooks, so the Poll hook never sees it unset.
		g_hideCursorMessage = ::RegisterWindowMessageW(L"SkyrimMouseRelease.HideCursor");
		if (g_hideCursorMessage == 0) {
			SKSE::log::warn("Could not register the hide-cursor message (error {}); the Windows cursor may show after a menu closes", ::GetLastError());
		}

		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_BSWin32MouseDevice[0] };
		InitializeHook::func = vtable.write_vfunc(kInitializeSlot, InitializeHook::Thunk);
		PollHook::func = vtable.write_vfunc(kPollSlot, PollHook::Thunk);
		ReinitializeHook::func = vtable.write_vfunc(kReinitializeSlot, ReinitializeHook::Thunk);
		SKSE::log::info("Mouse device hooks installed");
	}

	void InstallWindowHook()
	{
		g_window = GameWindow();
		if (!g_window) {
			SKSE::log::error("Game window not found; the mouse will not be freed correctly");
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

	void InstallMovementFilter()
	{
		auto* input = RE::BSInputDeviceManager::GetSingleton();
		if (!input) {
			SKSE::log::error("Input manager not available; the menu cursor will jitter while the mouse is free");
			return;
		}
		static MovementFilter filter;

		// First in the list, before the game's own handlers. AddEventSink would put it last.
		RE::BSSpinLockGuard locker(input->lock);
		if (input->notifying) {
			SKSE::log::error("Input handlers busy; the menu cursor will jitter while the mouse is free");
			return;
		}
		input->sinks.insert(input->sinks.begin(), &filter);
		SKSE::log::info("Movement filter installed, first of {} input handlers", input->sinks.size());
	}

	void SetWantFree(bool a_wantFree)
	{
		g_wantFree = a_wantFree;
	}

	void SetGameCursorShown(bool a_shown)
	{
		g_gameCursorShown = a_shown;
		if (g_isFree) {
			RefreshCursorShape();
		}
	}
}
