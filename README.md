# Awesome WotLK - Peaap Fork

This fork keeps the upstream Awesome WotLK code available, but the currently
recommended Grimfall WoW path is the small `AwesomeMacroLite.dll` target.

`AwesomeMacroLite.dll` implements the macro conditional path needed for
`[@cursor]` and `[@playerlocation]`. It patches the secure macro parser and the
small terrain-submit path required for ground-target spells, while avoiding the
full renderer, camera, voice, nameplate, and addon-bridge hook surface.

## Current Safe Target

- Target: `AwesomeMacroLite`
- Output: `build/Release/AwesomeMacroLite.dll`
- Installed DLL: `%WOW_ROOT%\AwesomeMacroLite.dll`
- Runtime log: `%WOW_ROOT%\_mpq_tools\reports\macro_lite\awesome_macro_lite.log`
- Required client: Grimfall WoW, based on 32-bit WotLK 3.3.5a build 12340
- Current feature: `[@cursor]` and `[@playerlocation]` ground-target macro casts

Build and install:

```bat
build_install_macro_lite_x86.bat C:\Games\Grimfall-WoW
```

For direct `Wow.exe` launches, build and run the Grimfall patcher:

```bat
build_grimfall_patch_x86.bat
build\Release\GrimfallWotlkPatch.exe
```

Put `GrimfallWotlkPatch.exe` beside `Wow.exe` and run it. If the patcher cannot
find `Wow.exe` automatically, it opens a file picker. It still accepts an
explicit path for advanced use. After patching, `Wow.exe` loads
`AwesomeMacroLite.dll` directly. MacroLite still waits for Grimfall's
`ClientExtensions.DLL` before installing hooks.

## Trust Model

This fork should stay source-readable:

- no packed binary loader
- source-visible Grimfall patcher
- SHA-256 printed after local build/install
- small macro-only DLL for the current Grimfall WoW compatibility path
- expected-byte checks before patching client code
- generated build/dependency output excluded from git
- full Awesome WotLK library kept separate until each module is reverified

More detail is in `TRUST.md` and `docs/macro_lite.md`.

## Source Layout

The Grimfall-specific MacroLite target is split into small reviewable modules
under `src/AwesomeMacroLite` instead of one large source file. Each feature
area owns its own hook, state, or utility code so future ports can stay
isolated.

## Hook Backend

The stable macro path still uses the existing small x86 hooks. KNSoft
SlimDetours `v1.2.1-beta` is vendored under `deps/KNSoft.SlimDetours` with the
matching KNSoft.NDK `1.2.50-beta` headers under `deps/KNSoft.NDK` for new
or reverified hooks that need relocated trampolines, such as functions whose
first bytes include relative `call` or `jmp` instructions.

The first SlimDetours-backed MacroLite hook is the Grimfall stance/form fix at
`SpellOnCast`, used to keep stance/form macro chains responsive after a
successful form cast.
