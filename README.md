# Awesome WotLK - Peaap Fork

This fork is focused on Grimfall WoW. Legacy upstream Awesome WotLK modules are
kept out of the tracked tree unless they are reworked into the Grimfall-specific
MacroLite path.

`AwesomeMacroLite.dll` implements the Grimfall-tested feature path. It patches
the secure macro parser and terrain-submit path for ground-target spells,
includes Spell stance support, and registers the modular Nameplate API CVars.
Nameplate positioning is opt-in through `grimfallNameplatePositioning` while
stacking/layout work is ported in small verified pieces.

## Current Safe Target

- Target: `AwesomeMacroLite`
- Output: `build/Release/AwesomeMacroLite.dll`
- Installed DLL: `%WOW_ROOT%\AwesomeMacroLite.dll`
- Runtime log: `%WOW_ROOT%\_mpq_tools\reports\macro_lite\awesome_macro_lite.log`
- Required client: Grimfall WoW, based on 32-bit WotLK 3.3.5a build 12340
- Current features: `[@cursor]` and `[@playerlocation]` ground-target macro
  casts, Spell stance support, modular Nameplate API CVars, and opt-in
  nameplate anchor positioning

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
- small Grimfall-specific DLL for the current compatibility path
- expected-byte checks before patching client code
- generated build/dependency output excluded from git
- full Awesome WotLK library kept separate until each module is reverified

More detail is in `TRUST.md` and `docs/macro_lite.md`.

## Addon UI

The `addons/GrimfallMacroLite` folder contains an Ace3-backed custom options
UI for Grimfall MacroLite settings. It currently saves addon bridge settings
and will mirror them into `grimfallAddonBridge`, `grimfallAddonBridgeTrace`,
and `grimfallAddonBridgeMaxBytes` once the backend registers those CVars.
Ace3 `Release-r1390` is embedded so users do not need a separate Ace3 install.

Open it with `/gml`, `/grimfallmacrolite`, or Interface Options.

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
