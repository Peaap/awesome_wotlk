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

Run with the tandem loader from the local tool folder:

```bat
%WOW_ROOT%\_mpq_tools\launchers\start_wow_load_macro_lite_with_clientextensions.bat
```

## Trust Model

This fork should stay source-readable:

- no packed binary loader
- no hidden exe patcher
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
