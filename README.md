# GiantsRecomp

An unofficial native Windows port of **Skylanders: Giants** (Xbox 360), made with static
recompilation (ReXGlue). Work in progress: the project is being set up and does not boot yet.

You need your own copy of the game: nothing from the game is in this repository.

## Status

See [docs/STATUS.md](docs/STATUS.md) and [docs/roadmap.md](docs/roadmap.md).

| Area | State |
|---|---|
| Build | ✅ codegen and build work |
| Boot | ✅ title screen |
| Portal | ⬜ (the portal code is in, not tried with this game) |

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
