# Changelog

## 1.0 (2026-10-30, first release)

The 0.2.0 build released as 1.0, with its own logo and a `Fix9mm` setting.

- Play Joel's chapters as Ellie: she moves, aims, shoots, reloads, interacts and crafts in every chapter; Joel's 9mm sits straight; Joel follows her as the companion.
- F10 switches between Ellie and the story character, with on-screen loading and "Swapped" messages.
- `Fix9mm` (1 = straight 9mm, the default; 0 = the game's own table). Known issue with 1: when the 9mm runs dry and the game switches weapons by itself, Ellie's arms stay out until she aims. Six mixed tables were tried in game (test builds 1.0.1 / 1.0.2, F11 to cycle); all tilted the gun up, so 1 stays.
- New artwork: logo, Fluffy Mod Manager preview (`screenshot=` in `modinfo.ini`) and Nexus header.
- GitHub: https://github.com/w1zr/Playable-Ellie
- Nexus Mods: https://www.nexusmods.com/thelastofuspart1/mods/259

## 0.2.0 (2026-10-30, test build, not released)

- **On-screen messages instead of beeps** (owner's request): a small box at the bottom left in w1z's menu's style. "Loading Ellie's animations... 50 / 51" with a spinner and progress bar, "Restarting checkpoint...", then "Swapped to Ellie" / "Swapped to Joel"; also "Playing as Ellie (F10 to switch)" on her first spawn, and any errors. `Notifications=1`. Beeps are now `Sounds=0` (off). The overlay is w1z's menu's DirectX 12 / Dear ImGui overlay without the menu; it does no ImGui work while no message is shown.
- **Faster F10 swap:** it no longer waits for `anim-ellie-player-mm-wade`, which never loads in Joel's chapters (every test: 50 of 51); the swap used to wait ~5.5 s for it to time out. Loading is also checked every 10 frames instead of 30.
- **Fix:** F10 pressed during the checkpoint restart toggled Ellie off again (seen in the 0.1.0 log). F10 is now ignored until the restart has finished and says "Still swapping - one moment".
- Lighter per-frame work: the player check runs once per frame (was up to five calls), package name hashes are computed once, hash maps for the look resolver, fewer look log lines (30 per spawn).
- No level-set memory scan at the main-menu load screen (it never found anything there and cost ~2 s of a background thread); the first search runs ~5 s after the game loop starts.
- The 0.1.0 settings file is updated in place to the new keys (values kept).

## 0.1.0 (2026-10-30, test build, not released)

First version, made from the playable Ellie in w1z's menu 1.2.0.

- Play Joel's chapters as Ellie, in the outfit for the chapter (or a fixed one, `Outfit=`).
- Her animations are streamed in by the game (level-set patch + the game's want-load rebuild), then the checkpoint restarts once. Also when Ellie is on at launch or a new chapter starts: at most two restarts in a row, so a failure can't loop.
- Crafting in every chapter (the game's Ellie crafting flag) and all recipes (the developer menu's All Recipes).
- Joel's 9mm held straight, with recoil (copy of the revolver's Ellie table with her pistol hold).
- Joel as the companion (look-resolver hook + Joel's companion packages), `JoelCompanion=1`.
- F10 toggles Ellie in game and restarts from the last checkpoint; beeps for feedback.
- Settings in `Documents\Playable Ellie (WORKING)\PlayableEllie.ini`; log `PlayableEllie.log` next to the game exe.
- Loads as `hid.dll` (no other TLOU mod uses it). Steps aside when w1z's menu (`winmm.dll`) is installed.
- Changes from the menu's code: no Play as Joel in Winter, no research hooks; in Ellie's own chapters the companion and the toggle key leave things alone; the 9mm table search is retried on Ellie spawns (its data may only be in memory once a game is loaded).
