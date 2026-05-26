#include "StanceFormFix.h"

#include "GameClientLite.h"
#include "Log.h"
#include "X86Hook.h"

#include <stdint.h>
#include <stdio.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr size_t kSpellOnCastPatchSize = 8;

    SpellOnCastFn g_originalSpellOnCast = nullptr;

    struct FormSpell {
        uint32_t spellId;
        uint8_t form;
    };

    constexpr FormSpell kFormSpells[] = {
        { 768, 0x01 },    // Cat Form
        { 783, 0x03 },    // Travel Form
        { 1066, 0x04 },   // Aquatic Form
        { 5487, 0x05 },   // Bear Form
        { 9634, 0x08 },   // Dire Bear Form
        { 33891, 0x02 },  // Tree of Life
        { 2457, 0x11 },   // Battle Stance
        { 71, 0x12 },     // Defensive Stance
        { 2458, 0x13 },   // Berserker Stance
    };

    bool GetFormFromSpell(uint32_t spellId, uint8_t& form) {
        for (const FormSpell& formSpell : kFormSpells) {
            if (formSpell.spellId == spellId) {
                form = formSpell.form;
                return true;
            }
        }
        return false;
    }

    void SetValueByte(CGObjectLite* object, uint32_t field, uint8_t offset, uint8_t value) {
        if (!object || !object->values || offset >= 4) {
            return;
        }

        uint32_t& current = object->values[field];
        uint32_t shift = static_cast<uint32_t>(offset) * 8;
        uint32_t mask = 0xFFu << shift;
        current = (current & ~mask) | (static_cast<uint32_t>(value) << shift);
    }

    int __cdecl SpellOnCastHook(int spellId, int a2, int a3, int a4, int a5) {
        int result = g_originalSpellOnCast(spellId, a2, a3, a4, a5);
        if (!result) {
            return result;
        }

        uint8_t form = 0;
        if (!GetFormFromSpell(static_cast<uint32_t>(spellId), form)) {
            return result;
        }

        CGObjectLite* player = ObjectMgrGet(GetPlayerGuid(), TypeMaskPlayer);
        if (!player) {
            Log("StanceFormFix form cast succeeded but player was unavailable");
            return result;
        }

        SetValueByte(player, UnitFieldBytes2, ShapeshiftFormByteOffset, form);

        static LONG updates = 0;
        LONG update = InterlockedIncrement(&updates);
        if (update <= 20 || (update % 1000) == 0) {
            char line[160];
            sprintf_s(line, "StanceFormFix updated form update=%ld spell=%d form=%u",
                update,
                spellId,
                static_cast<unsigned>(form));
            Log(line);
        }

        return result;
    }
}

bool InstallStanceFormFixHook() {
    BYTE expected[kSpellOnCastPatchSize] = { 0x55, 0x8B, 0xEC, 0xE8, 0x48, 0x5D, 0xCC, 0xFF };
    return InstallJumpHook(
        "SpellOnCast",
        Addresses::SpellOnCast,
        expected,
        kSpellOnCastPatchSize,
        reinterpret_cast<void*>(&SpellOnCastHook),
        reinterpret_cast<void**>(&g_originalSpellOnCast));
}
}
