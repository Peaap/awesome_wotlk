#include "UnitApiLite.h"

#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr uint32_t kTypeMaskUnit = 0x8;
    constexpr uint32_t kTypeMaskPlayer = 0x10;

    constexpr size_t kObjectEntryOffset = 0x08;
    constexpr size_t kUnitNameplateOffset = 0xC38;

    constexpr size_t kUnitEntryFlagsOffset = 0xEC;
    constexpr size_t kUnitEntryNpcFlagsOffset = 0x148;

    using ObjectMgrGetFn = void* (__cdecl*)(guid_t guid, uint32_t typeMask);
    using UnitReactionFn = int(__thiscall*)(void* unit, void* other);
    using CanAttackFn = bool(__thiscall*)(void* unit, void* other);
    using CanAssistFn = bool(__thiscall*)(void* unit, void* other, bool ignoreFlags);
    using GetCreatureRankFn = int(__thiscall*)(void* unit);

    auto ObjectMgrGetUnit = reinterpret_cast<ObjectMgrGetFn>(Addresses::ObjectMgrGet);
    auto UnitReaction = reinterpret_cast<UnitReactionFn>(0x007251C0);
    auto CanAttack = reinterpret_cast<CanAttackFn>(0x00729A70);
    auto CanAssist = reinterpret_cast<CanAssistFn>(0x007293D0);
    auto GetCreatureRank = reinterpret_cast<GetCreatureRankFn>(0x00718A00);

    bool IsReadable(const void* address, size_t size) {
        if (!address || size == 0) {
            return false;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(address, &mbi, sizeof(mbi)) == 0) {
            return false;
        }

        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            return false;
        }

        uintptr_t begin = reinterpret_cast<uintptr_t>(address);
        uintptr_t regionBegin = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        uintptr_t regionEnd = regionBegin + mbi.RegionSize;
        return begin >= regionBegin && begin + size <= regionEnd;
    }

    bool TryReadPtr(void* base, size_t offset, void*& value) {
        auto* address = static_cast<unsigned char*>(base) + offset;
        if (!IsReadable(address, sizeof(void*))) {
            return false;
        }

        value = *reinterpret_cast<void**>(address);
        return true;
    }

    bool TryReadU32(void* base, size_t offset, uint32_t& value) {
        auto* address = static_cast<unsigned char*>(base) + offset;
        if (!IsReadable(address, sizeof(uint32_t))) {
            return false;
        }

        value = *reinterpret_cast<uint32_t*>(address);
        return true;
    }

    bool TryReadGuid(void* base, size_t offset, guid_t& value) {
        auto* address = static_cast<unsigned char*>(base) + offset;
        if (!IsReadable(address, sizeof(guid_t))) {
            return false;
        }

        value = *reinterpret_cast<guid_t*>(address);
        return true;
    }
}

bool TryGetUnitInfo(void* unit, UnitInfoLite& info) {
    info = {};
    if (!IsReadable(unit, kUnitNameplateOffset + sizeof(void*))) {
        return false;
    }

    void* entry = nullptr;
    if (!TryReadPtr(unit, kObjectEntryOffset, entry) || !IsReadable(entry, kUnitEntryNpcFlagsOffset + sizeof(uint32_t))) {
        return false;
    }

    guid_t guid = 0;
    if (!TryReadGuid(entry, 0, guid) || guid == 0) {
        return false;
    }

    info.unit = unit;
    info.guid = guid;
    TryReadPtr(unit, kUnitNameplateOffset, info.nameplate);
    TryReadU32(entry, kUnitEntryFlagsOffset, info.flags);
    TryReadU32(entry, kUnitEntryNpcFlagsOffset, info.npcFlags);
    info.rank = GetCreatureRank(unit);

    void* player = ObjectMgrGetUnit(GetPlayerGuid(), kTypeMaskPlayer);
    if (player && player != unit && IsReadable(player, kUnitNameplateOffset + sizeof(void*))) {
        info.reaction = UnitReaction(player, unit);
        info.canAttack = CanAttack(player, unit);
        info.canAssist = CanAssist(player, unit, false);
        info.friendly = info.reaction >= 5 || (info.reaction == 4 && !info.canAttack);
        info.enemy = info.canAttack || info.reaction <= 3;
    }

    return true;
}

bool TryGetUnitInfoByGuid(guid_t guid, UnitInfoLite& info) {
    if (guid == 0) {
        info = {};
        return false;
    }

    void* unit = ObjectMgrGetUnit(guid, kTypeMaskUnit | kTypeMaskPlayer);
    if (!unit) {
        info = {};
        return false;
    }

    return TryGetUnitInfo(unit, info);
}
}
