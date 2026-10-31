# Test report: Playable Ellie (WORKING)

Game: Steam The Last of Us Part I 1.1.4.0 (build 18995818), `tlou-i.exe` timestamp `0x685BEBE1`, `tlou-i-l.exe` `0x685BF099`.

## 1.0 (release, 2026-10-30): GitHub v1.0 and Nexus mod 259

Build: `build\hid.dll` SHA-256 `ec6b64930d86b6d6c66d46294160ff40f2503f26efda76c90b8509096d115384`; zip `release\PlayableEllieWorking-v1.0.0.zip` (`modinfo.ini`, `screenshot.png`, `hid.dll`, empty `PlayableEllie.log`, readme). It replaced the first GitHub upload (SHA-256 of that `hid.dll`: `603b8bb9...`, kept in `release\superseded\`).
0.2.0's code plus the `Fix9mm` setting (default 1, same 9mm table as 0.2.0).

### Automatic weapon switch freeze (owner, 2026-10-30): known issue
| Test | Result |
|---|---|
| 1.0 / 1.0.1-test with `Fix9mm=1`: 9mm runs dry, the game switches weapons by itself | **Fail**: Ellie's arms stay out with no animation until she aims |
| 1.0.1-test with `Fix9mm=0` (the game's own 9mm table) | No freeze, but the 9mm tilts up again |
| 1.0.2-test, tables 2-6 (mixes of the 9mm's and the revolver's entries, F11 to cycle) | All tilted the 9mm up (owner) |
| Decision | Keep table 1 (straight), list the freeze as a known issue (aim once to recover); `Fix9mm=0` documented as the alternative |

Offline: build with no warnings, `scan_test` all passed on both exes, `proxy_test` all passed.
In game: the 1.0 build itself has not been run. Its behaviour with `Fix9mm=1` is 0.2.0's, which the owner ran and approved, and the owner's ini is set to 1. The 1.0 zip still has to be ticked in Fluffy in place of the 1.0.2 test build.

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
