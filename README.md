# Awesome WotLK - Peaap Fork

This fork keeps the upstream Awesome WotLK code available, but the currently
recommended Grimfall WoW path is the small `AwesomeMacroLite.dll` target.

`AwesomeMacroLite.dll` only patches the macro conditional parser needed for
`[@cursor]` and `[@playerlocation]`. It avoids the full renderer, camera,
voice, nameplate, and addon-bridge hooks while the custom `ClientExtensions.DLL`
compatibility work is being verified.

## Current Safe Target

- Target: `AwesomeMacroLite`
- Output: `build/Release/AwesomeMacroLite.dll`
- Installed DLL: `%WOW_ROOT%\AwesomeMacroLite.dll`
- Runtime log: `%WOW_ROOT%\_mpq_tools\reports\macro_lite\awesome_macro_lite.log`
- Required client: Grimfall WoW, based on 32-bit WotLK 3.3.5a build 12340

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
- full Awesome WotLK library kept separate until each module is reverified

More detail is in `TRUST.md` and `docs/macro_lite.md`.
