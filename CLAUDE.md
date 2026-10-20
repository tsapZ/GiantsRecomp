# CLAUDE.md: GiantsRecomp

## Goal

Native Windows port of Skylanders: Giants (Xbox 360, `rom/default.xex`) via ReXGlue static
recompilation, built like SwapForceRecomp (`../../SwapForceRecomp/SwapForceRecomp`) and SSARecomp
(`../../SSARecomp/SSARecomp`): the same SDK, app, portal and menus. Later part of a Skylanders hub
where every game stays its own exe.

## Rules

- Analyse first, then build. When unclear, ask instead of guessing.
- Small commits, Conventional Commits (`feat:`, `fix:`, `build:`, `docs:`).
- Never commit game data, secrets or tokens. Before EVERY commit check `git status`: no `rom/`
  content, `*.xex`, `*.iso` or figure dumps.
- No personal information about the owner in files, commit messages or docs.
- Every `config/default.toml` entry and every kernel stub gets a comment: what is at the address,
  how it was found, why the fix or stub is safe. Addresses from other projects (SwapForceRecomp,
  SSARecomp, TheBiemGamer's GiantsRecomp) are checked against this XEX before use.
- Do not modify submodules (`thirdparty/`) except on clearly named own branches. The portal code
  is shared with SwapForceRecomp and SSARecomp (fork `tsapZ/skylanders-portal`, branch
  `feature/portal-menu`) and so is the SDK (fork `tsapZ/rexglue-sdk`, branch `skylanders`):
  changes there must keep working in every game.
- If Giants differs technically from Swap Force in a significant way, report it and adapt the
  plan (known: older IGA archives, docs/game/giants.md).
- Git identity is repo-local (`git config user.name/user.email`), never global.
- Scripted or experimental game runs use a copied `--user_data_root`, never the user's real save
  folder; the shared figure folder sits next to it ("Skylanders\Figures"), so a copy gets its own.
  Always `--portal_mode=software` unless a real portal test is wanted.

## Build

Always from the **x64 Native Tools Command Prompt for VS 2022**.

    just submodules        # git submodule update --init --recursive
    just build-release     # configures on first run, then builds
    just regen-release     # rerun codegen from rom/default.xex, reconfigure, rebuild
    just test-release
    just play-release

## Status convention

`docs/STATUS.md` is updated at the end of every session; `docs/roadmap.md` when the plan changes,
and the README status table with it.
