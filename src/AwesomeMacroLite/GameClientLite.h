#pragma once

#include <stdint.h>

namespace AwesomeMacroLite {
    using guid_t = uint64_t;

    struct C3Vector {
        float x;
        float y;
        float z;
    };

    struct TerrainClickEvent {
        guid_t guid;
        C3Vector pos;
        uint32_t button;
    };

    struct CGObjectLite {
        void** vtable;
    };

    namespace Addresses {
        constexpr uintptr_t SecureCmdOptionParse = 0x00564AE0;
        constexpr uintptr_t OnLayerTrackTerrain = 0x004F66C0;
        constexpr uintptr_t LuaGetTop = 0x0084DBD0;
        constexpr uintptr_t LuaType = 0x0084DEB0;
        constexpr uintptr_t LuaToLString = 0x0084E0E0;
        constexpr uintptr_t LuaSetTop = 0x0084DBF0;
        constexpr uintptr_t LuaPushLString = 0x0084E300;
        constexpr uintptr_t LuaPushNil = 0x0084E280;
        constexpr uintptr_t GetPlayerGuid = 0x004D3790;
        constexpr uintptr_t ObjectMgrGet = 0x004D4DB0;
        constexpr uintptr_t HandleTerrainClick = 0x00527830;
    }

    constexpr int LuaStringType = 4;
    constexpr uint32_t TypeMaskPlayer = 0x10;

    using SecureCmdOptionParseFn = int(__cdecl*)(void* luaState);
    using OnLayerTrackTerrainFn = int(__thiscall*)(void* worldFrame, int* terrainArgs);
    using LuaGetTopFn = int(__cdecl*)(void* luaState);
    using LuaTypeFn = int(__cdecl*)(void* luaState, int index);
    using LuaToLStringFn = const char* (__cdecl*)(void* luaState, int index, size_t* length);
    using LuaSetTopFn = void(__cdecl*)(void* luaState, int index);
    using LuaPushLStringFn = void(__cdecl*)(void* luaState, const char* value, size_t length);
    using LuaPushNilFn = void(__cdecl*)(void* luaState);
    using GetPlayerGuidFn = guid_t(__cdecl*)();
    using ObjectMgrGetFn = CGObjectLite* (__cdecl*)(guid_t guid, uint32_t typeMask);
    using HandleTerrainClickFn = void(__cdecl*)(TerrainClickEvent* event);
    using ObjectGetPositionFn = C3Vector&(__thiscall*)(CGObjectLite* object, C3Vector& pos);

    inline auto LuaGetTop = reinterpret_cast<LuaGetTopFn>(Addresses::LuaGetTop);
    inline auto LuaType = reinterpret_cast<LuaTypeFn>(Addresses::LuaType);
    inline auto LuaToLString = reinterpret_cast<LuaToLStringFn>(Addresses::LuaToLString);
    inline auto LuaSetTop = reinterpret_cast<LuaSetTopFn>(Addresses::LuaSetTop);
    inline auto LuaPushLString = reinterpret_cast<LuaPushLStringFn>(Addresses::LuaPushLString);
    inline auto LuaPushNil = reinterpret_cast<LuaPushNilFn>(Addresses::LuaPushNil);
    inline auto GetPlayerGuid = reinterpret_cast<GetPlayerGuidFn>(Addresses::GetPlayerGuid);
    inline auto ObjectMgrGet = reinterpret_cast<ObjectMgrGetFn>(Addresses::ObjectMgrGet);
    inline auto HandleTerrainClick = reinterpret_cast<HandleTerrainClickFn>(Addresses::HandleTerrainClick);
}
