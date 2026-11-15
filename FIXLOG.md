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

**Test 5 result:** stray cursor after Esc on the other screen still happens, although the log shows the move back into the window ran and the restore succeeded. Cause still unknown; once the Windows cursor was 750 px from where the plugin put it at the next open, so something moved it during play. The center jump is explained: at every open the Windows cursor is at 960, 540 (centered during play by something else) while Skyrim's menu cursor is still where it closed. The menu draws its cursor at the old spot and only redraws on movement, so the first move jumps to the synced center.

## Round 4 (2026-11-14): start where the menu left off; watch the cursor after close

**Changes**
- On menu open (not on refocus), the Windows cursor is moved to where Skyrim's menu cursor is, so the sync starts from the game's own spot. New. The menu redrawing only on movement is the reason the Round 2 zeroing doesn't hide the jump; Round 2 is unchanged.
- For 3 seconds after each close, the log records every 250 ms whether the Windows cursor is showing, its position, its shape handle, and whether the game window is active. Diagnostic only, for the stray cursor; no fix until the cause is known. The Round 3 move-back stays.

**Test 6 result:** center jump fixed. Stray cursor found: after a normal close the log shows the Windows cursor `hidden ... shape 0`; after closing from the other screen it shows `showing ... shape 65539` (the standard arrow) for the whole 3 seconds. A screen recording (ByFrame sheets, 2x crop at 0:14) shows the arrow drifting over gameplay and clicking nothing. Also seen: a focus flicker during recording made one reapply fail (`Acquire=0x80070005`), and the next one, milliseconds later, succeeded.

## Round 5 (2026-11-14): reset the cursor shape after bringing it back

**Cause:** during play the Windows cursor is always present with no shape. On the other screen Windows gives it the arrow; Round 3 moves it back over the game, but the shape only resets on the next mouse message, and with the game holding the mouse exclusively none arrives.

**Change:** when Round 3's move-back runs, it also sets a one-shot flag and posts `WM_SETCURSOR` to the game window, and the existing handler sets no shape. *Extends* the Round 3 move-back and the Round 2 hide on open (same mechanism); only fires in that case, so other mods' cursors are untouched.

**Test 7 result:** fixed. Three closes from the other screen, each followed by `hidden ... shape 0`; no errors. The 3-second "After close" diagnostic from Round 4 is removed. All reported issues are resolved.

Saved: tag `v0.1-pause-menu` (commit `8db478a`) and its build `backups/SkyrimMouseRelease-8db478a-pause-menu-working.zip`.

## Round 6 (2026-11-14): more menus

New feature, not a fix: the user chose to free the mouse in the main menu, character creator, loading screens, inventory, magic and skills menus as well (not the map, which scrolls at the screen edge).

**Changes**
- `sMenus` default and the shipped INI list the new menus.
- One on/off flag *replaced* by the set of watched menus that are open, so closing one (the pause menu while a save loads, say) doesn't take the mouse back while another is still open.
- Watched menus already open when the listener is registered at `kDataLoaded` (the main menu) are added then. New.
- Cursor shape over the game while released now depends on whether Skyrim draws its own cursor (`Cursor Menu`): none if it does, the arrow if not (loading screens, which have no cursor). *Replaces* the Round 1 rule "always hide while released".
- The Round 5 one-shot hide on close now runs on every close, not only after bringing the cursor back from another screen (a loading screen leaves the arrow too). *Extends* Round 5; the shared posting code is one helper.

**Risk to test:** the Round 2 movement zeroing may stop the character creator, inventory item view and skills menu from turning with the mouse.

**Test 8 result:** all six areas work: main menu, character creator (turning the character still works), loading screens (arrow shows, leaving and coming back works), inventory (item turning works), magic, skills (turning between constellations works). Clicking back into the main menu or character creator starts the cursor near the middle; the user is fine with that. The loading-screen model couldn't be turned, but keys didn't turn it either, so it isn't this plugin.

Log notes for the safety pass:
- Twice a loading screen ended while the mouse was on the other screen and the restore's `Acquire` failed (`0x80070005`, access denied; the window lost focus 60 ms later). On the next activation the game rebuilt the mouse device itself and play continued normally. Recovery works, but it relies on the game.
- The session ended with the mouse still released (game closed from a menu). Harmless, since Windows releases the device with the process, but worth confirming in the safety pass.

**Decision:** the Tab menu (`TweenMenu`: Skills, Magic, Items, Map) stays locked. The user sees no need, since Esc gives the same freedom, and the map opens from it.

## Round 7 (2026-11-15): safety pass

Code review of all sources; nothing critical. Fixed, as approved:

- **Retry backoff.** A failed release retried every frame, and each try moved the Windows cursor, so a device that kept refusing would pin the cursor and flood the log. Now a failed release, re-apply or restore waits 1, 2, 4, then 8 s; focus coming back retries at once. The cursor moves to the menu's spot only after the release succeeds. A failed restore (both attempts) now keeps the released state and retries instead of giving up. *Replaces* the per-frame retry and the Round 4 move-before-switch order.
- **Block clicks and scrolls outside.** While the cursor is outside the window and Skyrim is still active, mouse button presses and wheel turns are unlinked from `BSInputEventQueue` (releases kept). Logged once per open, with a count on close. New, next to the Round 2 movement zeroing.
- **Quieter log.** Unwatched menu events, focus changes, cursor positions, enter/leave, device resets while not released and the first-zeroing line are now debug level; `bVerboseLog=1` turns them back on. *Replaces* logging everything at info.
- **Minor.** `bReleaseMouse`/`bVerboseLog` accept 0/1/true/false in any case and warn on anything else (was: only `1` or `true`, others silently off).

Not changed: missing Address Library (CommonLib's standard error; list it as a requirement), gamepad (can't be tested here).

**Test 9 result (1.6.1170):** all passed. Pause menu, inventory and loading screens work; scrolling and clicking on the other monitor did nothing in the inventory; the default log is 42 lines for the session. One log bug: the "Blocked N clicks or scrolls" count never printed, because every re-apply after refocus reset the count. Fixed by resetting it only when a menu opens (Round 7 code, same commit area; not a new mechanism).

**1.7.x decision:** not tested. The user's collection reaches 1.6.1170 through Skyrim Runtime Swapper, which patches the 1.7.104 game at launch and reverts it on exit, and only SKSE for 1.6.1170 is installed; testing 1.7.104 would mean changing that working launch setup. Release as tested on 1.6.1170, with 1.7.x stated as untested on the Nexus page (the plugin still declares Address Library compatibility, so it loads on 1.7.x).

## Cleanup (2026-11-15): readability, no behavior change intended

- `MouseRelease.cpp` reorganized into labeled sections with an overview comment at the top. Comments now explain why, without fix-round history.
- Names: `Reconcile` → `UpdateMouseMode`, `Release`/`Restore` → `FreeMouse`/`ReturnMouse`, `NoteSwitchResult` → `RecordAttempt`, `g_wanted`/`g_released` → `g_wantFree`/`g_isFree`, `SetWanted` → `SetWantFree`. Log labels follow: `Free`, `Free again`, `Return`, `Return with default flags`.
- A string comparison that decided behavior (`a_reason == "Release"`) replaced by a bool parameter.
- The two copy-pasted device-reset hooks became one template; the two passes over the input queue became one (`FilterQueuedMouseInput`).
- Removed two diagnostics from the fix rounds (menu cursor space, first zeroed movement).
- `bReleaseMouse=0` now skips the menu watcher too (it only logged).
- `iRestoreFlags` that isn't a number now logs a warning.
- `build.ps1` finds Visual Studio with `vswhere` instead of a fixed path.

**Cleanup test:** passed. The user repeated test 9; the log has no warnings or errors and now prints the blocked count ("Blocked 83 clicks or scrolls", "Blocked 1").

## Release 1.0.0 (2026-11-15)

Published on Nexus as mod 199209: https://www.nexusmods.com/skyrimspecialedition/mods/199209. Same build as the cleanup test (the DLL is newer than every source change; later commits only touched docs). The page settings, image and update steps are recorded in [release/nexus-page.md](release/nexus-page.md).
