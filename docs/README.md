# GiantsRecomp documentation

Pages, roughly in reading order:

| Page | What it covers |
|---|---|
| [roadmap.md](roadmap.md) | Phases 0 to 4: what is done, what comes next, with issue numbers |
| [STATUS.md](STATUS.md) | What works right now and how each part was found; updated at the end of every session |
| [game/giants.md](game/giants.md) | The game, the target dump, its archives (IGA v8, IGZ v6) and how it differs from Swap Force |
| [portal.md](portal.md) | The Portal of Power here: modes, the figure folder, the menu at the game's portal screens, the portal check |
| [menus.md](menus.md) | Menus and keys: portal menu with the game's pictures, upgrades and hats, player menu, settings menu |
| [development.md](development.md) | Building from source on Windows, scripted test runs |
| [build.md](build.md) | Codegen fixes (`config/default.toml`), kernel stubs, the fix loop, testing |
| [reverse-engineering.md](reverse-engineering.md) | Headless Ghidra: identify and name functions |

The shared parts are documented where they live: the portal library and its menus in
SwapForceRecomp's [portal.md](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/portal.md)
and [menus.md](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/menus.md), the high frame
rate research in its [high-framerate.md](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/high-framerate.md),
the hub and online co-op in [Skylanders: Core of Light](https://github.com/tsapZ/SkylandersCoreOfLight).

## Glossary

- **Static recompilation:** translating the game's PowerPC machine code into C++ once, ahead of
  time, and compiling that into a native Windows program. No emulation of the CPU at runtime.
- **ReXGlue:** the SDK that does the translation (codegen) and provides the runtime: Xbox 360
  kernel/XAM functions, the Xenos GPU on Direct3D 12/Vulkan, audio, input. Lives in
  `thirdparty/rexglue-sdk` (our fork, branch `skylanders`).
- **`default.xex`:** the game's main executable on the disc. Everything in `config/default.toml`
  is keyed to addresses inside exactly one version of this file.
- **Guest address:** an address in the original Xbox 360 program (e.g. `0x8217F350`). Functions
  without a known purpose are named `sub_<address>`.
- **Midasm hook:** a call into our own C++ inserted at a guest address, declared in
  `config/default.toml` and implemented in `src/hooks/`.
- **Alchemy:** the engine of Giants and Swap Force; its archives (`.bld`, `.arc`) hold scenes
  (IGZ) with the game's textures, texts and scripts.
- **Cvar:** a runtime setting (`REXCVAR_DEFINE_*`), set in `giantsrecomp.toml`, by `--flag` or by a
  `REX_*` environment variable.
- **Input script:** a text file of timed controller input and probes (`--input_script`), for
  repeatable test runs; see development.md.
