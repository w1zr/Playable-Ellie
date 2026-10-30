Playable Ellie (WORKING) v1.0 - The Last of Us Part I (PC)
=============================================================

Play Joel's chapters as Ellie, done properly:
  - she moves, aims, shoots, reloads and interacts (her animations are
    streamed in before a checkpoint restart),
  - crafting works in every chapter, with all recipes,
  - Joel's 9mm sits straight in her hands, with recoil,
  - Joel follows her as the AI companion (optional, on by default),
  - she wears the outfit that matches the chapter (or one you choose).

Install
  Fluffy Mod Manager: put the zip in Fluffy's Games\TLOU\Mods folder and
  tick it.
  By hand: copy hid.dll next to tlou-i.exe
  (Steam > right-click the game > Manage > Browse local files).
  Uninstall: deactivate in Fluffy, or delete hid.dll.

Use
  Ellie is on by default. Load a save of one of Joel's chapters and you
  play as Ellie. Her animations usually load with the chapter; if they are
  not in yet, the mod loads them and restarts from your last checkpoint once
  (progress since that checkpoint is lost, as with Pause > Restart
  Checkpoint). A small message at the bottom left shows what
  is happening.

  F10 (in game) switches between Ellie and the story character and restarts
  from your last checkpoint as that character. The message shows "Loading
  Ellie's animations..." with a progress bar, "Restarting checkpoint...",
  then "Swapped to Ellie" or "Swapped to Joel". F10 waits until it's done.

Settings
  Documents\Playable Ellie (WORKING)\PlayableEllie.ini (made on first launch,
  kept when you update). PlayAsEllie, ToggleKey (121 = F10), JoelCompanion,
  Outfit (0 = match the chapter, 1-11 = a fixed outfit), GiveRecipes,
  Notifications (on-screen messages), Sounds (beeps, off), Logging.

Log
  PlayableEllie.log next to tlou-i.exe says what was found and what the mod
  did. Please include it with bug reports.

Known issues
  - Bow: Ellie's arm clips through it when it is put away.
  - Shotgun and hunting rifle twitch in from the right when aiming.
  - Companion Joel speaks Ellie's lines in her voice.
  - Not tested yet: cutscenes, chapter transitions, Ellie's own chapters
    (Winter) with the mod on, playing a whole chapter through.

Other mods
  w1z's menu already has all of this (Character page). If it is installed
  (winmm.dll), this mod steps aside and changes nothing.
  Works next to the TLOU Infinite Ammo mod and the TLOU Outfit Changer
  (different DLL names); not tested together yet.

Notes
  Single-player only. The mod never writes save or profile files itself;
  the recipes it teaches Ellie are saved by the game at its next checkpoint.
  Everything is found by signature in the game's code; if a game update
  changes something, that part switches itself off and the log says why.
  Made for Steam version 1.1.4.0 (tlou-i.exe and tlou-i-l.exe).

By w1z. MIT licence. Uses MinHook and Dear ImGui.
