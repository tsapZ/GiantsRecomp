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
| Languages | English, French, Spanish on this disc (German comes out English) |

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
  under each setting); German and Italian come out English. "auto" picks the Windows language
  only if the disc has it, so the menus and the game agree.
