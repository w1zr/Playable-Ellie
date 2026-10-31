![Playable Ellie (WORKING)](nexus/images/00-header.png)

# Playable Ellie (WORKING)

A mod for **The Last of Us Part I** (PC, Steam 1.1.4.0) that lets you play Joel's chapters as **Ellie, done properly**:

- she moves, aims, shoots, reloads and interacts: her animation packages are streamed in by the game before a checkpoint restart;
- crafting works in every chapter, with all recipes;
- Joel's 9mm sits straight in her hands, with recoil;
- Joel follows her as the AI companion instead of a second Ellie (optional, on by default);
- she wears the outfit that matches the chapter, or one you pick.

It is the playable Ellie from [w1z's menu](https://github.com/w1zr/w1z-menu) 1.2.0, packaged on its own for players who only want Ellie. If w1z's menu is installed, this mod steps aside (the menu's Character page already does all of this).

**Version 1.0.** Download: [GitHub releases](https://github.com/w1zr/Playable-Ellie/releases/latest). Tested in game; see [TEST_REPORT.md](TEST_REPORT.md).

## Install

- **Fluffy Mod Manager:** put `PlayableEllieWorking-v1.0.0.zip` in Fluffy's `Games\TLOU\Mods` folder and tick it. Untick it to turn the mod off.
- **By hand:** copy `hid.dll` next to `tlou-i.exe` (Steam → right-click the game → Manage → Browse local files).

To uninstall, untick it in Fluffy or delete `hid.dll`. Don't install it both ways at once: Fluffy would treat the hand-copied `hid.dll` as a game file and put it back on uninstall.

## Use

Ellie is on by default. Load a save of one of Joel's chapters and you play as Ellie: her animations load with the chapter. If they are not in yet (for example the game was already running when the mod found its data), the mod has the game stream them in and restarts from your last checkpoint once. A small message at the bottom left shows what is happening. Progress since that checkpoint is lost, as with Pause → Restart Checkpoint.

**F10** in game switches between Ellie and the story character and restarts from your last checkpoint as that character. While it works, a message with a spinner shows "Loading Ellie's animations... 50 / 51" with a progress bar, then "Restarting checkpoint...", then "Swapped to Ellie" (or "Swapped to Joel"). F10 is ignored until the swap is done.

## Settings

`Documents\Playable Ellie (WORKING)\PlayableEllie.ini`, created on first launch and kept across updates ([defaults](dist/PlayableEllie.ini)):

| Key | Default | |
|---|---|---|
| `PlayAsEllie` | 1 | Play Joel's chapters as Ellie (F10 changes it and saves it here) |
| `ToggleKey` | 121 | Windows virtual-key code of the toggle key (121 = F10, 0 = none) |
| `JoelCompanion` | 1 | The story companion Ellie becomes Joel while you play as Ellie |
| `Outfit` | 0 | 0 = match the chapter, 1-11 = costume 01, 02, 03, 04, 05, 06, 07, 08, 11, 13, 15 |
| `GiveRecipes` | 1 | Teach Ellie every crafting recipe after she spawns |
| `Fix9mm` | 1 | Joel's 9mm sits straight in her hands (0 = the game's own: tilts up, but no freeze on the automatic weapon switch) |
| `Notifications` | 1 | On-screen messages (loading, swapped, errors) |
| `Sounds` | 0 | Also beep for the toggle key and the swap |
| `Logging` | 1 | Write `PlayableEllie.log` next to the game exe |

## Known issues

- When the 9mm runs dry and the game switches weapons by itself, Ellie's arms stay out with no animation until you aim. Aim once to fix it. `Fix9mm=0` avoids it, but then the 9mm tilts up in her hands (the game's own table for her is made for a scripted scene).
- Bow: Ellie's arm clips through it when it is put away.
- Shotgun and hunting rifle twitch in from the right when aiming.
- Companion Joel speaks Ellie's lines in her voice (voices belong to the companion).

## How it works

Everything is found by signature in the game's code (`src/resolve.cpp`, `src/engine_defs.h`); a part that isn't found switches itself off and the log says why. The mod loads as `hid.dll`, a DLL the game imports and no other TLOU mod uses, and forwards every `hid.dll` function to the real one in System32.

| Feature | How |
|---|---|
| Ellie as the player | Hook on the core player spawn (found from the developer menu's Spawn Player): `t1x-joel-costume-NN` becomes the matching `t1x-ellie-costume-NN` |
| Her animations | Joel's chapters don't load them. The chapter's player level set (`joel-player-all` / `-no-explosives`) is patched in memory to also list Ellie's sets and `t1x-anim-ellie-pistol`; the game's own want-load rebuild (found from the `LoadLevelSet` console command) streams them in; when they're loaded, the checkpoint restarts |
| Crafting | The game only lets an Ellie player craft in Winter or when a game flag is set; the mod sets that flag while Ellie plays, and runs the developer menu's All Recipes after she spawns |
| 9mm | While Ellie plays, Joel's 9mm uses a private copy of the revolver's Ellie overlay table whose two hold entries use Ellie's own pistol hold |
| Joel companion | Hook on the game's look resolver: Ellie looks resolved outside the player spawn become Joel's; Joel's companion animations are added to the level-set patch |
| Messages | Dear ImGui drawn right before each DirectX 12 Present (the overlay from w1z's menu); no ImGui work at all while no message is on screen |

## Building

Visual Studio 2022 Build Tools (C++ workload). From a command prompt: `build.bat`, then `package.bat` (or `package.bat test`). `python tools/make_art.py` redraws the logo, the Fluffy preview and the Nexus header. Offline tests:

```
build\scan_test.exe "<game folder>\tlou-i.exe" "<game folder>\tlou-i-l.exe"
build\proxy_test.exe
```

`tools/gen_hid_proxy.py` regenerates the export stubs from System32's `hid.dll` (needs `pefile`).

## Credits

By w1z. Uses [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-clause) and [Dear ImGui](https://github.com/ocornut/imgui) (MIT). MIT licence.
