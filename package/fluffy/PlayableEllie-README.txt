Playable Ellie (WORKING) v1.1 - The Last of Us Part I (PC)
=============================================================

Play Joel's chapters as Ellie, done properly:
  - she moves, aims, shoots, reloads and interacts (her animations are
    streamed in before a checkpoint restart),
  - crafting works in every chapter, with all recipes,
  - Joel's 9mm sits straight in her hands, with recoil,
  - Joel follows her as the AI companion (optional, on by default),
  - she wears the outfit Ellie wears in that chapter (or one you choose),
    and the Ellie skins you pick in Extras > Skins,
  - every Extras > Skins outfit and weapon skin is unlocked and owned
    (optional, on by default),
  - Joel's bow sits vertically on her back,
  - works with ReShade and OptiScaler.

New in 1.1
  - Works with ReShade and OptiScaler: the mod's on-screen messages turn
    themselves off when either is installed (both drawing crashed the game).
  - Fixed: Ellie's body or arms invisible on some PCs.
  - Fixed: invisible companion Joel ("walking flashlight").
  - Fixed: crash in Extras > Skins, and Joel shown in the Ellie preview.
  - New: every Extras > Skins skin unlocked (UnlockSkins=1).
  - New: the bow sits vertically on her back (BowOnBack=1).

Install
  Fluffy Mod Manager: put the zip in Fluffy's Games\TLOU\Mods folder and
  tick it (untick any older version).
  By hand: copy hid.dll next to tlou-i.exe
  (Steam > right-click the game > Manage > Browse local files).
  Uninstall: deactivate in Fluffy, or delete hid.dll.

Use
  Ellie is on by default. Load a save of one of Joel's chapters and you
  play as Ellie. If her animations are not loaded yet, the mod loads them
  and restarts from your last checkpoint once (progress since that
  checkpoint is lost, as with Pause > Restart Checkpoint).
  F10 (in game) switches between Ellie and the story character and
  restarts from your last checkpoint as that character.

Settings
  Documents\Playable Ellie (WORKING)\PlayableEllie.ini (made on first launch,
  kept when you update; new keys are added). Changes apply the next time the
  game starts. PlayAsEllie, ToggleKey (121 = F10), JoelCompanion, Outfit
  (0 = the chapter's outfit, 1-11 = a fixed outfit; an Ellie skin from
  Extras > Skins is drawn over it), GiveRecipes, Fix9mm, Notifications
  (on-screen messages; off by itself with ReShade/OptiScaler), Sounds
  (beeps), Logging, UnlockSkins (every Extras skin), BowOnBack (bow on her
  back).

Known issues
  - After some cutscenes that change Joel's outfit (e.g. he puts his
    backpack on in Pittsburgh) you play as Joel again. Press F10 twice.
  - When the 9mm runs dry and the game switches weapons by itself, Ellie's
    arms stay out until you aim. Aim once. Fix9mm=0 avoids it, but the 9mm
    then tilts up in her hands.
  - After a bow shot or a reload she can stand with her arms out for a
    moment; aiming or moving brings her back.
  - Close to cars and walls her gun or arm can twitch; step back a little.
  - Shotgun and hunting rifle twitch in from the right when aiming.
  - Companion Joel speaks Ellie's lines in her voice.
  - With ReShade or OptiScaler there are no on-screen messages (Sounds=1
    beeps instead).

Fixing a problem yourself
  You are Joel again ............ F10 twice
  Ellie's body is invisible ..... Outfit=0 (default); or try Outfit=1
  Companion Joel looks wrong .... JoelCompanion=0
  Crash with an overlay tool .... Notifications=0
  Outfit= does nothing .......... restart the game; Ellie skin = Default
  Bow looks wrong ............... BowOnBack=0
  Skins menu looks wrong ........ UnlockSkins=0
  Arms out after the 9mm ........ aim once; or Fix9mm=0
  Anything else ................. Pause > Restart Checkpoint or F10 twice,
                                  then send PlayableEllie.log as a file.

Log
  PlayableEllie.log next to tlou-i.exe says what was found and what the mod
  did. Please send it (the file, not copied text) with bug reports.

Other mods
  w1z's menu already has the playable Ellie (Character page). If it is
  installed (winmm.dll), this mod steps aside and changes nothing.
  TLOU Outfit Changer: this mod leaves the skin unlock to it when it is
  installed. Works next to TLOU Infinite Ammo.

Notes
  Single-player only. The mod never writes save or profile files itself;
  the recipes it teaches Ellie are saved by the game at its next checkpoint.
  Everything is found by signature in the game's code; if a game update
  changes something, that part switches itself off and the log says why.
  Made for Steam version 1.1.4.0 (tlou-i.exe and tlou-i-l.exe).

By w1z. MIT licence. Uses MinHook and Dear ImGui.
