# Roadmap

Where the project stands, what is done, what comes next. Details of every session are in
[STATUS.md](STATUS.md); each phase is a GitHub milestone. A step only counts as done when it was
observed in a real build or a real play test, not inferred.

## Overview (2026-10-20)

| Phase | State | Issues | What is left |
|---|---|---|---|
| 0 Repository and scaffold | ✅ done | 2/2 | – |
| 1 Recompile and boot | ✅ done | 2/2 | – |
| 2 In-game | 🟡 first level | 2/5 | later levels and the Giants (#7), Battle Mode with two pads (#8), the owner's play test (#21) |
| 3 Portal | 🟡 almost | 3/4 | the real USB portal (#12) |
| 4 Menus and polish | 🟡 almost | 7/8 | installer (#20) |

**Where we are:** the first level plays, in five languages and at up to the screen's frame rate.
The portal menu opens by itself at the game's portal screens for both players and shows the
game's own portraits, item names, upgrades and hats.

## Now, next, later

| When | What | Who |
|---|---|---|
| **Now** | Play test (#21): German voices, later levels at 200 FPS, a Giant on the portal, a figure with bought upgrades and one with a hat, Battle Mode with both pads, any wrong picture, the real USB portal | you |
| Next | Whatever the play test finds | me |
| Then | Installer (#20) | me |
| After all games | The hub, [Skylanders: Core of Light](https://github.com/tsapZ/SkylandersCoreOfLight) | both |

## Phase 0: repository and scaffold ✅

- [x] Disc extracted, `default.xex` hash pinned (the North American disc, the one TheBiemGamer's
  GiantsRecomp pins) (#1)
- [x] Own repository, scaffolded from SSARecomp (#2)

## Phase 1: recompile and boot ✅

- [x] Codegen and build; four Bink jump tables needed their real size (#3)
- [x] Logo movies skipped, the opening movie optional (#4)

**Done when:** the title screen comes up. Observed 2026-10-20.

## Phase 2: in-game 🟡

- [x] Menus and the first level with a figure, pause menu, Continue (#5)
- [x] German and Italian through the region hook (#6)
- [ ] Later levels, the Giants, saving and loading (#7)
- [ ] Battle Mode with two controllers (#8)
- [ ] The owner's play test (#21)

**Done when:** the first levels are played through with a controller.

## Phase 3: portal 🟡

- [x] Emulated portal, a second Skylander as player 2 (#9)
- [x] The portal menu opens by itself at "Please put a Skylander", for player 1 and 2 (#10)
- [x] The portal check on the game's "too many" screens (#11)
- [ ] The real USB portal and the hybrid mode (#12)

**Done when:** both portals work and two players can play.

## Phase 4: menus and polish 🟡

- [x] The game's portraits for every Skylander, sidekick and magic item (#13)
- [x] The game's names for magic items (#14)
- [x] Upgrades in the details view like the game's upgrade screen (#15)
- [x] Upgrade pictures and element symbols the right way up (#16)
- [x] Hats in the details view (#17)
- [x] Frame rate above 30 without changing the game's speed (#18)
- [x] Ultrawide (#19): the 3D view; the HUD is stretched
- [ ] Installer tested on a clean Windows VM (#20)

**Done when:** the menus show everything the game's own screens do, and the installer works on a
clean PC.

## From the play tests (2026-10-21)

- [x] Portraits the right way up (the owner's report)
- [x] Hat bonuses with their symbol (SwapForceRecomp #58)
- [x] The game's gold symbol (SwapForceRecomp #57)
- [x] Controls page with a controller picture (drawn) and a keyboard view
- [x] Achievements: Xbox 360-style pop-up, list with the controller
