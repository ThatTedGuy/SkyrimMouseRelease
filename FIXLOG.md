# Fix log

One entry per round of fixes: what changed, why, and what it replaced.

## Round 1 (2026-11-14): one pointer, survive focus changes

**Found in test 2** (prototype `f05021b`): the cursor could leave the window, but Skyrim's menu cursor and the Windows cursor moved separately, and after clicking another monitor and back once, the cursor was trapped again. The log showed the release succeeding (all calls `0x0`) and nothing after it.

**Causes**
- Two pointers: in shared mode Windows shows its own cursor, while Skyrim keeps moving its menu cursor from raw DirectInput deltas. The two use different speeds, so they drift apart. Confirmed by design.
- Trapped again: not yet confirmed. Likely Skyrim resetting the mouse to exclusive mode or re-clipping the cursor on refocus, which the prototype never noticed.

**Changes**
- Mouse switching moved from SKSE tasks into a hook on `BSWin32MouseDevice::Poll`. *Replaces* the task-queue release and restore; menu events now only set a flag.
- While released, Skyrim's menu cursor is set to the Windows cursor position each frame, and the Windows cursor is hidden over the game window (`WM_SETCURSOR`). New.
- Window procedure hook logs `WM_ACTIVATEAPP` and schedules a re-apply on refocus; `Initialize`/`Reinitialize` hooks log device resets and schedule a re-apply; a per-frame check removes and logs any cursor clip that reappears. New, and the new log lines show which mechanism traps the cursor.
- `bReleaseMouse` now defaults to `1`.

**Research before test 3:** no existing mod frees the mouse in menus. Cursor Unbound (Nexus 188955) replaces the menu cursor with the Windows cursor but keeps it clipped to the window and leaves DirectInput exclusive. Decision: stay standalone.

**Test 3 result:** pending.
