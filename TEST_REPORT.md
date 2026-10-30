# Test report: Playable Ellie (WORKING)

Game: Steam The Last of Us Part I 1.1.4.0 (build 18995818), `tlou-i.exe` timestamp `0x685BEBE1`, `tlou-i-l.exe` `0x685BF099`.

## 1.0 (release, 2026-10-30)

Build: `build\hid.dll` SHA-256 `603b8bb9d710d5a679a480140068fe500aefeb3cb11e2fc6b89f4b45e456317d`; zip `release\PlayableEllieWorking-v1.0.0.zip` (`modinfo.ini`, `screenshot.png`, `hid.dll`, empty `PlayableEllie.log`, readme).
Same code as 0.2.0; only the version string changed. Offline: build with no warnings, `scan_test` all passed on both exes, `proxy_test` all passed.
In game: 1.0 itself not run; the code is 0.2.0's, which the owner ran and approved ("That's good. I like it."). The Fluffy install of the 1.0 zip still has to be ticked and checked by the owner.

## 0.2.0 (test build, 2026-10-30)

Build: `build\hid.dll`, 875,008 bytes, SHA-256 `78f738953c520e4421581cc1f19ed3e1cc3d9b8019991a16d75e4cf9984b2733`. Test zip: `release\test\PlayableEllieWorking-v0.2.0-test.zip`.

### Offline (done)
| Test | Result |
|---|---|
| `build.bat` (MSVC, `/W4 /WX`) | Builds with no warnings |
| `scan_test.exe tlou-i.exe tlou-i-l.exe` | All passed (38 checks; the resolver is unchanged since 0.1.0) |
| `proxy_test.exe` | All passed (47 exports, forwarding) |

### In game (the owner, 2026-10-30)
The owner played 0.2.0 and approved it ("That's good. I like it."). The log of that session shows the overlay hooks installed and the game's command queue captured. The rows below were not checked one by one.
| To test | Result |
|---|---|
| Messages appear at the bottom left: "Playing as Ellie (F10 to switch)" on her first spawn | Not tested |
| F10 to Joel: "Swapping to Joel..." / "Restarting checkpoint...", then "Swapped to Joel" | Not tested |
| F10 to Ellie: "Loading Ellie's animations... n / 51" with a progress bar, restart, "Swapped to Ellie"; the restart starts in about 1 s instead of ~5.5 s | Not tested |
| F10 pressed during a swap: "Still swapping - one moment", Ellie stays as she was | Not tested |
| No visible hitch or FPS drop from the overlay | Not tested |
| The 0.1.0 ini is updated in place (Notifications / Sounds keys, values kept) | Not tested |
| Everything 0.1.0 did still works | Not tested |

## 0.1.0 (test build, 2026-10-30)

Build SHA-256 `2476ea75f0f1be02dccd7867405d6f5d7e8afbfa999090644aa4c0145a250aae`. Offline: all passed (38 scan checks, proxy test).

### In game (the owner, 2026-10-30): working
The owner played it and said everything worked. From `PlayableEllie.log` of that session:
| Test | Result |
|---|---|
| Mod loads, every signature found, ready | Passed |
| Load a Joel chapter with Ellie on | Passed: `t1x-joel-costume-02 -> t1x-ellie-costume-02`, 50 of 51 packages already loaded (the level-set patch was in before the chapter loaded), so no restart was needed |
| 9mm | Overlay tables found; "the revolver's animations with Ellie's pistol hold" in use |
| Recipes | All Recipes ran ~3 s after the spawn |
| F10 to Joel and back | Passed (Joel spawn, patches restored; back to Ellie after streaming + restart) |
| Problems seen | Each F10 swap to Ellie waited ~5.5 s for `anim-ellie-player-mm-wade`, which never loads; F10 pressed during the restart toggled Ellie off again; the beeps were annoying (all fixed in 0.2.0) |

Not tested yet (both versions): cutscenes, chapter transitions, death and checkpoint reloads, `JoelCompanion=0`, `Outfit=`, Winter with the mod on, together with w1z's menu, `tlou-i-l.exe`, together with TLOU Co-op (its `avrt.dll` was installed during the test; no problems seen).

Known issues carried over from w1z's menu (not worked on, as asked): bow clips Ellie's arm when put away; shotgun / hunting rifle twitch when aiming; companion Joel speaks Ellie's lines.
