# CLAUDE.md: GiantsRecomp

The rules every Skylanders repository follows (tests only on copies of the owner's data, git and
GitHub, shared code, releases) are in Core of Light's maintainer/RULES.md
(https://github.com/tsapZ/SkylandersCoreOfLight/blob/main/maintainer/RULES.md):

@../../SkylandersCoreOfLight/SkylandersCoreOfLight/maintainer/RULES.md

## Goal

Native Windows port of Skylanders: Giants (Xbox 360, `rom/default.xex`) via ReXGlue static
recompilation, one of six built the same way: the SDK (`thirdparty/rexglue-sdk`) and the app,
portal, menus, disc check and test tools (`thirdparty/skylanders-portal`) are shared. The game
stays complete on its own; Core of Light, the launcher, only starts it.

## This game

- Older IGA archives and IGZ version 6 (docs/game/giants.md).
- TheBiemGamer's GiantsRecomp (MIT) pins the same `default.xex`: its findings are read for reference
  and credited; each address is checked against this executable before use.
- If it differs technically from the other games in a significant way, report it and adapt the
  plan.

## Build

From the **x64 Native Tools Command Prompt for VS 2022** (`release.py build` sets that up itself):

    just submodules        # git submodule update --init --recursive
    just build-release     # configures on first run, then builds
    just regen-release     # rerun codegen from rom/default.xex, reconfigure, rebuild
    just test-release
    just play-release
    just --list            # everything else: packaging, scripted runs, Ghidra, RenderDoc, logs

## Status convention

`docs/STATUS.md` is updated at the end of every session (only what a build actually did);
`docs/roadmap.md` when an issue is closed or the plan changes, and the README's "How far it is"
with it.
