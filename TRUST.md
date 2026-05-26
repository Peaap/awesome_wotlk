# Trust and Verification

This fork is intended to be reviewable by server owners and players before use.

## What The Release Contains

- `AwesomeMacroLite.dll`, built from this repository for the current Grimfall
  WoW macro conditional path
- `AwesomeWotlkLib.dll`, kept available for upstream feature work after module
  compatibility is reverified
- addon folders under `Interface/AddOns`
- source-visible build and install scripts
- `GrimfallWotlkPatch.exe`, built from `src/GrimfallWotlkPatch`
- `SHA256SUMS.txt` for release-file verification

The release package does not need a closed or packed patch executable. The
Grimfall patcher is source-visible, verifies expected bytes before writing, and
creates a `Wow.exe.grimfall-awesome.bak` backup before applying the patch.
MacroLite still waits for Grimfall WoW's `ClientExtensions.DLL` before
installing hooks.

## MacroLite Guarantees

`AwesomeMacroLite.dll`:

- is built from `src/AwesomeMacroLite/Entry.cpp`
- imports only `KERNEL32.dll` and `ntdll.dll` in the current macro build
- waits for `ClientExtensions.DLL` before installing hooks
- uses one manual x86 trampoline at `SecureCmdOptionParse`
- uses one manual x86 trampoline at `CGWorldFrame::OnLayerTrackTerrain`
- uses KNSoft.SlimDetours for `SpellOnCast`, whose prologue contains a
  relative call that the simple x86 trampoline cannot relocate
- verifies the expected hook bytes before patching memory
- clears armed macro-target flags after a short timeout or successful terrain click
- logs startup, hook install, rewrite/click events, timeout cleanup, and hook-byte mismatches
- does not enable renderer hooks, camera hooks, voice hooks,
  nameplate hooks, or the addon bridge

The hook surface is intentionally small: it lets `cursor` and `playerlocation`
survive `SecureCmdOptionParse`, then submits the terrain position needed by
ground-target spells without enabling the rest of the full Awesome feature set.
The `ntdll.dll` import comes from the SlimDetours-backed spell hook.

## Local Verification

```powershell
.\build_install_macro_lite_x86.bat C:\Path\To\Grimfall-WoW
.\build_grimfall_patch_x86.bat
.\build\Release\GrimfallWotlkPatch.exe C:\Path\To\Grimfall-WoW\Wow.exe
dumpbin /dependents %WOW_ROOT%\AwesomeMacroLite.dll
Get-FileHash -Algorithm SHA256 %WOW_ROOT%\AwesomeMacroLite.dll
```

Patcher utilities:

```text
GrimfallWotlkPatch.exe --status C:\Path\To\Grimfall-WoW\Wow.exe
GrimfallWotlkPatch.exe --unpatch C:\Path\To\Grimfall-WoW\Wow.exe
```

## Current Compatibility Build

This Grimfall WoW build keeps the stable feature set narrow.

- enabled: macro conditional ground-target casts for `cursor` and `playerlocation`
- disabled: full Awesome modules until each one is isolated and reverified

That avoids known conflicts with Grimfall WoW's existing runtime hooks through
`ClientExtensions.DLL`.
