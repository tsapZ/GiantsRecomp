# GiantsRecomp

An unofficial native Windows port of **Skylanders: Giants** (Xbox 360), made with static
recompilation through ReXGlue. The first level plays with the emulated Portal of Power, and the
portal menu shows the game's own portraits, upgrades and hats.

> **Unofficial fan project.** It is not affiliated with or endorsed by Activision, Toys for Bob
> or Microsoft. This repository contains **no game data**: you need your own copy of the game.

One of a set: [SSARecomp](https://github.com/tsapZ/SSARecomp), GiantsRecomp and
[SwapForceRecomp](https://github.com/tsapZ/SwapForceRecomp) share one portal, one menu style and
one figure folder. Later the hub, [Skylanders: Core of Light](https://github.com/tsapZ/SkylandersCoreOfLight),
starts them all; each game stays playable on its own.

## Status

First level playable. Where each part stands and what comes next: **[docs/roadmap.md](docs/roadmap.md)**.
Session details and known bugs: [docs/STATUS.md](docs/STATUS.md).

| Area | State |
|---|---|
| Boot, menus, saving | ✅ title screen after about 15 s (logo movies skipped), Choose Play, game slots, pause menu, Continue |
| First level | ✅ the intro with Eon's narration, then Chapter 1 with a figure; three minutes of scripted play without errors |
| Rest of the game | 🟡 not played through yet (#7); Battle Mode needs two pads (#8) |
| Frame rate | ✅ 30 FPS like the console, or faster: 96-108 FPS in the first level with 144 (F4 > Bildrate), game speed unchanged |
| Audio | ✅ real 5.1 (voices on the center, LFE, rear channels) on a 5.1 device, stereo otherwise |
| Languages | ✅ English, German, French, Spanish, Italian (German and Italian through a region hook, the North American disc has them all) |
| Portal | ✅ emulated: figures come in with their portal sequence, a second Skylander as player 2; real USB portal still to test (#12) |
| Portal menu | ✅ opens by itself at "Please put a Skylander" for player 1 and 2; the portal check on "too many"; the game's own portraits for every figure, item names, upgrades like the game's upgrade screen, hats |
| Settings menu | ✅ F4 or Back + RB: presets, resolution, frame rate, language, portal, controls |
| Ultrawide | ✅ the 3D view shows more to the sides (`ultrawide_fov`); the HUD is stretched |
| Installer tested on a clean PC | ⬜ planned (#20) |

## Playing

Build it once (below), then from the **x64 Native Tools Command Prompt for VS 2022**:

    just play-release

The keys and menus are in [docs/menus.md](docs/menus.md), the portal in [docs/portal.md](docs/portal.md).
The emulated figures live in `Documents\Skylanders\Figures`, shared with the other games.

## Building

Needs Windows, Visual Studio 2022 (C++ workload and C++ Clang tools), CMake, Ninja, just, Git and
Python 3, and the game's `default.xex` in `rom/` (see [rom/README.md](rom/README.md)). The steps are
the same as for SwapForceRecomp: [docs/development.md](docs/development.md). All docs:
[docs/README.md](docs/README.md).

## Credits

- **[SSARecomp](https://github.com/tsapZ/SSARecomp)** and
  **[SwapForceRecomp](https://github.com/tsapZ/SwapForceRecomp)**: this project is scaffolded from
  them; app, portal, menus and tooling come from there.
- **[GiantsRecomp by TheBiemGamer](https://github.com/TheBiemGamer/GiantsRecomp)** brought
  Skylanders Giants to PC first and is the template SwapForceRecomp started from: build setup,
  tooling and workflow. Its findings are read for reference and credited where used.
- **[ReXGlue](https://github.com/rexglue/rexglue-sdk)**, the static recompilation SDK, here in
  TheBiemGamer's fork with our fixes on top.
- **[extract-xiso](https://github.com/XboxDev/extract-xiso)** is included (prebuilt, unmodified)
  for the installer (`installer/extract-xiso.LICENSE.txt`).

Skylanders is a trademark of Activision. This project is not affiliated with or endorsed by
Activision.
