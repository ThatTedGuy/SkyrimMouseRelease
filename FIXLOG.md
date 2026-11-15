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

**Test 3 result:** one pointer, and refocus works. The log shows the game builds a new mouse device on every activation (`Initialize`, `Reinitialize`, new device pointer) and the re-apply succeeds each time. Left over: a loading wheel when the menu opens, jitter, and Skyrim's cursor moving on its own while the mouse is outside the window and Skyrim is still active.

## Round 2 (2026-11-14): no loading wheel, no jitter, edge pinning

**Causes**
- Loading wheel: shared mode shows the Windows cursor in its last shape until the next `WM_SETCURSOR`, which only comes when the mouse moves.
- Jitter: Poll queues the frame's mouse movement; the menu then adds it to the position the plugin just set, so the cursor overshoots by one frame of motion and snaps back.
- Cursor moving outside the window: while Skyrim stays active, shared-mode DirectInput still sends movement, and the Round 1 sync stopped writing the position once the mouse left the window.

**Changes**
- On release, if the cursor is over the game, a `WM_SETCURSOR` is posted to the game window so the existing handler hides the Windows cursor at once. New.
- After Poll, while released, the queued mouse-move event (`BSInputEventQueue`) has its movement set to zero, so the menu cursor stays exactly where the plugin puts it. New; logged once as "Zeroing the game's mouse movement".
- The sync no longer stops outside the window: it pins Skyrim's cursor to the nearest window edge. *Replaces* the Round 1 early return when the cursor is outside. Also keeps the cursor off the menu's buttons, so a click meant for another window presses nothing.

**Note:** an approach based on Cursor Unbound was proposed and dropped at the user's request; nothing from it is used. The queue approach is our own.

**Test 4 result:** works mostly. No wheel, smooth cursor, and the log shows the zeroing ran. Left over: pressing Esc while the mouse is on another screen leaves a visible Windows cursor that moves oddly, and the cursor sometimes starts in the middle when the menu reopens (cause unknown, maybe not this plugin). The log also showed `Restore` runs on worker threads, not the main thread.

## Round 3 (2026-11-14): bring the cursor back on close

**Causes**
- Stray cursor after Esc on another screen: exclusive mode leaves the Windows cursor where it is, and the plugin only hides it over the game window, so it stays visible on the other screen while the game reads movement from DirectInput.
- Cursor in the middle on reopen: not confirmed. While released, Skyrim's cursor copies the Windows cursor, so the Windows cursor was already in the middle; something moves it there during play.

**Changes**
- On close, if the Windows cursor is outside the game window, it is moved to the nearest edge spot (where Skyrim's cursor was pinned) before exclusive mode is restored. New.
- The log records the Windows cursor and menu cursor positions at every open and close. New, to find what centers the cursor. The user chose to leave the start position to the game.
- Comments and README corrected: the Poll hook runs on the main thread while paused and on a worker thread during play.

**Test 5 result:** pending. To check: Esc while on the other screen leaves no stray cursor; the log shows where the cursor is at each open.
