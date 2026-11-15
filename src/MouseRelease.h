#pragma once

// Frees the mouse while a watched menu is open.
//
// While released, Skyrim's DirectInput mouse runs in shared (non-exclusive) mode, the cursor
// clip is removed, Skyrim's menu cursor follows the Windows cursor, and the Windows cursor is
// hidden while it is over the game window, so only one pointer is visible at a time.
namespace MouseRelease
{
	// Hooks the mouse device; call once from SKSEPlugin_Load.
	void InstallHooks();

	// Hooks the game window; call once the window exists (kDataLoaded).
	void InstallWindowHook();

	// Thread-safe; applied on the main thread during the next mouse poll.
	void SetWanted(bool a_wanted);
}
