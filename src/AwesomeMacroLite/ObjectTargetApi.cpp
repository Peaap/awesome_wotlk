#include "ObjectTargetApi.h"

#include "GameClientLite.h"
#include "Log.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr uint32_t kTypeMaskObject = 0x1;
    constexpr uint32_t kTypeMaskItem = 0x2;
    constexpr uint32_t kTypeMaskContainer = 0x4;
    constexpr uint32_t kTypeMaskUnit = 0x8;
    constexpr uint32_t kTypeMaskPlayer = 0x10;
    constexpr uint32_t kTypeMaskGameObject = 0x20;
    constexpr uint32_t kTypeMaskDynamicObject = 0x40;
    constexpr uint32_t kTypeMaskCorpse = 0x80;
    constexpr uint32_t kTypeMaskAny = kTypeMaskObject | kTypeMaskItem | kTypeMaskContainer |
        kTypeMaskUnit | kTypeMaskPlayer | kTypeMaskGameObject | kTypeMaskDynamicObject | kTypeMaskCorpse;

    DWORD LastRegisterTick = 0;

    void PushGuidString(void* luaState, guid_t guid) {
        if (!guid) {
            LuaPushNil(luaState);
            return;
        }

        char value[17] = {};
        sprintf_s(value, "%08X%08X",
            static_cast<unsigned>(guid >> 32),
            static_cast<unsigned>(guid & 0xFFFFFFFF));
        LuaPushLString(luaState, value, strlen(value));
    }

    bool TryParseGuid(const char* value, size_t length, guid_t& guid) {
        guid = 0;
        if (!value || length == 0 || length > 18) {
            return false;
        }

        size_t offset = 0;
        if (length > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
            offset = 2;
        }

        if (length - offset == 0 || length - offset > 16) {
            return false;
        }

        guid_t parsed = 0;
        for (size_t i = offset; i < length; ++i) {
            char c = value[i];
            unsigned digit = 0;
            if (c >= '0' && c <= '9') {
                digit = static_cast<unsigned>(c - '0');
            }
            else if (c >= 'a' && c <= 'f') {
                digit = static_cast<unsigned>(c - 'a' + 10);
            }
            else if (c >= 'A' && c <= 'F') {
                digit = static_cast<unsigned>(c - 'A' + 10);
            }
            else {
                return false;
            }
            parsed = (parsed << 4) | digit;
        }

        guid = parsed;
        return guid != 0;
    }

    int __cdecl LuaGetPlayerGuid(void* luaState) {
        PushGuidString(luaState, GetPlayerGuid());
        return 1;
    }

    int __cdecl LuaGetTargetGuid(void* luaState) {
        PushGuidString(luaState, GetGuidByKeyword("target"));
        return 1;
    }

    int __cdecl LuaGetMouseoverGuid(void* luaState) {
        guid_t guid = 0;
        if (!GetMouseoverGuid(&guid)) {
            LuaPushNil(luaState);
            return 1;
        }

        PushGuidString(luaState, guid);
        return 1;
    }

    int __cdecl LuaObjectExists(void* luaState) {
        size_t length = 0;
        const char* value = LuaToLString(luaState, 1, &length);
        guid_t guid = 0;
        if (!TryParseGuid(value, length, guid)) {
            LuaPushBoolean(luaState, 0);
            return 1;
        }

        LuaPushBoolean(luaState, ObjectMgrGet(guid, kTypeMaskAny) ? 1 : 0);
        return 1;
    }
}

bool RegisterObjectTargetApi() {
    DWORD now = GetTickCount();
    if (LastRegisterTick != 0 && now - LastRegisterTick < 5000) {
        return true;
    }
    LastRegisterTick = now;

    FrameScriptRegisterFunction("GML_GetPlayerGUID", &LuaGetPlayerGuid);
    FrameScriptRegisterFunction("GML_GetTargetGUID", &LuaGetTargetGuid);
    FrameScriptRegisterFunction("GML_GetMouseoverGUID", &LuaGetMouseoverGuid);
    FrameScriptRegisterFunction("GML_ObjectExists", &LuaObjectExists);
    Log("ObjectTargetAPI registered Lua functions");
    return true;
}
}
