# GiantsRecomp

An unofficial native Windows port of **Skylanders: Giants** (Xbox 360), made with static
recompilation (ReXGlue). Work in progress: it boots, and the first level plays with a figure.

You need your own copy of the game: nothing from the game is in this repository.

## Status

See [docs/STATUS.md](docs/STATUS.md) and [docs/roadmap.md](docs/roadmap.md).

| Area | State |
|---|---|
| Build | ✅ codegen and build work |
| Boot | ✅ title screen, menus |
| In-game | ✅ first level with a figure |
| Portal | ✅ emulated portal: figures are read and come into the game |
| Portal menu | ✅ opens by itself when the game asks for a Skylander; portal check on "too many"; the game's own portraits, item names and upgrade screen |
| Languages | ✅ English, German, French, Spanish, Italian |
| Frame rate | ✅ 30 FPS like the console, or up to your screen's rate (F4 > Bildrate) |

## Building

The same as SwapForceRecomp: see [docs/development.md](docs/development.md).

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
