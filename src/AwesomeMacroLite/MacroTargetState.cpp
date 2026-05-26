#include "MacroTargetState.h"

#include "Log.h"

#include <stdio.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr DWORD kMacroTargetTimeoutMs = 1500;

    volatile LONG g_cursorKeywordActive = 0;
    volatile LONG g_playerLocationKeywordActive = 0;
    volatile LONG g_macroTargetArmedTick = 0;
}

void ArmMacroTarget(MacroTarget target) {
    InterlockedExchange(const_cast<LONG*>(&g_cursorKeywordActive), target == MacroTarget::Cursor ? 1 : 0);
    InterlockedExchange(const_cast<LONG*>(&g_playerLocationKeywordActive), target == MacroTarget::PlayerLocation ? 1 : 0);
    InterlockedExchange(const_cast<LONG*>(&g_macroTargetArmedTick), static_cast<LONG>(GetTickCount()));
}

void ClearMacroTargetFlags(const char* reason) {
    LONG hadCursor = InterlockedExchange(const_cast<LONG*>(&g_cursorKeywordActive), 0);
    LONG hadPlayerLocation = InterlockedExchange(const_cast<LONG*>(&g_playerLocationKeywordActive), 0);
    InterlockedExchange(const_cast<LONG*>(&g_macroTargetArmedTick), 0);

    if (hadCursor != 0 || hadPlayerLocation != 0) {
        char line[192];
        sprintf_s(line, "Macro target flags cleared reason=%s cursor=%ld playerlocation=%ld",
            reason ? reason : "unknown",
            hadCursor,
            hadPlayerLocation);
        Log(line);
    }
}

bool MacroTargetFlagsExpired() {
    LONG armedTick = InterlockedCompareExchange(const_cast<LONG*>(&g_macroTargetArmedTick), 0, 0);
    if (armedTick == 0) {
        return false;
    }

    DWORD elapsed = GetTickCount() - static_cast<DWORD>(armedTick);
    return elapsed > kMacroTargetTimeoutMs;
}

bool IsCursorTargetActive() {
    return InterlockedCompareExchange(const_cast<LONG*>(&g_cursorKeywordActive), 0, 0) != 0;
}

bool IsPlayerLocationTargetActive() {
    return InterlockedCompareExchange(const_cast<LONG*>(&g_playerLocationKeywordActive), 0, 0) != 0;
}
}
