# Status

Updated at the end of every session. Only record what a build actually did.

## Current state (2026-10-21)

| Area | State |
|---|---|
| Repository | scaffolded from SSARecomp: build, app, portal (emulated, USB, hybrid), portal / player / settings menus, input scripts and test tools; none of its game hooks or file readers |
| Game data | disc extracted into `rom/` (988 files), `default.xex` hash pinned |
| SDK | our fork `tsapZ/rexglue-sdk`, branch `skylanders`, shared with SwapForceRecomp and SSARecomp |
| Codegen | works: 188 files; four jump tables needed their real size (config/default.toml) |
| Boot | ✅ to the title screen ("Press A to begin") in about 15 s, logo movies skipped |
| Menus | ✅ autosave notice, Choose Play (Story, Battle Mode, Settings), game slots, difficulty |
| In-game | ✅ new game: the in-engine intro with Eon's voice plays through, the portal prompt, Chapter 1 with the HUD |
| Portal | ✅ emulated portal: Scarlet Ninjini placed at the prompt, her portal sequence, then play |
| Portal menu | ✅ opens by itself at "Please put a Skylander", closes when a figure is on; the portal check opens on "too many Skylanders"; nothing in the pause menu. Every Skylander, sidekick and magic item shows the game's collection portrait, the element filters the game's symbols; magic items have the game's names; the details view shows each Skylander's upgrades as the game's upgrade screen does (names, descriptions, prices, pictures) |
| Languages | English, German, French, Spanish (Latin American), Italian: menus and subtitles checked; German and Italian through a region hook |
| Frame rate | 30 FPS by default as on the console; `frame_rate_limit` (F4 > Bildrate) gives 60.0 and 143.9 FPS on the title screen, 96-97 FPS in the first level with 144 (scale 2); walking speed and the intro's pace unchanged |
| Audio | works; the game mixes real 5.1: voices on the center, bass effects on the LFE, the rear channels in use (measured 2026-10-20 with a temporary per-channel peak log of the guest's 6-channel render frames). With Windows' output device set to 5.1 the SDK passes the six channels through, otherwise it folds them to stereo |

## 2026-10-20: session log

- **Own repository** (the owner's call: every game done by us, each playable on its own and later
  through the hub). TheBiemGamer's GiantsRecomp (MIT) is read for reference and credited; nothing
  of it is copied without checking it against this executable.
- **Scaffold:** SSARecomp's tracked files, renamed; its Spyro's Adventure-specific parts left out.
- **Disc:** Alchemy engine like Swap Force, archives in an older IGA version:
  docs/game/giants.md. TheBiemGamer's GiantsRecomp pins the same `default.xex`, so his
  addresses apply; each is checked here before use.
- **Codegen and build:** worked at the first try (188 files). The first boot ended with
  0xC000001D right after the Activision logo: the generated switches of four jump tables in the
  Bink bit-stream decoder trapped (codegen saw 4, 8, 4 and 8 entries). Guest memory shows 11 code
  addresses in each (input script `dump`); with those in `config/default.toml` (also TheBiemGamer's
  finding) the game reaches its title screen.
- **Intro skip** (`skip_intro`, on; `skip_opening_movie`, off): his BinkOpen hook address
  (0x82153DB0) checked in the generated code (the "KB2e" compare after the header read into
  r1+96), the logos' header values checked against the disc; the opening movie is `FMV01.mov`.
- **Menus and the first level:** title, autosave notice, Choose Play, game slot, difficulty; with
  the opening movie skipped the in-engine intro plays through with Eon's narration (so the XMA
  fix carries over), then "Player 1: Please put a Skylander on the Portal of Power". Scarlet
  Ninjini placed on the emulated portal (`place`, a copy) came in with her portal sequence and
  Chapter 1 "Time of the Giants" started with her HUD. No further codegen fixes were needed so
  far.
- **Languages:** the disc is the North American one: English, French, Spanish (title screen
  under each setting); German and Italian came out English at first.
- **German and Italian after all.** The archives carry twelve language packs (GERMAN.pak,
  ITALIAN.pak, ... in global.bld and every character archive) and voice files in the same
  languages (`.../german/vo_*.wav`, Eon's narration and hints too). The game picks its language
  set by console region (`sub_8217F2D8`, the tfbXenonApplication setup): XGetGameRegion 0x201 or
  0x2FE-0x2FF gives the European set, anything else (the SDK returns 0xFFFF) the North American
  one, and a language outside the set becomes English. A midasm hook right after that call
  (`src/hooks/language_region_hook.cpp`, 0x8217F350) reports 0x2FE when German or Italian is
  chosen. Checked: title, autosave notice, Choose Play and the game slots in German, Choose Play
  in Italian, the first level's subtitles ("Eon: Doch die eiserne Herrschaft der Arkeyaner...")
  and the prompt ("Spieler 1: Bitte setze einen Skylander auf das Magische Portal.") in German;
  English and Spanish unchanged. Whether the voices are German is for a listen (the files are
  there). The language choice is back to five, "auto" included.
- **Frame rate.** The game measured 30.0 FPS on the title and in the first level. Its D3D swap
  queue (`sub_823D9758`, the same 107 instructions as Swap Force's `sub_82F5C258`) already gets
  presentation interval 1 (swap word 0x100, logged by a test hook), so unlike Swap Force no hook
  is needed: the 30 FPS are the SDK's emulated vblank, which runs at 30 Hz without
  `frame_rate_limit` and at the limit's rate with one. Measured: 60.0 FPS with 60, 143.9 with
  144 and 180 with 200 on the title screen; 60.0 and 96-97 in the first level (scale 2). Game
  speed: holding forward for 2 s in the first level ends at the same spot at 30 and 144 FPS, and
  the intro's subtitles keep their pace (screenshots every 4 s); only the loading screen before it
  finishes about 4 s sooner at 144 FPS. The setting is `frame_rate_limit` on F4 > Bildrate,
  0 (30 FPS) by default.
  Three minutes of scripted play in the first level at 144 (walking in every direction,
  attacks, jumps): a steady 103-108 FPS, coins and the tutorial prompts work, no errors.
- **Menus and saves, scripted:** the pause menu's Chapter Select ("1: Zeit der Riesen"),
  Objectives, Collections and Settings open and close; Continue on save slot 1 ("Skylander: 3")
  loads, back at Chapter 1's start at 0 %. Battle Mode asks for a controller per player; the
  input script is one controller, so it waits for a test with two pads.
- **The game's pictures in the portal menu** (`src/game/toy_portraits.cpp`). The archives are
  IGA version 8 (little-endian; `src/game/alchemy/iga8_archive.h`), the scenes in them IGZ
  version 6 (`src/game/alchemy/igz6_file.h`: numbered fixup records instead of fourccs, pointers
  with the pool in the top byte, igImage2 with its name at +0x08). Each Skylander's scene archive
  `character/NNN_<model>[_<deco>].bld` (NNN = its figure id) holds the collection screen's
  portraits, 256x256 DXT5: plain, `_series2`, `lightcore`, and the Legendary / alternate deco
  models in archives of their own. Indexed over every character archive with a Python prototype
  first; the menu picks by the catalog variant (Series 2, LightCore, alternate deco 3 =
  Legendary, other alternate decos = `_Alt` / `_Red`), and Spyro's Adventure's own variant ids
  (4xx Legendary, 28 Dark Spyro). 100 portraits from 69 archives load in 0.6 s on a background
  thread, decoding only the archive blocks the portraits and the scene's tables are in. Checked
  in the menu: Double Trouble, Royal Double Trouble, Scarlet Ninjini, Ninjini, Chop Chop, Spyro
  (Series 1 and 2), Voodood, Wrecking Ball (Series 1 and 2), Pop Fizz. The element symbols come
  from `CardGame_Streaming.bld`. Sidekicks and magic items have no portrait in their own scenes,
  but the collection screen's scene (`misc/UI_Collection_Champions.bld`) has all 24 by name:
  "<Skylander>_sidekick" (sidekick id = 500 + the Skylander's id, Giants' four from a table) and
  "magicitem_<name>" (a table by id; the golden Dragonfire Cannon has its own). Checked in the
  menu: Mini Jini and all twelve Spyro's Adventure items. The character scenes also name the
  upgrade pose pictures (`Cynder_Up1.png` ...), which sit as separate textures in the character's
  `.arc`: material for a details view later.
- **The game's names for magic items and adventure packs** (`src/game/game_text.cpp`), in the
  language the game runs in. A scene's StringInfo objects name its text keys; the archive's
  language pack (`GERMAN.pak`, ..., IGZ 6 with a LanguagePackInfo) holds the texts as UTF-16BE in
  the same order (checked on global.bld: 908 keys, 908 texts). Each item archive's first
  `..._String` text is its name; in the menu, German: "Ambossregen", "Düstergruft",
  "Drachenfeuer-Kanone", "Skorpion-Schläger" and the rest. The golden Dragonfire Cannon, the
  Platinum Treasure Chest and the sidekicks keep the catalog's names (the game calls Barkley
  "TreeRexling"). The character scenes have each Skylander's upgrades in the same way
  (`032_Name(00)` "Cynder-Flug" ... `(11)` the Wow Pow, `032_Upgrade_PathA` with descriptions).
- **Upgrades in the details view** (`src/game/toy_upgrades.cpp`), like Swap Force's. The texts
  come from each Skylander's scene, whose keys are spelled by hand in many ways (counted over all
  48: `UpgradePath1Name`, `Upgrade_PathA`, `UpgradeBranch_Left`, `Action1PowerName`,
  `PrimaryAttackName`, ...); the layout is always 00-03 basic, 04-06 first path, 07-09 second
  path, 10 Soul Gem, 11 Wow Pow (Spyro's Adventure's Skylanders only, shown for Series 2). The
  prices are the global scene's `UpgradeCost_(01)` ... `(09)`: 500, 700, 900, 1200; 1700, 2200,
  3000; 4000; 5000. The bits follow the game's `pp_Flags1_UpgradeBits` field list, the same as
  Swap Force's. The pictures are the 512x360 upgrade poses in the character's `.arc`, stored upside
  down, read when the view first shows them. The texts' button glyphs `[` `]` `{` become X, Y, B.
  The card game's element symbols are model textures too and were upside down in the menu
  (seen by the owner): both are turned now; the collection portraits are UI sprites and come
  the right way up.
- **Hats in the details view** (`src/game/toy_hats.cpp`): the hat number a figure stores is the
  game's kTfbSpyroTag_Hat_* (Swap Force's executable has the enum), and Giants' own data follows
  it with number - 1: the store archives `misc/StoreItem_Hat_NNN.bld` (icon, bonuses as text)
  and the global scene's `HatName(kk)` (all 37 names of Spyro's Adventure's hats checked against
  the enum, German and English). 96 names and 52 hats with picture and bonuses load in the
  background. Checked with a test copy of Chop Chop given hat 1: "Kampfhut", the army helmet,
  "+15 Elementarkraft".
  Checked in the menu: Double Trouble ("Kanalisierer" / "Beschwörer", "Erweiterter
  Geisterstrahl", 1700 Gold) and Scarlet Ninjini ("Schwerter der Macht" / "Alte Dschinn-Magie",
  "Wunschklingen-Kombos: Drücke X, X, HALTE Y für Klingenschwung"). Not yet seen with a figure
  that has upgrades bought (the test copies have none).
- **Portal menu by itself at the prompt** (`src/game/portal_prompt_watch.cpp`). Giants has no
  CMagicMoment (Swap Force) and no prompt object like Spyro's Adventure: its portal screens are a
  game script, the "Portal Control" state machine. The text "Please put a Skylander" is read
  once into a widget, so a read watch on it found only the text loader. Guest memory snapshots
  (new input script command `save`) at the prompt, in play, in the pause menu, in a cutscene and
  on the title, in two runs, showed that the script heap is laid out the same every time, and that
  two named script variables change exactly with the prompt: "portal error string" (0, or the
  text key on screen, `203_PortalError_TagTypeNotOnPortal`) and the state machine's "current
  state name" (`Portal Control - Portal - 0 - Idle P1 Only` at the prompt). The application's
  pause stack (tfbXenonApplication) changes for the pause menu too, so it can't tell them apart.
  The watch finds both variables by class and name, then polls them. Checked by script: the menu
  opens at the first prompt and again after the figure was taken off, closes when Ninjini is
  placed; a second Skylander gives `102_PortalError_TooManyMinions` and the portal check; the
  pause menu, the title and the autosave notice open nothing. Player 2 (input script `player 2`,
  Start, A) gets the prompt with the state `... Idle P2 Only`, and the menu opens for player 2;
  Stealth Elf placed comes in as player 2 next to Ninjini, and taking him off asks player 2
  again.
- **Input script:** `save` (guest memory to a file, for snapshot comparisons), `remove <slot>`,
  and addresses as `hit<n>` (a hit of the last scan) or `*<addr>` (through a pointer), for data
  whose heap address changes from run to run.
- **Repository brought to SwapForceRecomp's level:** GitHub milestones per phase and an issue
  for every step, done ones closed with their result, the open ones as the plan; a play test
  issue for the owner; docs/roadmap.md with issue numbers, docs/README.md, docs/portal.md and
  docs/menus.md; the README's status table.
- **Saves survive a crash in the middle of saving** (SDK fork `fix/save-commit-on-close`, found in
  SwapForceRecomp, where a crash while saving left an empty save): the game saves by recreating its
  package (`Save_Slot_1` and `Save_Header`: create, write, close within 0.1 s), and the old one now waits in
  `<user data>/save-backup/pending/` until the new one is closed, then stays in
  `save-backup/previous/`; a start after a crash in between puts it back. Checked on a test copy:
  both saves log "saving ... anew" and "saved".
- **No crash when B closes the portal menu's "real portal is active" message** (portal fork
  8f56b05, found in SwapForceRecomp with the USB portal).
- **Crash report in the log** (`src/dev/write_watch.cpp`, `InstallCrashReport`, from
  SwapForceRecomp): a real crash logs the guest call chain and registers ("Crash: ...").
- **No speed-up above 120 FPS** (#22). The Cemu packs for the Wii U version patch a clamp against
  a "speed-up above 120 FPS". The game clock here (`sub_822F18D8`, once per frame) has such a
  clamp, a minimum time step at +0x10 of the clock, but its minimum is 0 in this executable, so it
  never raises a step. Measured with a new midasm hook at its compare (`src/hooks/game_clock_hook.cpp`,
  developer setting `game_clock_log`: game time against real time every 5 s): speed 1.000 at
  230-240 FPS (resolution scale 1, title and level start) and at 125-130 FPS in play. The hook
  changes nothing and stays as the measuring tool.
- **SDK: vertex data the CPU rewrites in place is uploaded again** (fork `fix/vertex-buffer-cpu-writes`,
  found in SwapForceRecomp, where the second logo movie drew with the first one's texture
  coordinates), and **a rebuilt GPU plugin is copied next to the exe** without a relink (fork
  `fix/stage-gpu-plugin`). Checked: boot with the logo movies, no errors. New RenderDoc script
  `tools/renderdoc/vsout.py` (vertex shader outputs per vertex).

## 2026-10-21: session log (night)

- **Player menu with two players** (portal library, #56 in SwapForceRecomp): Player 1, Player 2
  and "Not playing" (guest users 2 and 3, e.g. a third controller) instead of four player columns;
  left / right moves a controller through the three. Checked by script.
- **Controls page for every device** (#55 / #29): the game's own Xbox 360 controller picture stays
  for Xbox pads; any other pad (PlayStation, Nintendo, a generic one like the Logitech Dual Action),
  and a game without a picture of its own, gets a drawn controller in its layout and with its
  button names. Used with the keyboard (F4, or a key pressed in the menu) the page shows a keyboard
  (QWERTZ in German): a button setting marks its key (F6, F5, F9, F4), the overview every menu key
  and the keys of the keyboard's controller emulation (`keybind_*`) with the button each one
  stands for ("Laufen" on WASD, "A" on Space). Checked by script on both views.
- **Achievements like on the Xbox 360** (#59): an unlocked one pops up in the bottom middle (the
  round icon opens into a dark bar, "Erfolg freigeschaltet", "25G - name", no sound), and the list
  opens with F7 or from F4 > Spiel > Erfolge with a controller (unlocked first with the date,
  totals; up / down, LB / RB, B). Both replace the SDK's own.
- **English checked** (title, portal menu, details, settings) with `--language=english`.
- **Portraits the right way up** (the owner saw them upside down): every collection
  portrait (Skylanders, sidekicks, magic items) is stored upside down like the element symbols;
  flipped when read. The hat icons, UI sprites, were right.
- **Hat bonuses with their symbol** (#58 in SwapForceRecomp): the stats screen's pictures
  (character/UI_StatsScreen_Extra.bld), matched to each bonus by the same text in ENGLISH.pak.
- **The game's gold symbol** (#57 there): the stats screen's coin with the Skylanders S
  (permanent/global.bld `.../ui_main/sprites/stats/coin.png`).
- **Ultrawide** (#19; src/hooks/ultrawide_hook.cpp, `ultrawide_fov`, on): a window wider than 16:9
  (`resolution`) shows more to the sides instead of black bars, as in SwapForceRecomp. The
  perspective builder (sub_822D0240) found by Swap Force's instructions in a dump of this game's
  code (42 of its first 48 the same); its four callers probed in the first level: 0x82198530 every
  frame with the camera's FOV, 0x82198580 every frame 40 degrees wider (presumably culling),
  0x82199988 / 0x821999B0 every third frame. All four get the wider aspect and FOV (the vertical
  FOV kept), so nothing the wider view shows is culled; the app turns the letterbox off. Checked
  by script in a 2560x1080 window: Chop Chop in his proportions, more of the level at both sides
  (the nest on the left, the cliff on the right), nothing missing at the edges; the HUD stretched.
- **The game's fonts in the portal menu** (src/game/game_fonts.cpp, #24): the titles in the UI's
  display font "contemp" (permanent/global.bld, size 85), the text in the plain "simple"
  (permanent/bootstrap.bld, size 45), both igBitmapFont in IGZ version 6 with the later games'
  layout; their pages are DXT5 with the coverage in the alpha. 177 glyphs each, German's umlauts
  and ß among them; Segoe UI fills in the rest. Read in 240 ms at start. Checked by script: the
  portal menu's title in the same letters as the game's "Spieler 1:" behind it, names, tabs,
  hints and the hat's "Generalsmütze" in the text font.
- **The HUD at the window's edges** (`ultrawide_hud`, on, with ultrawide): the owner found the
  stretched HUD squashed. The UI is laid out on a screen object of 1280 x 720 that the UI camera
  also takes its orthographic projection from; the hook gives it the window's width at the UI's
  height (1707 for 21:9), so the game lays its UI out to the window's edges and the projection
  keeps its proportions (approach from TheBiemGamer's GiantsRecomp, read, not copied). Checked by
  script in a 2560x1080 window: the portal prompt, the HUD (Ninjini's round portrait at the left
  edge) and the pause menu in their own proportions.
- **Movies on ultrawide** (2026-10-21): the logo movies and the opening movie are 16:9 Bink
  movies; while one plays the presenter shows it with black bars at the sides instead of
  stretched (present_use_actual_aspect off and present_letterbox on for that time, not saved).
  Start: BinkOpen's header hook; end: a hook at the start of BinkClose (sub_82155B08, called only
  from the game's movie close). Checked by script in a 2560x1080 window with skip_intro off: the
  logos 16:9 in the middle, the screen after them fills the window again.

## 2026-10-22: session log (night)

- **Portal library** (83cc22e, 141d32e): the later games' cards say "Fahrzeug", "Sensei", "Falle" and "Serie 2" in the German menu; a plain hat symbol where a game has no hat picture.
- **New issues** for options the other games have: #25 the native 3D scene (its size is neither data nor Spyro's Adventure's constants here; needs a RenderDoc capture), #26 the game's FXAA at the internal resolution, #27 bloom and motion blur strength, #28 the game's controller picture.
- **Giants decompiled** (new private repository tsapZ/GiantsDecomp): readable C++ for some of this
  game's functions, built in only when the CMake cache variable `GIANTS_DECOMP_DIR` points at a
  checkout of it (off by default); each replaces the recompiled function and is checked against it
  with `--decomp_verify=true`. First one: the figure data CRC (sub_82201FD8), "matches the original
  (16777216 inputs)"; a run with it in to the title, then the build set back to without.

## 2026-10-22: later the same night

- **The controls page shows the game's controller picture** (item/UI_Options_Item.arc's controls_360.png; the red callout lines filled in straight across, the red the DXT blocks mixed into the edges turned grey), #28 closed.
- **Both fonts' button symbols left out** ("<" A, ">" B, "]" X, "}" Y, the sticks, Back, Start, the d-pad): the menus drew them as dots where the text has "<" or ">".
- **Native 3D picture** (src/hooks/scene_size_hook.cpp, F4 > Grafik, experimental, off): the scene's target is made in code at the screen's size but at most 1000x600 (sub_821939E0, and the helpers sub_8218ED90 / sub_8218EDD8 for what follows the scene); found with a RenderDoc capture, checked with another: the scene drawn and resolved at 1264x704. #25 closed. #26 (FXAA): the captured scenes use 2x MSAA and no FXAA pass, see the issue.
- The settings example and the installer's template set skip_intro twice (an invalid file); fixed, and the example says 60 FPS.
- **Settings file read even with a repeated key** (SDK fork 8761248): a key set twice in a table makes the whole TOML file invalid, and the game then ignored every setting in it without a visible error (the error went to a log that didn't exist yet). The SDK now reads such a file with the repeats left out and logs which key was repeated.
- **`--settings_schema=<file>`** (skylanders-portal 4521ced): the game writes its F4 menu's pages with their values and quits before a window opens; the Core of Light launcher shows them as the game's settings and writes into this game's settings file.
- **Figure pictures for the launcher** (skylanders-portal e17786b): while the game runs, every figure picture it reads from the disc goes into `Documents\Skylanders\Pictures\<game>` once, for the launcher's figure library; later the same night (skylanders-portal 6c3c6a3) also the hats it knows (hats.txt, their pictures) and the upgrade trees of the figures in the figure folder; and
(skylanders-portal f571d26) the game's achievement list with the title's own pictures, for the
launcher's achievements screen.
- **Bloom strength (F4 > Grafik > Leuchten (Bloom)), as in the other games:** Giants has Toys for
  Bob's own bloom (`tfb_bloom_combine_PS`); `sub_8219C730` draws its combine pass with the effect
  object's `_bloomIntensity` (+52) as the colour of the pass's constants. A midasm hook at the
  first store (0x8219C9E8) scales it (`src/hooks/post_effects_hook.cpp`). Checked on the title
  screen: at 0 the haze over the sky and the logo is gone, at 3 it is much stronger (mean
  brightness 158 / 166 / 176). Giants has no motion blur pass.
- **Nothing skipped by default** (the owner's choice, 2026-10-22): `skip_intro` is off, the logo
  movies play as on the console (the option skips them); the settings templates say so too. The
  16x texture filter now comes from skylanders-portal (`ApplyGraphicsDefaults`), so the Core of
  Light launcher shows 16x as the game uses it.

## 2026-10-23: session log (night)

- **Title music from the disc** for the Core of Light launcher (skylanders-portal 6e5c284,
  rexglue-sdk 63e87be): `--export_title_music=<file.wav>` decodes the track the title screen
  streams (found with a run logging the game's file reads) from XMA2 with the SDK's FFmpeg and
  quits before a window opens, in under a second; each normal start leaves `title_music.wav` in
  the game's picture folder once. Checked: 0x6986a413.wav.hz of level/title.arc (54.9 s); the title also plays a quiet ambience (0x1385dec8, -15 dB in a recording), left out; identical to FFmpeg 8.1's own decode of the
  track (-63 dB).
- **Leaving the game with the pad:** F4 > Spiel ends with "Spiel beenden" and "Spiel neu
  starten" (both ask, "Nein" first; restart not offered when the launcher started the game; the
  new instance waits for the old one), the player menu is a row on the Portal page, the settings
  menu's buttons have no "Aus" any more (it locked a pad out), "Alle Einstellungen" says it is for
  keyboard and mouse (skylanders-portal 5fff7aa).
- **HD texture packs:** rexglue-sdk 37d3e77 dumps the game's BC1-BC3 textures
  (`texture_dump_dir`) and loads `<hash>.dds` replacements of any size (`texture_replace_dir`);
  a pack in `Documents\Skylanders\Textures\Giants` is used by itself (skylanders-portal 94175f0).
  Checked in Swap Force's hub (see SwapForceRecomp docs/texture-packs.md, #62).
- **F4 > Grafik:** "Schärfe-Stärke" (CAS's additional sharpness) and "Farbverläufe glätten
  (Dithering)", dithering on by default.
- **Installer:** its own AppId (all six shared one); `/NOROM=1` installs without the disc for the
  launcher (the settings file is still written); reasons a silent install stops go into its log.
  Silent install from the owner's ISO, start (German title screen), silent uninstall: checked.
- **#26 (FXAA) closed, not needed:** the loader keeps `tfb_FXAA_PS` in the global 0x826ED040; a
  read/write watch on it from start through the intro, the title, a figure's arrival and 25 s of
  play saw no access, while the same watch on the bloom combine shader's global saw 1478 reads in
  10 s. The game smooths edges with 2x MSAA there. No long-fall check like Swap Force's
  (`_stuckFallingDistanceThreshold`) in this game's executable (strings searched), so nothing to
  fix there.

## 2026-10-23: Weiterspielen into the save played last

- Core of Light's Weiterspielen tile (`--continue_game=true`) gets into the save played last: the
  "Speicherplatz wählen" list always starts on slot 1 (checked with a copy of the owner's save,
  slot 4, slots 1-3 empty). `src/game/continue_slot.cpp` takes the slot whose `Save_Slot_<n>.xsg`
  was written last and, before the continue's A, taps Down until the menu script's "selection
  cur" is that slot's entry. The slot page is the "current menu" whose member "items" lists four
  entries ("Spiel wählen" has three); the entries are in order, so the target is known exactly.
  Found by comparing the script heap's variables on "Spiel wählen", on the slot page and after
  one Down. Checked by script on the owner's save copy without input: the cursor went to slot 4,
  the save loaded, the game asked for a Skylander and player 1's figure from Recent went on.

- **Weiterspielen removed again** (the owner's decision the same evening): blind A presses through
  the menus are too unsafe, and a game is at its title in seconds, where the player picks the right
  save himself. `src/game/continue_slot.cpp` and the library's continue step are gone again.

## 2026-10-24: 0.1.0-beta.7 (a colleague's evening on SteamOS)

- **Ultrawide by itself in fullscreen** (skylanders-portal bd1b318): the ultrawide camera and the
  presenter take the aspect from `resolution`, so "Fenstergröße: Automatisch" stayed 16:9 on a
  21:9 screen. `ResolutionFromScreenIfAutomatic`, first thing in OnPreSetup, sets it for the start
  to the primary screen's mode (nothing written; not for the launcher's schema).
- **Window sizes** add 2560x1080 and 3440x1440 (21:9), 3840x1600 (24:10) and 5120x1440 (32:9).
- **"Tastatur spielt als"** is greyed and can't be changed while "Tastatur und Maus" is off
  (skylanders-portal 10a00cd, `enabled_by`, also in the schema for Core of Light).

## 2026-10-24: 0.1.0-beta.8

- **Later games' versions in the portal menu's version choice** (skylanders-portal 202b4df): Y on a
  Skylander lists its versions from the later games too (Spyro in Spyro's Adventure: Series 2 and
  3, LightCore, Eon's Elite; the game plays them by their id as its own; the owner's wish). The
  tabs stay this game's own; "Figuren späterer Spiele" now only adds the later games' tabs.
- Ultrawide checked in a 2560x1080 window from the title to the first menus in Spyro's Adventure,
  Giants, Swap Force, SuperChargers and Imaginators (menus centred, nothing stretched, movies with
  bars); Trap Team's window couldn't be captured here (black at 16:9 too), so not by picture.

## 2026-10-24: 0.1.0-beta.9

- **One data folder for every game** (the owner's call; skylanders-portal data_folder.h): the saves,
  achievements and shader cache are in `Documents\Skylanders\Giants` now, next to Figures and
  Pictures, or in the folder picked in Core of Light (`%APPDATA%\Skylanders\data-folder.txt`). The
  first start copies the old `Documents\giantsrecomp` there (complete or not at all) and leaves a
  note in it; Core of Light asks before the old folder goes. `--user_data_root` works as before.
- **Keyboard keys changeable in F4** (SwapForceRecomp #63): a "Tastatur" page after "Steuerung", a
  row per controller button; A and then the key. Y also saves with a German keyboard now (the
  menu reads keys by their US place, where that key is Z).
- The portal menu's Portal of Power: a band of light round its side, ridges on the rim, rings of
  light running out over the floor (SwapForceRecomp #57).
- Checked: every game boots to its first menus on fresh scratch data with the new data folder
  code (with --user_data_root, so the copy itself was checked in the launcher on a scratch
  Documents).

## 2026-10-25: 0.2.0-beta.1

- **Portal menu, companions by game** (skylanders-portal 2278b99): what goes on next to a player's
  Skylander now depends on the game (sidekicks up to Swap Force, traps in Trap Team, vehicles from
  SuperChargers on). Here the sidekicks behave as before.
- **Mouse wheel in the menus** (SwapForceRecomp #65; skylanders-portal ccc7009): F4's rows, the
  portal menu's cards and its variant list scroll a row per notch; the chosen row stays, and the view
  follows it again once the keys or the pad move it.
- **A chosen 4x texture filter stays 4x** (Core of Light #21, a colleague's report and fix;
  skylanders-portal 23be5f2): 4x is the SDK's own default, so it counted as unset and became 16x.

## 2026-10-25 (night): native 3D picture off for now (#30)

- At native size most outside scenes were black (the title, the Junkyard Isles; the ship's inside
  fine): the game still plans its EDRAM as the 360's 2048 tiles. Measured by scene size: 1000x600,
  1024x608, 1040x608 fine (colour + depth at 2x MSAA, 80x8-pixel tiles: 1976 tiles); 1120x640,
  1264x608, 1008x704 black (2240-2432). The hook keeps 1000x600 for a scene above 2048 tiles and
  logs it; the F4 row is gone until the planning is found and moved as in Swap Force
  (SwapForceRecomp src/hooks/scene_size_hook.cpp: planning heap, tiling decision, depth base).

## 2026-10-25: 0.2.0-beta.2

- The setup's version for what came since 0.2.0-beta.1: the native 3D picture is off for now (#30, above). Also in every game: the portal overview (Back + Y / F5: what is on the portal, taken off one by one without lifting the
  Skylander) and F4 > Controller (button jobs, vibration strength per player; not yet tried with a
  real controller), skylanders-portal f2c4940 / 6e96c16; an A press right after the menu opened by
  itself no longer picks a figure (SwapForceRecomp #72); one bad command-line option no longer drops
  the others (rexglue-sdk 605000d).

## 2026-10-25: afternoon (after 0.2.0-beta.2)

- **"Needs a break"** (`304_PortalError_NeedsRemoval_Minion`): opens that player's menu, to swap
  the Skylander in one go, instead of the check that says to take it off; the figure is marked
  tired.
- **The portal overview holds the game** while it is open (rexglue-sdk adf2a90, a frame gate in
  VdSwap: the guest waits before handing over its next frame while the menus keep drawing;
  skylanders-portal ab11cbc). It closes when the portal menu opens and after a vehicle is taken
  off there (the game's next request took the Skylander off with the same A, SuperChargersRecomp).
  Its cards stand just above the portal, and the portal's band of light goes all the way round.
- **Tired Skylanders** (skylanders-portal `PlayerSkylanderTired`): when the game says a player's
  Skylander is out of health, the menu opens and that figure gets a red cross until it has been on
  the portal a while again (a new level). It stays pickable, so a wrong guess can't lock a figure.
- **A hat shows only in a game that knows it** (Enigma's SuperChargers hat showed as "Hut Nr. 269").
- **F4 opens on "Spiel"**, which now has the game's **volume** (0-100 %, SwapForceRecomp #68) and
  the **frame rate display** (the SDK's F3 window, Core of Light #28). **F4 > Tastatur** names each
  key as this PC's keyboard prints it ("Ö / Leertaste", "Umschalt+Pfeil hoch") and has "Alle
  Tasten zurücksetzen" (SwapForceRecomp #66). skylanders-portal 218defd.
- The "who's talking" circle sits higher, above the game's health bar.
- **Settings descriptions broken into lines** (SwapForceRecomp #69): one line on what a setting does,
  the choices on lines of their own, notes ("Kostet ...", "Braucht D3D12") after a blank line;
  the same wording in every game where the setting is the same. The Back + Y row is called
  "Portal-Übersicht" now (it opens the portal overview since skylanders-portal f2c4940).
  skylanders-portal 2a97055.
- **Native 3D picture back** (#30): the game planned its EDRAM as the 360's 2048 tiles, so a
  scene above that (1264x704 needs 2816 tiles for colour and depth at 2x MSAA) was cut into
  predicated-tiling strips and its depth surface put over the colours: the title and the outside
  scenes drew black. Two mid-asm hooks as in Swap Force (its third, the frame planner heap, isn't
  in this game): 0x822F9C30 in sub_822F9B20 lets a pass up to 4096 tiles go untiled, 0x822FA8A8 in
  sub_822FA6E8 puts the depth at the top of the SDK's 4096-tile EDRAM. Checked: the title at
  1264x704 draws in full (logo, sky, rocks), the log shows the scene's 2816-tile pass untiled and
  its depth at tile 2688. F4 > Grafik offers "Natives 3D-Bild" again. Not yet seen in a level.
- **Hints in the pad's own buttons** (Core of Light #29, skylanders-portal 39ab210): after a press on a
  PlayStation pad the menus' hints show its symbols and L1 / R1 / Options, on a Nintendo pad its
  letters in their places and L / R / ZL / ZR / +; Xbox-like pads stay as they were.

## 2026-10-25: 0.2.0-beta.3

- The setup's version for what came since the last one: the native 3D picture back (#30) and "needs a break" opening the menu. Also in every game: the portal overview holds the game while open, a Skylander out of health gets the menu and a red cross, hats only where the game knows them, F4 opens on "Spiel" with volume and frame rate rows, key names as the keyboard prints them and a reset row, descriptions in short lines, hints in a PlayStation or Nintendo pad's own buttons, and figure changes with an HD pack without the stutter (skylanders-portal 39ab210, rexglue-sdk ce9dbb6).

## 2026-10-26

- **Nightmare from the start** (`nightmare_from_start`, F4 > Game, off; SwapForceRecomp #74, as in
  Swap Force and Imaginators): the new-game script in Title.bld reads system flag 3 and calls
  "Difficulty UI - Show Difficulty Menu" with "allow nightmare" = that value; the game's own other
  call (from the pause menu) passes 0, so the menu's check stays as it is. A mid-asm hook where a
  macro parameter is bound (0x821C8990 in sub_821C8930, src/hooks/nightmare_hook.cpp) gives that
  one call's parameter the call's literal 1 ("force default option" next to it). Checked on empty
  scratch data: on, the menu shows four and "Entfesselter Kaos (Albtraum)" can be chosen; off,
  three up to "Schwergewicht (Schwer)". The save isn't touched.
- rexglue-sdk 07f1106: no null read in the presenter's vblank thread when the window moves to
  another monitor at start.

## 2026-10-26: 0.2.0-beta.4

- The setup's version for what came since the last one: "Nightmare from the start" (F4 > Game, off by default, the save isn't changed). Also in every game: no crash at start when the window moves to another monitor (rexglue-sdk 07f1106), and the portal menu no longer lifts a Skylander the game is still loading from the portal (skylanders-portal d4ce7b6).

## 2026-10-26: night

- rexglue-sdk 90f3bad: an XMA input counts as used up only once its output is out (the cut-off ends of Trap Team's portal lines, TrapTeamRecomp #15).
- skylanders-portal 8cbf30d: the picture cache also leaves the game's element symbols for the launcher's figure library (SkylandersCoreOfLight #38). Checked: all eight.

## 2026-10-26: 0.2.0-beta.5

- The setup's version for what came since the last one: In every game: a sound the game stops right at its end keeps its last 1024 samples (rexglue-sdk 90f3bad), and the game leaves its element symbols for the launcher (skylanders-portal 8cbf30d).

## 2026-10-27: night

- rexglue-sdk 5201aa3: an option for the pixel position shaders read at resolution scale 2 and up (each host pixel's centre instead of its corner), off by default and here: this game's title screen lines up with scale 1 as it is (SSARecomp #40).

## 2026-10-27: shadows, start-up

- **Softer shadows and blur at resolution scale 2 and up** (ImaginatorsRecomp #12): skylanders-portal afba1dd sets draw_resolution_scaled_texture_offsets = false unless it was set. The SDK divided texture fetch offsets by the resolution scale, so the games' fixed blur kernels (the shadow mask's 9-tap blur, bloom) shrank with the scale. Compared here at scale 2, on and off: the menus' 3D background unchanged; no level compared yet (the script didn't reach the save's slot), but Spyro's Adventure, same engine, was. Scale 1 is the same either way.
- **A start-up crash less** (rexglue-sdk 807af87): SDL's audio subsystem starts on the UI thread before the game's audio driver asks for it. Started from the audio thread it could race SDL_UpdateAudio in the UI thread's event pump and end in exception C0000264 (once in Trap Team, 2026-10-27).

## 2026-10-27: 0.2.0-beta.6

- The setup's version for what came since the last one: softer shadows and blur at resolution scale 2 and up (texture fetch offsets in the console's texels, skylanders-portal afba1dd) and SDL's audio started on the UI thread against a rare start-up crash (rexglue-sdk 807af87).

## 2026-10-27: morning

- The texture offset change of 0.2.0-beta.6 compared in the first level since (scale 2, on and off, the same input both times): no artefacts, the bridge's and the cliff's shadows as before within the camera's small difference.

## 2026-10-27: shared libraries

- SDK 7be1eb2: two upstream fixes (kernel objects freed again, saved-game results checked properly) and resolved memory read back on request.

## 2026-10-27: shared libraries

- SDK 642311f: menus over a held game (the portal overview) present at the frame cap instead of every 100 ms: no more 10 FPS stutter in the overview.

## 2026-10-27: 0.3.0-beta.1

- The setup's version for what came since the last one: one version for all six games from now on; the portal overview (Back + Y, F5) runs smoothly while it holds the game (rexglue-sdk 642311f), and two upstream SDK fixes: kernel objects freed again, saved-game results checked properly (rexglue-sdk 4a34314, 3cb725a).

## 2026-10-28: shared libraries

- SDK cf59726: the XMA "no bits to copy" line is debug only now; library 433cb9f: the crash report also logs where the SDK's fatal errors happen (SSARecomp #42), and developer tools (pollwatch, the write watch works in Trap Team).

## 2026-10-28: shared libraries

- SDK 88d6730: cheaper register writes on the GPU thread (Spyro's Adventure Chapter 1: 51-55 -> 60 FPS at 1080p, frame cap off).

## 2026-10-28: 0.3.0-beta.2

- The setup's version for what came since the last one: the GPU thread writes registers more cheaply (rexglue-sdk 88d6730: Spyro's Adventure's Chapter 1 51-55 -> 60 FPS at 1080p), the crash report also says where the SDK's fatal errors happen (skylanders-portal 433cb9f, SSARecomp #42), and the XMA "no bits to copy" line is debug only (rexglue-sdk 7d147fe).
