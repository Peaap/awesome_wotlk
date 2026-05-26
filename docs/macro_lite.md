# AwesomeMacroLite

`AwesomeMacroLite.dll` is the narrow compatibility target for Grimfall WoW,
which already loads `Data\Extensions\ClientExtensions.DLL`.

The full Awesome WotLK DLL is feature-rich, but it currently collides with the
Grimfall WoW client after login in ways that are not isolated to one
user-facing feature.
MacroLite keeps the proven macro-conditional behavior and removes the rest of
the hook surface.

## What It Does

MacroLite installs two manual x86 trampolines:

```text
SecureCmdOptionParse = 0x00564AE0
expected bytes       = 55 8B EC 83 EC 10
patch size           = 6

OnLayerTrackTerrain  = 0x004F66C0
expected bytes       = 55 8B EC 81 EC A4 00 00 00
patch size           = 9
```

After the original parser runs, it checks the Lua return stack. When the parsed
target is `cursor` or `playerlocation`, it rewrites the return values so the
client accepts the target conditional instead of rejecting it.

The terrain hook then submits the ground-target position:

- `cursor` uses the terrain position from the active targeting event.
- `playerlocation` reads the current player position and submits that.

Armed macro-target flags are cleared after a successful terrain click or after
a short timeout, so a canceled/failed cast cannot leave a stale target active.

## Module Layout

- `Entry.cpp`: DLL startup and hook initialization order.
- `Log.cpp`: runtime log path resolution and file logging.
- `SlimHook.cpp`: KNSoft SlimDetours wrapper for future relocated trampolines.
- `X86Hook.cpp`: expected-byte checks, trampoline allocation, and jump patching.
- `GameClientLite.h`: minimal Grimfall/WotLK addresses and client structs.
- `MacroParser.cpp`: `SecureCmdOptionParse` rewrite for macro targets.
- `MacroTargetState.cpp`: armed `cursor` / `playerlocation` state and timeout cleanup.
- `TerrainTargeting.cpp`: terrain submit hook for ground-target casts.

## What It Avoids

MacroLite intentionally does not include:

- enabled Detours/SlimDetours hooks in the stable macro path
- D3D or renderer hooks
- camera hooks
- voice or text-to-speech hooks
- nameplate hooks
- addon communication bridge hooks
- full miscellaneous interaction hooks

The current stable macro DLL imports only `KERNEL32.dll`. Future hooks that
actually use the SlimDetours backend may add `ntdll.dll`.

## Build

Use a 32-bit MSVC environment. Pass your WoW folder explicitly or set
`WOW_ROOT` first:

```bat
build_install_macro_lite_x86.bat C:\Games\Grimfall-WoW
```

The script configures `build-codex-x86` when needed, builds the
`AwesomeMacroLite` target, copies it to `%WOW_ROOT%\AwesomeMacroLite.dll`, and
prints the installed file hash.

To build without installing:

```bat
build_macro_lite_x86.bat
```

## Runtime

For direct `Wow.exe` launches, use the source-visible Grimfall patcher:

```bat
build_grimfall_patch_x86.bat
build\Release\GrimfallWotlkPatch.exe C:\Games\Grimfall-WoW\Wow.exe
```

The patcher modifies `Wow.exe` to load `AwesomeMacroLite.dll` through the
client's internal DLL loader. MacroLite then waits for `ClientExtensions.DLL`
before installing hooks, preserving the stable tandem order without a `.bat`
launcher.

Expected log lines:

```text
DllMain DLL_PROCESS_ATTACH
InitThread begin
ClientExtensions.DLL detected
SecureCmdOptionParse manual hook installed ...
OnLayerTrackTerrain manual hook installed ...
SecureCmdOptionParseHook rewriting target ...
OnLayerTrackTerrainHook cursor click ...
Macro target flags cleared reason=cursor-click ...
```

The rewrite log is rate-limited so normal gameplay does not produce a large
passthrough trace.
