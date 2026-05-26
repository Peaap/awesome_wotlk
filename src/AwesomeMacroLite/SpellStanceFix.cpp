#include "SpellStanceFix.h"

#include "GameClientLite.h"
#include "Log.h"
#include "SlimHook.h"

#include <stdio.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr size_t kSpellOnCastExpectedSize = 8;
    constexpr uint32_t kUnitFieldBytes2 = 0x0006 + 0x0074;
    constexpr uint8_t kOffsetShapeshiftForm = 3;

    using SpellOnCastFn = int(__cdecl*)(int spellId, int a2, int a3, int a4, int a5);
    SpellOnCastFn g_originalSpellOnCast = nullptr;

    enum ShapeshiftForm : uint8_t {
        FormCat = 0x01,
        FormTree = 0x02,
        FormTravel = 0x03,
        FormAqua = 0x04,
        FormBear = 0x05,
        FormDireBear = 0x08,
        FormBattleStance = 0x11,
        FormDefensiveStance = 0x12,
        FormBerserkerStance = 0x13,
    };

    bool GetFormFromSpell(uint32_t spellId, uint8_t& form) {
        switch (spellId) {
        case 768:
            form = FormCat;
            return true;
        case 33891:
            form = FormTree;
            return true;
        case 783:
            form = FormTravel;
            return true;
        case 1066:
            form = FormAqua;
            return true;
        case 5487:
            form = FormBear;
            return true;
        case 9634:
            form = FormDireBear;
            return true;
        case 2457:
            form = FormBattleStance;
            return true;
        case 71:
            form = FormDefensiveStance;
            return true;
        case 2458:
            form = FormBerserkerStance;
            return true;
        default:
            return false;
        }
    }

    bool SetObjectValueByte(CGObjectLite* object, uint32_t index, uint8_t offset, uint8_t value) {
        if (!object || offset >= 4) {
            return false;
        }

        auto base = reinterpret_cast<uintptr_t>(object);
        auto* data = *reinterpret_cast<uint32_t**>(base + sizeof(void*));
        if (!data) {
            return false;
        }

        uint32_t& current = data[index];
        uint8_t currentByte = static_cast<uint8_t>((current >> (offset * 8)) & 0xFF);
        if (currentByte == value) {
            return true;
        }

        current &= ~(0xFFu << (offset * 8));
        current |= static_cast<uint32_t>(value) << (offset * 8);
        return true;
    }

    int __cdecl SpellOnCastHook(int spellId, int a2, int a3, int a4, int a5) {
        int result = g_originalSpellOnCast(spellId, a2, a3, a4, a5);

        uint8_t form = 0;
        if (!result || !GetFormFromSpell(static_cast<uint32_t>(spellId), form)) {
            return result;
        }

        CGObjectLite* player = ObjectMgrGet(GetPlayerGuid(), TypeMaskPlayer);
        if (!player) {
            Log("SpellOnCastHook form cast succeeded but player unavailable");
            return result;
        }

        if (!SetObjectValueByte(player, kUnitFieldBytes2, kOffsetShapeshiftForm, form)) {
            Log("SpellOnCastHook failed to update shapeshift byte");
            return result;
        }

        static LONG appliedCount = 0;
        LONG call = InterlockedIncrement(&appliedCount);
        if (call <= 20 || (call % 100) == 0) {
            char line[192];
            sprintf_s(line, "SpellOnCastHook applied call=%ld spell=%d form=0x%02X",
                call,
                spellId,
                form);
            Log(line);
        }

        return result;
    }
}

bool InstallSpellStanceFixHook() {
    BYTE expected[kSpellOnCastExpectedSize] = { 0x55, 0x8B, 0xEC, 0xE8, 0x48, 0x5D, 0xCC, 0xFF };
    return InstallSlimDetoursHook(
        "SpellOnCast",
        Addresses::SpellOnCast,
        expected,
        kSpellOnCastExpectedSize,
        reinterpret_cast<void*>(&SpellOnCastHook),
        reinterpret_cast<void**>(&g_originalSpellOnCast));
}
}
