#pragma once

// Frees the mouse while a watched menu is open.
//
// While released, Skyrim's DirectInput mouse runs in shared (non-exclusive) mode, the cursor
// clip is removed, Skyrim's menu cursor follows the Windows cursor, and over the game window only
// one pointer shows: Skyrim's when it draws one, otherwise the Windows arrow.
namespace MouseRelease
{
	// Hooks the mouse device; call once from SKSEPlugin_Load.
	void InstallHooks();

	// Hooks the game window; call once the window exists (kDataLoaded).
	void InstallWindowHook();

	// Thread-safe; applied during the next mouse poll.
	void SetWanted(bool a_wanted);

	// Thread-safe; whether Skyrim's own cursor ("Cursor Menu") is open.
	void SetGameCursorShown(bool a_shown);
}
