# Test report: Playable Ellie (WORKING)

Game: Steam The Last of Us Part I 1.1.4.0 (build 18995818), `tlou-i.exe` timestamp `0x685BEBE1`, `tlou-i-l.exe` `0x685BF099`.

## 1.1 (release, 2026-11-01)

Build: `build\hid.dll` SHA-256 `aba5498ce8e13afaff3c484b240a38f5bbe5f9442f44c5c4e39e57fc6a6e43a1`; zip `release\PlayableEllieWorking-v1.1.0.zip` (`modinfo.ini` v1.1, `screenshot.png`, `hid.dll`, empty `PlayableEllie.log`, readme). The 1.0.22-test code with the version set to 1.1.0 (1.0.22-test was already built without the F11 bow cycling and the research hooks). Offline: build with no warnings, `scan_test` all passed on both exes (46 checks, including the Skins menu, level-load and Extras > Skins sites), `proxy_test` all passed.

| In game (1.0.x test builds) | Result |
|---|---|
| ReShade + OptiScaler, no crash on spawn or F10 (messages off on purpose) | Passed (player, 1.0.8+) |
| Ellie's full body (outfit of the chapter) on the player's laptop | Passed (player, 1.0.12+) |
| Companion Joel visible | Passed (player, 1.0.16+) |
| Extras > Skins: no crash, Ellie in the preview | Passed (player, 1.0.18) |
| Every Extras skin unlocked (UnlockSkins) | Passed (owner, 1.0.19) |
| Bow vertical on her back (BowOnBack) | Passed (owner, 1.0.22: "that worked"; log: Ellie's bow settings while Ellie, Joel's own for Joel) |
| Back to Ellie after the Pittsburgh backpack scene | **Fail, known issue** (the scene changes Joel's look without a spawn; F10 twice fixes it). 1.0.22's log shows the new look read every frame from worker threads (`0x1FBC6B3`, `0x1FBC721`, `0x1FBE1A2`): lead for a fix |
| The 1.1.0 build itself in game | Not run (same code as 1.0.22-test) |

## 1.0.22-test (2026-11-01): bow on Ellie's back

Owner: the bow should sit vertically like in w1z's menu; only the bow, none of the earlier table changes. Change: 1.0.21-test + the bow settings from 1.0.6-test switched on in every build (Joel's bow record gets Ellie's `+0x158`, `+0x180`, `+0x198` values while Ellie plays, Joel's own for Joel; no overlay table), with the new ini key `BowOnBack` (default 1; 0 = Joel's own). The F11 bow cycling stays test-only. `release\test\PlayableEllieWorking-v1.0.22-test.zip` (in Fluffy): `hid.dll` SHA-256 `97af2da6ccc0ee4f029e8464874c80fd32330f9ff964ebb3e884c70e16072f0a`. Offline: build, `scan_test` both exes, `proxy_test` passed. In game: **Not tested**.

| Test | Result |
|---|---|
| Bow put away: vertical on her back | Not tested |
| Bow aim and shoot: no crouch, bow visible | Not tested |
| Log: "Joel's bow uses all of Ellie's bow settings" | Not tested |
| Backpack scene log (1.0.21) | Not tested |

## 1.0.21-test (2026-11-01): backpack scene log

Owner on 1.0.20: the spawn after the cutscene is right now (`t1x-joel-costume-02-no-backpack -> t1x-ellie-costume-01-no-backpack`, parts loaded, no restart). But later in that scene Joel picks up his backpack and the player turns into Joel (screenshot: Joel in cover, store). No player spawn in the log between 18:07:41 and the owner's F10 at 18:08:47: the game changes the player's look to `t1x-joel-costume-02` without a spawn, and the mod only swaps at spawns. The look log budget was used up, so the request isn't visible. Swapping every Joel-outfit request would also turn the companion (now `t1x-joel-costume-02` itself) back.

Change (diagnostic only): every Joel-outfit request outside the spawn while Ellie plays is logged with caller and thread (up to 60). `release\test\PlayableEllieWorking-v1.0.21-test.zip` (in Fluffy): `hid.dll` SHA-256 `f340c355be02e06121cd2e1fe7c9d2e0b0da1b95c2cde474a25af278bd453e11`. Offline tests passed.

## 1.0.20-test (2026-11-01): back to Ellie after a cutscene

Owner on 1.0.19 ("everything is working great, almost ready to publish"; skins log: unlock flag, 2 menu checks, 2 owned + 1 equip, installed): in a chapter played with cutscenes, after a cutscene the player came back as Joel; F10 twice brought Ellie back.

Log: after the cutscene the player spawned as `t1x-joel-costume-02-no-backpack -> t1x-ellie-costume-02-no-backpack`. Only `joel-costume-02` had a known chapter outfit, so the learner saw the companion's outfit 01 and restarted the checkpoint by itself ("Dressing Ellie in this chapter's outfit...") right after the cutscene: that's when the owner saw Joel. Then two F10 presses (18:00:16, 18:00:31).

Change: no automatic restart any more; a learned outfit applies from the next spawn. Joel's no-backpack variant uses the same chapter outfit as his normal one, with Ellie's matching no-backpack variant (`t1x-ellie-costume-01-no-backpack` here).

`release\test\PlayableEllieWorking-v1.0.20-test.zip` (in Fluffy): `hid.dll` SHA-256 `cb687de3c7be625d0d6c289e5a8508489b58703bcb816d8e0b23b3658e2c7d42`. Offline: build, `scan_test` both exes, `proxy_test` passed. In game: **Not tested**.

| Test | Result |
|---|---|
| Same cutscene: back to Ellie (log: `t1x-joel-costume-02-no-backpack -> t1x-ellie-costume-01-no-backpack`, no restart) | Not tested |
| Ellie's full body after the cutscene, Joel companion | Not tested |
| UnlockSkins (1.0.19) | **Passed** (owner: log "installed", everything working) |

## 1.0.19-test (2026-11-01): every Extras > Skins skin unlocked (UnlockSkins)

Owner: the mod should come with every Extras skin, like TLOU Outfit Changer, optional and on by default.

Change: new setting `UnlockSkins` (default 1). The patches are TLOU Outfit Changer 1.0.0's, unchanged (`src/skinscan.*` = its `scan.*`, `src/skins.cpp` = its `game.cpp` install): the main-menu and in-game Skins menu checks read a byte the mod owns instead of the debug "Unlock All Extras Content" bool; the two "owned" display checks and the equip check jump to stubs that answer "owned". Points, saves and the profile are never touched. Every site by signature with exact counts; any mismatch writes nothing. If TLOU Outfit Changer is installed (`TLOUOutfitChanger.ini` in the game folder, or its `xinput1_4.dll` loaded), this part steps aside. The ini also gets the new key and comments (Outfit vs Extras skins, Notifications off with ReShade/OptiScaler, changes apply at the next start); older ini files are rewritten with the new key, values kept.

`release\test\PlayableEllieWorking-v1.0.19-test.zip` (in Fluffy): 1.0.18-test + this. `hid.dll` SHA-256 `7a7c55ea579e89926193264972b4778babb9d34f342049da19470cafb6ed4c3a`. Offline: build, `scan_test` both exes (skin sites found: unlock flag, 2 menu checks, 2 owned + 1 equip; stubs build), `proxy_test` passed. In game: **Not tested** (the same patches are tested in game in TLOU Outfit Changer 1.0.0).

| Test | Result |
|---|---|
| Log: "skins: installed: Skins menus open, every skin owned" | Not tested |
| Extras > Skins open on any save; every Ellie and Joel skin owned, equip works, points unchanged | Not tested |
| UnlockSkins=0: the game's own locks | Not tested |
| With TLOU Outfit Changer installed: "left to it" | Not tested |

## 1.0.18-test (2026-11-01): Skins preview, second try

Tester on 1.0.17: preview still Joel (with a skin after F10 twice: Joel's head on a backpack). In game fine (Ellie full body, companion `-> t1x-joel-costume-02`; as Joel, the companion Ellie wears the selected skin). Log: the level-load looks were left alone, so that theory was wrong. The Ellie tab's skin requests are the base look `t1x-ellie` (no outfit number) plus outfits 06/11/13, all left alone; the preview body must come from another request for `t1x-ellie` that was turned into `t1x-joel` (not logged: the per-spawn log budget was used up).

Change: `t1x-ellie` (the base look) is never swapped; in every game log so far, in-game Ellies ask for numbered outfits only. Logged as "t1x-ellie (no outfit) asked for (caller ...): left alone" (up to 10). The level-load exception from 1.0.17 stays.

`release\test\PlayableEllieWorking-v1.0.18-test.zip` (in Fluffy): `hid.dll` SHA-256 `895cf6c264eb50db9a7ed79c618bdd6f643d3e6606cd9bbe5c882c3acfb5dd11`. Offline: build, `scan_test` both exes, `proxy_test` passed. In game: **Not tested**.

| Test | Result |
|---|---|
| Skins menu, Ellie tab: Ellie in the preview, with and without a skin, after F10 | **Passed** (tester: "everything works") |
| In game: Ellie full body, Joel companion | **Passed** (tester) |
| A cutscene with the companion: Joel | Not tested |

## 1.0.17-test (2026-11-01): Skins menu Ellie preview

Owner: fix the preview (in game everything works; the tester's screenshot also shows an Ellie skin applied in game). Change: the level-load look registration (signature `kLevelLookCall`, call must go to the look resolver; returns at `0x1359D4E` in `tlou-i.exe`, `0x135B98E` in `tlou-i-l.exe`) is never swapped; characters still become Joel when they spawn. Skin-system log lines now only when the look changes (up to 60), so the Ellie tab shows in the log. Theory not confirmed: if the preview still shows Joel, the log's skin lines show what the Ellie tab asks for.

`release\test\PlayableEllieWorking-v1.0.17-test.zip` (in Fluffy): `hid.dll` SHA-256 `72ff64d6c2f9cafdd740e5e3316ee79af1545276eaa758ef45e6a0476865d8d3`. Offline: build, `scan_test` both exes (new check passes on both), `proxy_test` passed. In game: **Not tested**.

| Test | Result |
|---|---|
| Skins menu, Ellie tab: Ellie in the preview (as Ellie and after F10) | Not tested |
| Joel companion still Joel, Ellie full body | Not tested |
| A cutscene with the companion: Joel, not Ellie | Not tested |

## 1.0.16-test (2026-11-01): the companion becomes the chapter's Joel

Tester on 1.0.15, `Outfit=0`, no skin requests in the log: player `t1x-ellie-costume-01` with legs and torso loaded (full body), but the companion is again "a walking flashlight". The companion's Ellie outfit 01 was turned into Joel's outfit **01** by number (`-> t1x-joel-costume-01`), but in Pittsburgh Joel wears **02**: the level doesn't load Joel 01, the owner's PC streams it in time, the tester's doesn't. Same cause as Ellie's missing body, mirrored. (So 1.0.14's flashlight was most likely this too, not the skin.)

Change: the companion becomes Joel in the outfit the player spawn started from (`t1x-joel-costume-02` in Pittsburgh), keeping the no-backpack variant when both have one; by number only when that's unknown.

`release\test\PlayableEllieWorking-v1.0.16-test.zip` (in Fluffy): `hid.dll` SHA-256 `6c7d81e98e6e50e825b7215a80ac302af7fc34041c254b2a4666a88c1e87134c`. Offline: build, `scan_test` both exes, `proxy_test` passed. In game: **Not tested**.

| Test | Result |
|---|---|
| Tester: Ellie full body + Joel companion visible (log: `t1x-ellie-costume-01 -> t1x-joel-costume-02`) | **Passed** (tester: "everything works", screenshots: Ellie full body, Joel companion; F10 both ways) |
| Owner: same, nothing else changed | Not tested |
| Skins menu: no crash | **Passed** (tester) |
| Skins menu, Ellie tab: preview | **Fail (cosmetic):** shows Joel; with an Ellie skin, Joel's head and a backpack without a body. Likely cause: at level load (`0x1359D4E`) the Ellie looks the level registers (outfits 06, 15, 11, 13) are turned into Joel's while Play as Ellie is on, and the menu previews reuse them for that session (even after F10 to Joel). Not changed yet: the same looks probably serve cutscenes |

## 1.0.15-test (2026-11-01): Joel companion with an Ellie skin equipped

Tester on 1.0.14 (`Outfit=11` = outfit 15, Ellie skin "Naughty Dog T-Shirt 3" equipped): **no Joel companion**, "only a walking flashlight" (the companion is there but invisible). On 1.0.13 (same skin) Joel was there. The game applies the selected Ellie skin to every Ellie, the companion included: as Joel, the companion asks for outfit 01 (`0x135BCF7`), then the skin system for outfit 15 (`0x135BF76`, ...). `SkinManager::Skin`'s call also dresses characters right after a spawn, so 1.0.14's exception left the companion's skin as Ellie's on Joel's body.

Change: the skin call is left alone only when no player spawn happened in the last 3 s (the Skins menu); right after a spawn it is swapped like any other request. Every skin-call decision is logged ("asked for by the skin system (Skins menu: left alone / after a spawn: swapped like any other)", up to 40).

`release\test\PlayableEllieWorking-v1.0.15-test.zip` (in Fluffy): `hid.dll` SHA-256 `3243da81ecd6d1463fd579e61e66eca637b835968fcb6134a9c2890fb2987af6`. Offline: build, `scan_test` both exes, `proxy_test` passed. In game: **Not tested**.

| Test | Result |
|---|---|
| Ellie skin equipped, as Ellie: Joel companion visible | Not tested |
| Extras > Skins > Ellie while playing as Ellie: no crash | Not tested |
| Full body with Outfit=0 | Not tested |

## 1.0.14-test (2026-11-01): Skins menu left alone

Tester on 1.0.12, `Outfit=0`, Pittsburgh: **full body** (outfit 01), companion still Ellie (the 1.0.12 bug fixed in 1.0.13). Then in Extras > Skins > Ellie the preview showed **Joel** and the game **crashed**: the companion swap turned the menu's Ellie skin looks into Joel's. Owner's 1.0.13 run: spawn `t1x-joel-costume-02 -> t1x-ellie-costume-01`, companion `-> t1x-joel-costume-01`, outfit parts loaded; a minute later bursts of outfit-01 requests from `0x135BCF7` on worker threads, kept as Ellie (asked the owner what happened then).

Change: `SkinManager::Skin`'s look call (signature `kSkinLookCall`; its call must go to the look resolver; return address `0x174229F` in `tlou-i.exe`, `0x17402BF` in `tlou-i-l.exe`) is never swapped. Log: "Skins menu look call found (left alone)". `scan_test` checks it on both exes.

`release\test\PlayableEllieWorking-v1.0.14-test.zip` (in Fluffy): 1.0.13-test + this. `hid.dll` SHA-256 `e5b164eb6efdee01237d48bce4c81d27566d1b4832f709e6b715d75c61e4074e`. Offline: build, `scan_test` both exes, `proxy_test` passed. In game: **Not tested**.

| Test | Result |
|---|---|
| Extras > Skins > Ellie while playing as Ellie: preview shows Ellie, no crash | Not tested |
| Full body, Joel companion (tester, Outfit=0) | Not tested |

## 1.0.13-test (2026-11-01): chapter outfit, and the companion still becomes Joel

Tester on 1.0.11 with `Outfit=1`: **body visible** ("that's a victory"), but **companion Joel gone**. Cause: since 1.0.7 the player's outfit resolved anywhere stayed Ellie, so a companion in the same outfit (01) stayed Ellie. 1.0.12 would do the same in Pittsburgh; not sent on.

Change: the player's outfit asked for again counts as the player's only on a worker thread (where the tester's late requests come from, callers `0x18F6B4F` / `0x1FBE1A2`); on the spawn thread outside the spawn (where the companion's requests come from) it is the companion's and becomes Joel. Plus 1.0.12's chapter outfit.

`release\test\PlayableEllieWorking-v1.0.13-test.zip` (in Fluffy): `hid.dll` SHA-256 `9554e82791426bd9f7fb47434328a257da66ca257e8cd660c527ca58f241df53`. Offline: build, `scan_test` both exes, `proxy_test` passed. In game: **Not tested**.

| Test | Result |
|---|---|
| Tester, Outfit=0, Pittsburgh: full body and Joel as the companion | Not tested |
| Owner: Ellie in outfit 01, Joel companion, nothing else changed | Not tested |

## 1.0.12-test (2026-11-01): Ellie wears the chapter's own outfit

Tester's 1.0.11 log: the late requests come from `0x18F6B4F` (a function in a table at `0x3105c80`, in the backpack/inventory code area, that reads a fixed flag of the look) and `0x1FBE1A2` (look helper), on many worker threads, once per frame; they read the look, they don't load it. Then the tester found **Outfit=1 works** (body visible). Outfit 01 is what the companion Ellie wears in Pittsburgh, so the level loads it; Outfit=0 used Joel's number (Joel 02 -> Ellie 02), an outfit this chapter doesn't use, which the owner's PC streams in fast enough and the tester's doesn't.

Change: with `Outfit=0`, Ellie wears the outfit the chapter's own Ellie (the companion) wears. Known pair: Joel `t1x-joel-costume-02` -> `t1x-ellie-costume-01` (Pittsburgh). Other chapters are learned: an Ellie look resolved for someone else within 5 s of a swapped spawn is taken as the chapter's Ellie outfit; if it differs from the player's, the log says "outfit: this chapter's Ellie wears ..." and, within 4 s of the spawn, the checkpoint restarts once ("Dressing Ellie in this chapter's outfit..."). Learned pairs last for the session. `Outfit=N` is unchanged. The learning needs the look hook (`JoelCompanion=1`).

`release\test\PlayableEllieWorking-v1.0.12-test.zip` (in Fluffy): 1.0.11-test + this; no bow change, no research log. `hid.dll` SHA-256 `06c11251a19ccb8f846382bcae090dbef3fb41f071f78f23298b7e86ccb9e51c`. Offline: build, `scan_test` both exes, `proxy_test` passed. In game: **Not tested**.

| Test | Result |
|---|---|
| Owner, Pittsburgh: player spawns as `t1x-ellie-costume-01`, no restart, full body | Not tested |
| Tester, Outfit=0, Pittsburgh: full body | Not tested |
| A chapter without a known pair: one restart, then the companion's outfit | Not tested |

## 1.0.11-test (2026-11-01): invisible body diagnostic for the tester

Tester (mantismeraklisi) on 1.0.9-test with `Outfit=0` and with `Outfit=8`: torso and arms still invisible (screenshot: head, hair, legs and a green backpack showing; the owner's Ellie in the same place has a blue backpack and a full body). Skins in Extras are default.

Findings:
- The tester's log has five look requests for the player's outfit 50 to 70 ms after the spawn, from other threads ("resolved outside the player spawn"). The owner's logs never have them. They are where the body breaks: in 1.0.0 they were turned into Joel's look (arms missing), in 1.0.7+ they stay Ellie's (torso and arms missing).
- The 17 requests from caller `0x174229F` before the spawn are `SkinManager::Skin::Skin(SpSkinEntrySet...)`: the Skins menu building its previews while the tester checked it. Not the cause (not in their 1.0.8 log).
- The outfit-part check ("her outfit ...: loaded [], not loaded [...]") says outfit 02's parts aren't loaded on the owner's PC either, where she is fully visible. So those part packages aren't how the game normally loads her outfit, and the 1.0.9 outfit-part theory is weak.

Change: the late request log line now has the caller and thread (up to 20 lines). `release\test\PlayableEllieWorking-v1.0.11-test.zip`: the 1.0.1 candidate's code plus this line (no bow change, no research log). `hid.dll` SHA-256 `1cec6031873d9f629efb5ee74047a61bc9c9b4aed2899d5a17bc0ebbfe0f0119`. Not tested.

## 1.0.10-test (2026-11-01): freeze and twitch research log

Owner: after a reload (e.g. the rifle) Ellie stands with her arms out and freezes for about a second (she can still move, but no running animation); near walls, cars and anything close to the gun, her arm and gun twitch. Same family as the bow post-shot and 9mm auto-switch freezes, where guessing table entries (1.0.3, 1.0.4) failed.

Lead: the exe has "Anim not found! All previous layers overwritten with bindpose"; arms out is the bind pose. So an animation the overlays point to is likely missing for Ellie at that moment.

1.0.9-test on the owner's PC with `Outfit=8`: the spawn was `t1x-ellie-costume-08`, all 5 parts loaded, full body. Outfit 08 looks almost like 02 (the Pittsburgh outfit), so the owner saw no change. The ini's `Outfit` was set back to 0 (backup `PlayableEllie.ini.bak-2026-11-01`).

Change (diagnostic only, test builds, `ResearchOverlays=1` in the ini, this game build only): w1z's menu's overlay research logging (SetSlot `0x1FD1C84`, apply-by-name `0x1FD1D54` / `0x1FD1D20`, name lookup `0x1EFBD28`), with markers for aim (right mouse), reload (R) and F9 ("MARK": press when she freezes or twitches). No change to the game's behaviour. `hid.dll` SHA-256 `678ba2253b25aeefd36dfcbbe65d07eb112cdaf79d17f7d686c4d2c89cdd40e1`; zip in Fluffy.

Results (owner's research runs, 2026-11-01, one log):
- **Twitch near the car, first run:** the player's slot 75 flips `anim-overlay-player-aim-block-default` ↔ `anim-overlay-player-aim-block-aim-blocked` (gun blocked by the car), sometimes twice within 14 ms, and each flip also flips slot 49 (`weapon-tweaks-aim` ↔ `-normal`) and slot 36. The game keeps deciding "blocked / not blocked".
- **Arm moving near the car, second run (MARK 16:18:24):** no aim-block flips, but slot 23 switches `anim-overlay-player-cover-left-hand-off-wall` ↔ `cover-left-hand-on-wall` (`0xD8FCDE1FB5EA5D52`) about once a second; slot 22 is the right hand (`0x81EE86346A395977`). Her hand keeps reaching for the car and letting go.
- Counts in the whole log (Ellie ~6 min, Joel ~40 s at the same cars): slot 75 Ellie 50 / Joel 4, slot 23 Ellie 23 / Joel 5, slot 22 Ellie 14 / Joel 9. Not normalised; Joel needs a longer run at the same spot.
- **9mm runs dry (MARK 15:57:52):** ammo `no-ammo`, then the automatic switch (`weapon-base-pistol` → `-shotgun`, draw-holster pistol → long-gun); Joel's switch looks the same apart from the next weapon (long gun). No obvious overlay difference yet.
- Both twitches are decided by the game's probes (`aim-block.cpp`, the wall-hand check), not by overlay tables. Their settings aren't in the loose DC files (only packed); next step is finding the probe values Ellie's own guns use.

Plan: owner reloads the rifle as Ellie (F9 at the freeze), stands by a car (F9 at the twitch), then F10 and does the same as Joel; the overlays changed right before each MARK, compared with Joel's, are the candidates. Change one live, confirm on screen, then build it in.

## 1.0.9-test and 1.0.1 candidate (2026-11-01): outfit parts for Outfit=

mantismeraklisi's full 1.0.8 log: `Outfit=8`, so the player was `t1x-ellie-costume-08`. The thread fix worked (five "resolved outside the player spawn; kept as the player's" lines right after the spawn). They said: on 1.0.0 only her arms were missing; on 1.0.7 everything but her head was missing.

Cause: Ellie's outfits are made of parts (`ellie-costume-NN-torso`, `-legs`, `-torso-no-backpack`, bandages) that no level set lists; the look system streams them in when the outfit is put on (`look2/characters.bin`). The mod's patch loads her animations and backpack, not these. Before 1.0.7 the late look requests were turned into Joel's (only her arms went missing); with them kept as Ellie's outfit 08, every unloaded part is missing.

Change: with `Outfit=N`, the level-set patch also loads that outfit's parts (those in `actor97.txt`), the swap waits for them, and a spawn with any of them missing goes through the normal reload-and-restart. Every Ellie spawn logs "her outfit ...: loaded [...], not loaded [...]" for any outfit, to check `Outfit=0` too.

Builds (offline: build, `scan_test` both exes, `proxy_test` all passed):
- `release\test\PlayableEllieWorking-v1.0.9-test.zip` (in Fluffy): 1.0.8-test + this. `hid.dll` SHA-256 `eadeb3ad152530917ea8e5878bb930fd5530fe67b48e445a62948aa0aa0014b2`.
- `release\test\PlayableEllieWorking-v1.0.1-candidate.zip` (replaced again): 1.0 + look fix + overlay fix + this; no bow change. `hid.dll` SHA-256 `61efbb4d9dc5fb8b5a7631852eef8c9a785d49a6f99cdc5e1e961b90789b4cce`. Not published.

| Test | Result |
|---|---|
| Reporter, Outfit=8: full body, arms and legs | Not tested |
| Log: "Outfit=8 also loads its parts", "her outfit ...: loaded [...]" | Not tested |
| Owner, Outfit=0: nothing changed; check the "her outfit" line | Not tested |

## 1.0.8-test and 1.0.1 candidate (2026-11-01): ReShade / OptiScaler crash

mantismeraklisi (Nexus, before trying 1.0.7): the crash on spawn and on F10 comes from the mod's message overlay. With Notifications=1 the game crashed with ReShade alone and with OptiScaler alone; with Notifications=0 and both active, no crash and F10 works. The invisible arms and torso are random (not the laptop power profile).

Change: at startup, if `ReShade.ini`, `OptiScaler.ini`, a `dxgi.dll` or a `d3d12.dll` is in the game folder (or `ReShade64.dll` / `OptiScaler.dll` is loaded), the overlay isn't installed; the log says why, and messages go to the log only. The owner's game folder has none of these, so nothing changes there.

Builds (offline: build, `scan_test` both exes, `proxy_test` all passed):
- `release\test\PlayableEllieWorking-v1.0.8-test.zip` (in Fluffy): 1.0.7-test + this. `hid.dll` SHA-256 `97e80f3f323b706d0760fb53bb6a640da271957f8604fb4382673ff10a207da8`.
- `release\test\PlayableEllieWorking-v1.0.1-candidate.zip` (replaces the earlier candidate): 1.0 + the look fix + this; no bow change. `hid.dll` SHA-256 `e342f1aad74020cca1df5d54ba4ee629830c83d4a40b7b16e23813b6207fa5f3`. Not published.

| Test | Result |
|---|---|
| Owner's PC: messages still show; nothing else changed | Not tested |
| With ReShade or OptiScaler: log says "overlay: off because of ...", no crash | **Passed** (mantismeraklisi, 1.0.8-test, ReShade + OptiScaler, Notifications=1: "overlay: off because of ReShade", no crash, no messages as intended) |
| Invisible arms / torso gone | **Fail** for mantismeraklisi on 1.0.8-test ("Ellie is the same"); on 1.0.7 they said her loading looked worse. Full log asked for |

Lead (offline): Ellie's outfit parts (`ellie-costume-NN-torso`, `-legs`, bandages) are not in any level set; the look system (`look2/characters.bin`) streams them when a character uses the outfit. The mod's patch loads her animations and backpack but not these, which fits "only head and backpack" and "random". Not confirmed; needs the reporter's full log (player spawn and `look:` lines) and the chapter.

## 1.0.7-test and 1.0.1 candidate (2026-11-01): invisible body with Joel as the companion

Reports on Nexus (posts on mod 259):
- samsung2026 (Windows, Steam): with JoelCompanion on, in the forest chapters after the Capitol, Ellie's body is invisible, only her head and backpack show. Fine with the companion off ("Ellie twins"). Gone when the game's CPU affinity is set to half the cores.
- mantismeraklisi: Ellie spawns with an invisible body, then the game crashes to the Naughty Dog error report (Fluffy or manual install; ReShade + OptiScaler installed). The log they posted is the same game build (`0x685BEBE1`) and stops after the scan line; probably only the top of the file. Full log asked for.

Cause (from the code; not reproduced here): the look hook decides "player" by the thread that runs the core player spawn. If the game resolves the player's look again on a worker thread (more likely with many cores, which the affinity workaround confirms), the player's outfit was turned into Joel's like the companion's.

Change: the look the mod gave the player at spawn is never changed, on any thread (logged as "resolved outside the player spawn; kept as the player's"). A companion wearing that same outfit stays Ellie (twins) in that case.

Builds:
- `release\test\PlayableEllieWorking-v1.0.7-test.zip` (in Fluffy): 1.0.6-test (bow modes, F11) + this fix. Build SHA-256 `00ad0a3674e92f5f78c7e6537b1e02a6a9de3045369a11d980a4d74339941f70`.
- `release\test\PlayableEllieWorking-v1.0.1-candidate.zip`: 1.0 + this fix only (bow settings off in release builds). `hid.dll` SHA-256 `63d7795f0760fa84cd730a8c1ec3055d915ba605b309d818dda0f78c192be41a`. Not published.

| Test | Result |
|---|---|
| Build, `scan_test` both exes, `proxy_test` (both builds) | All passed |
| Owner's PC: nothing changed (Ellie, Joel companion, 9mm, crafting) | Not tested |
| Reporter's PC: body visible with the companion on | Not tested |

## 1.0.6-test (test build, 2026-10-31): bow on Ellie's back, three modes

Build: SHA-256 `0d2b17ac75268904b7f471608cc99f4cef46d3b56f1cfe495eb13f38f2e76c3e`; zip `release\test\PlayableEllieWorking-v1.0.6-test.zip` (Fluffy name "Playable Ellie (WORKING) 1.0.6 TEST - bow on her back (F11)"), in Fluffy, not ticked.

1.0.5-test in game (owner): **didn't work**. The log shows the holster key was changed ("bow put away on Ellie's back"), but the bow still lay horizontally. Changing only the key isn't enough.

w1z's menu's working live test (chat transcript, 2026-10-30) swapped the three values at `+0x158`, `+0x180` and `+0x198` of Joel's bow record for Ellie's, and the owner saw the bow vertical after a weapon switch. The crouch and invisible bow came after the next change (Ellie's bow overlay table), which isn't used here.

Change: while Ellie plays, Joel's bow record gets Ellie's values. Mode 2 (default) = all three; mode 1 = only the `+0x180` block; mode 0 = Joel's own. F11 (test build) cycles them; switch weapons to see the change. Each field is checked before writing; Joel's values come back for Joel.

| Test | Result |
|---|---|
| Build, `scan_test` both exes, `proxy_test` | All passed |
| Mode 2: bow put away vertically on her back | Not tested |
| Mode 2: aim and shoot, no crouch, bow visible | Not tested |
| Mode 1: same checks (is the smaller change enough?) | Not tested |
| F10 to Joel: his bow in his spot | Not tested |

## 1.0.5-test (test build, 2026-10-31): bow on Ellie's back

Build: SHA-256 `509f1f7c921b415c3e9106970b0e233a91a465001b21183d9983cbe80f1bbbe0`; zip `release\test\PlayableEllieWorking-v1.0.5-test.zip` (Fluffy name "Playable Ellie (WORKING) 1.0.5 TEST - bow on her back"), in Fluffy's Mods folder, not ticked. 1.0's code plus one change.

Owner (2026-10-31): with 1.0's code everything is good except the bow, which lies horizontally across Ellie's backpack when put away; Joel's bow does that only on Ellie.

Change: while Ellie plays, Joel's bow uses Ellie's own holster setting (`s_ellie-bow-slot1-holster`) instead of `s_bow-slot1-holster`. It is the key at `+0x40` of the bow record's `+0x180` settings block, the setting that put the bow vertically on her back in w1z's menu's live test. Only that key changes; the other bow settings (which made her crouch) don't. Found by signature: the `t1x-bow` and `t1x-bow-ellie` weapon records (ProcessWeaponBow hash, name), each must hold the expected holster key, or the change is skipped with a log line. Restored for Joel.

| Test | Result |
|---|---|
| Build (`/W4 /WX`), `scan_test` both exes, `proxy_test` | All passed |
| Both bow records and keys in the running game (read-only, `artrecs.py`, earlier session) | Found, values as expected |
| Log: "bow holster found", "bow put away on Ellie's back" | Not tested |
| Bow put away: vertical on her back, no clipping | Not tested |
| Bow aiming and shooting unchanged (no crouch, bow visible) | Not tested |
| F10 to Joel: his bow in his usual spot | Not tested |

## Back to 1.0 (2026-10-31)

1.0.4-test in game (owner, 2026-10-31): the bow freeze after a shot is **not fixed**, and the bow now sits horizontally when put away. Dropping `C93AB1770308D9FF` from the bow's table is the likely cause of the horizontal bow (1.0.4's only bow change; w1z's menu 1.2.0, which leaves the bow alone, shows it on her back).

The owner asked to do what w1z's menu does. The menu 1.2.0 changes only the 9mm (revolver copy + Ellie's hold) and never touches the bow, which is exactly 1.0. So the 1.0.3/1.0.4 table changes were taken out of the code (saved as `release\test\1.0.4-test-stuck-arms-tables.patch`); the source is 1.0 again and the `v1.0.0` zip in Fluffy is the build to use. Bow freeze, 9mm auto-switch freeze and 9mm twitch near cars: still open, same as in the menu.

Found for later (not built): the vertical bow holster in the menu's reverted test came from Ellie's holster setting `s_ellie-bow-slot1-holster` at `+0x40` of the bow record's `+0x180` block (Joel's bow has `s_bow-slot1-holster`). Live tool: `reference\research-scripts\artrecs.py` (read-only, finds both bow records).

## 1.0.4-test (test build, 2026-10-30): bow freeze, 9mm freeze, 9mm twitch

Build: SHA-256 `6a5cf6525f0e498cfa50012fc292f3de015b791a9e6f3562bb53cb3426f93cac`; zip `release\test\PlayableEllieWorking-v1.0.4-test.zip` (Fluffy name "Playable Ellie (WORKING) 1.0.4 TEST - bow freeze + 9mm twitch"), in Fluffy's Mods folder, not ticked.

1.0.3-test in game (owner, 2026-10-30): the log shows both of its tables applied, but **neither bug fixed**. Claude watched the owner play, using screenshots only:
- After a bow shot Ellie stands with the bow arm held stiffly out to the side.
- Next to the van, the 9mm flips between a raised pose and the aim from one frame to the next.

New candidates (offline):
- **Bow and 9mm freeze:** key `0xC93AB1770308D9FF`.
  - Ellie's own bow and pistol tables never override it.
  - Joel's bow table for her points it at its own animation; the revolver copy points it (and `0x59039FE9E0EDD04D`) at `0xA7D0A310BFA04EC9`.
  - The 9mm's own table, which doesn't freeze, points it at the scene hold, which is loaded (that's the tilt).
  - Change: dropped from the bow's copy and from the 9mm's copy (both keys). 1.0.3's two keys are back in.
- **9mm twitch:** the revolver copy has no `ellie-pistol-blocked--add` entry. The 9mm's copy now gets the entry from Ellie's own pistol table (`0x20230410BB70BC08`), which goes with the hold it already uses.

F11 (test build) turns all of it off and on. Offline: the build has no warnings, and `scan_test` (both exes) and `proxy_test` all passed. In game: **Not tested**.

## 1.0.3-test (test build, 2026-10-30): stuck arms after a bow shot

Build: `build\hid.dll` SHA-256 `6c7b35a79dec51b6761e2b61b3690ee238530eeed8307cb90283708c5806aa9c`; zip `release\test\PlayableEllieWorking-v1.0.3-test.zip` (Fluffy name "Playable Ellie (WORKING) 1.0.3 TEST - stuck-arms fix"), in Fluffy's Mods folder, not ticked.

Reported (owner): after a bow shot Ellie freezes with her arms out, like the 9mm auto-switch freeze; and near cars (and maybe other objects) the arm and gun twitch until she steps away.

Root-cause findings (offline, from the DC files):
- Joel's bow table for Ellie (`t1x-bow`, 47 entries) and the revolver table (the mod's 9mm table) both override keys `0xB88A762D597FB619` and `0x4DFCECDF96C13751`.
- The global player overlay (`player-overlays`) maps those two keys to `0x752038C1F2711CA1` and `0x41D505D4968C6E89`. Ellie's own bow and pistol tables, and the 9mm's own table (no freeze), never override the two raw keys.
- So both freezes come from the same two entries (not confirmed in game).

Change: while Ellie plays, the 9mm and Joel's bow use copies of their tables without those two entries; everything else is unchanged, including the 9mm's straight hold. The bow table is found by signature (first entries plus its `{table, count}` reference 0x60 before it); in the running game at the title screen there was exactly one match. F11 (test build only) turns the fix off and on. The rejected 9mm tables 2-6 were removed. The shotgun and rifle tables also have the two keys but were not touched.

Close-range twitch: not reproduced and not changed. The mod's 9mm table has no `ellie-pistol-blocked--add` entry, so a blocked 9mm uses the game's default. Next step: in game, check which guns twitch and whether Joel (F10) does it too.

| Test | Result |
|---|---|
| Build (`/W4 /WX`), `scan_test` both exes, `proxy_test` | All passed |
| Bow table found in the running game (title screen) | Yes, one match |
| In game: launched for testing while the owner was away, but control of the game window was not approved, so no gameplay test | Not tested |
| Bow: one shot, several shots, aim-shoot-aim, crouched, move after the shot, last arrow, zero arrows, pick up arrows | Not tested |
| 9mm runs dry, the game switches weapons: arms not stuck; 9mm still straight | Not tested |
| F11 off: freezes come back (proves the cause) | Not tested |
| Near cars, walls and furniture with each gun; compare with Joel | Not tested |

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
