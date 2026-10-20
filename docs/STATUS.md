# Status

Updated at the end of every session. Only record what a build actually did.

## Current state (2026-10-20)

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
  pause menu, the title and the autosave notice open nothing.
- **Input script:** `save` (guest memory to a file, for snapshot comparisons), `remove <slot>`,
  and addresses as `hit<n>` (a hit of the last scan) or `*<addr>` (through a pointer), for data
  whose heap address changes from run to run.
