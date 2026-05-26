#pragma once

namespace AwesomeMacroLite {
    enum class MacroTarget {
        Cursor,
        PlayerLocation
    };

    void ArmMacroTarget(MacroTarget target);
    void ClearMacroTargetFlags(const char* reason);
    bool MacroTargetFlagsExpired();
    bool IsCursorTargetActive();
    bool IsPlayerLocationTargetActive();
}
