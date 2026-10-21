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
  1566231, found in SwapForceRecomp with the USB portal).
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
