#pragma once

// Switches Skyrim's DirectInput mouse between exclusive mode (the game owns the mouse)
// and shared mode (the Windows cursor can leave the game window).
// Must be called on the game's main thread.
namespace MouseRelease
{
	void Release();
	void Restore();
}
