// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ThatTedGuy

#pragma once

// Frees the mouse while a watched menu is open, so it can leave the game window. See
// MouseRelease.cpp for how.
namespace MouseRelease
{
	// Hooks the game's mouse device. Call once while the plugin loads.
	void InstallHooks();

	// Hooks the game window. Call once the window exists (kDataLoaded).
	void InstallWindowHook();

	// Whether a watched menu is open. Safe from any thread; applied on the next frame.
	void SetWantFree(bool a_wantFree);

	// Whether Skyrim is drawing its own cursor ("Cursor Menu" is open). Safe from any thread.
	void SetGameCursorShown(bool a_shown);
}
