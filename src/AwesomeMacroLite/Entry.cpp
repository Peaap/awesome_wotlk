#include "Log.h"
#include "MacroParser.h"
#include "SpellStanceFix.h"
#include "TerrainTargeting.h"

#include <windows.h>

namespace {
    bool WaitForClientExtensions() {
        constexpr DWORD kWaitTimeoutMs = 30000;
        constexpr DWORD kWaitStepMs = 250;

        DWORD waitedMs = 0;
        while (waitedMs <= kWaitTimeoutMs) {
            if (GetModuleHandleW(L"ClientExtensions.DLL") ||
                GetModuleHandleW(L"clientextensions.dll")) {
                AwesomeMacroLite::Log("ClientExtensions.DLL detected");
                return true;
            }

            Sleep(kWaitStepMs);
            waitedMs += kWaitStepMs;
        }

        AwesomeMacroLite::Log("ClientExtensions.DLL wait timed out; installing MacroLite hooks anyway");
        return false;
    }

    DWORD WINAPI InitThread(LPVOID) {
        AwesomeMacroLite::Log("InitThread begin");
        WaitForClientExtensions();

        AwesomeMacroLite::InstallMacroParserHook();
        AwesomeMacroLite::InstallTerrainTargetingHook();
        AwesomeMacroLite::InstallSpellStanceFixHook();
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        AwesomeMacroLite::Log("DllMain DLL_PROCESS_ATTACH");

        HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
