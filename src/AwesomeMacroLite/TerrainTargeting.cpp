#include "TerrainTargeting.h"

#include "GameClientLite.h"
#include "Log.h"
#include "MacroTargetState.h"
#include "X86Hook.h"

#include <stdio.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr size_t kTerrainPatchSize = 9;

    OnLayerTrackTerrainFn g_originalOnLayerTrackTerrain = nullptr;

    void TerrainClick(float x, float y, float z) {
        TerrainClickEvent event = {};
        event.guid = 0;
        event.pos = { x, y, z };
        event.button = 1;
        HandleTerrainClick(&event);
    }

    bool GetObjectPosition(CGObjectLite* object, C3Vector& pos) {
        if (!object || !object->vtable || !object->vtable[11]) {
            return false;
        }

        auto getPosition = reinterpret_cast<ObjectGetPositionFn>(object->vtable[11]);
        getPosition(object, pos);
        return true;
    }

    int __fastcall OnLayerTrackTerrainHook(void* worldFrame, void*, int* terrainArgs) {
        static LONG calls = 0;
        LONG call = InterlockedIncrement(&calls);

        if (MacroTargetFlagsExpired()) {
            ClearMacroTargetFlags("timeout");
        }

        if (IsPlayerLocationTargetActive()) {
            CGObjectLite* player = ObjectMgrGet(GetPlayerGuid(), TypeMaskPlayer);
            if (player) {
                C3Vector pos = {};
                if (!GetObjectPosition(player, pos)) {
                    Log("OnLayerTrackTerrainHook playerlocation requested but player position unavailable");
                    return g_originalOnLayerTrackTerrain(worldFrame, terrainArgs);
                }

                TerrainClick(pos.x, pos.y, pos.z);
                ClearMacroTargetFlags("playerlocation-click");
                char line[192];
                sprintf_s(line, "OnLayerTrackTerrainHook playerlocation click call=%ld pos=%f,%f,%f",
                    call, pos.x, pos.y, pos.z);
                Log(line);
            }
            else {
                Log("OnLayerTrackTerrainHook playerlocation requested but player unavailable");
            }
        }
        else if (IsCursorTargetActive()) {
            if (terrainArgs) {
                auto* coords = reinterpret_cast<float*>(terrainArgs);
                C3Vector pos = { coords[2], coords[3], coords[4] };
                TerrainClick(pos.x, pos.y, pos.z);
                ClearMacroTargetFlags("cursor-click");

                char line[192];
                sprintf_s(line, "OnLayerTrackTerrainHook cursor click call=%ld pos=%f,%f,%f",
                    call, pos.x, pos.y, pos.z);
                Log(line);
            }
            else {
                Log("OnLayerTrackTerrainHook cursor requested but terrain args were null");
            }
        }

        return g_originalOnLayerTrackTerrain(worldFrame, terrainArgs);
    }
}

bool InstallTerrainTargetingHook() {
    BYTE expected[kTerrainPatchSize] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xA4, 0x00, 0x00, 0x00 };
    return InstallJumpHook(
        "OnLayerTrackTerrain",
        Addresses::OnLayerTrackTerrain,
        expected,
        kTerrainPatchSize,
        reinterpret_cast<void*>(&OnLayerTrackTerrainHook),
        reinterpret_cast<void**>(&g_originalOnLayerTrackTerrain));
}
}
