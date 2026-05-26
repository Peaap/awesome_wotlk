#include "Log.h"
#include "MacroParser.h"
#include "TerrainTargeting.h"

#include <windows.h>

namespace {
    DWORD WINAPI InitThread(LPVOID) {
        AwesomeMacroLite::Log("InitThread begin");
        Sleep(1000);
        AwesomeMacroLite::Log("InitThread after sleep");

        AwesomeMacroLite::InstallMacroParserHook();
        AwesomeMacroLite::InstallTerrainTargetingHook();
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
