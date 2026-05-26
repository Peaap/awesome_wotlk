#include "NameplateApi.h"

#include "Log.h"
#include "UnitApiLite.h"
#include "X86Hook.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr uintptr_t kCVarInitialize = 0x007663F0;
    constexpr uintptr_t kCVarGet = 0x00767460;
    constexpr uintptr_t kCVarRegister = 0x00767FC0;
    constexpr uintptr_t kNamePlateInitialize = 0x0098F390;
    constexpr uintptr_t kHideNamePlate = 0x00725840;
    constexpr uintptr_t kUpdateNamePlatePositions = 0x00725890;
    constexpr uintptr_t kSetPoint = 0x0048A260;
    constexpr uintptr_t kSetFrameDepth = 0x0048F5D0;
    constexpr uintptr_t kSetFrameLevel = 0x004910A0;
    constexpr uintptr_t kNamePlateDistanceSquared = 0x00ADAA7C;
    constexpr uintptr_t kWorldFrameGlobal = 0x00B7436C;
    constexpr size_t kWorldFrameRenderDirtyFlagsOffset = 0xB10;
    constexpr size_t kFrameWidthOffset = 0x54;
    constexpr size_t kFrameHeightOffset = 0x58;
    constexpr size_t kFrameDepthOffset = 0x60;
    constexpr size_t kFrameShownOffset = 0xDC;
    constexpr size_t kPlateNdcOffset = 0x2E0;
    constexpr size_t kPlateDepthZOffset = 0x2E8;
    constexpr DWORD kStatusLogIntervalMs = 5000;
    constexpr int kMaxTrackedNameplates = 1024;

    using guid_t = uint64_t;
    using CVarHandlerFn = int(__cdecl*)(void* cvar, const char* previousValue, const char* newValue, void* userData);
    using CVarInitializeFn = void(__cdecl*)();
    using CVarGetFn = void* (__cdecl*)(const char* name);
    using CVarRegisterFn = void* (__cdecl*)(const char* name, const char* description, unsigned flags, const char* defaultValue,
        CVarHandlerFn callback, int a6, int a7, int a8, int a9);
    using NamePlateInitializeFn = int(__thiscall*)(void* plate, void* unit);
    using HideNamePlateFn = void* (__thiscall*)(void* unit);
    using UpdateNamePlatePositionsFn = int(__cdecl*)(void* worldFrame);
    using SetPointFn = void(__thiscall*)(void* frame, int point, void* relativeTo, int relativePoint, float xOffset, float yOffset, int doResize);
    using SetFrameDepthFn = void(__thiscall*)(void* frame, float depth, int flag);
    using SetFrameLevelFn = void(__thiscall*)(void* frame, uint32_t level, int levelChildren);
    struct NamePlateRecord {
        void* plate;
        guid_t guid;
        DWORD lastSeenTick;
        float stackOffsetX;
        float stackOffsetY;
        bool friendly;
        bool hasUnitInfo;
        bool active;
    };

    auto CVarGet = reinterpret_cast<CVarGetFn>(kCVarGet);
    auto CVarRegister = reinterpret_cast<CVarRegisterFn>(kCVarRegister);
    auto SetPoint = reinterpret_cast<SetPointFn>(kSetPoint);
    auto SetFrameDepth = reinterpret_cast<SetFrameDepthFn>(kSetFrameDepth);
    auto SetFrameLevel = reinterpret_cast<SetFrameLevelFn>(kSetFrameLevel);

    CVarInitializeFn OriginalCVarInitialize = nullptr;
    NamePlateInitializeFn OriginalNamePlateInitialize = nullptr;
    HideNamePlateFn OriginalHideNamePlate = nullptr;
    UpdateNamePlatePositionsFn OriginalUpdateNamePlatePositions = nullptr;

    void* CVarNameplateApi = nullptr;
    void* CVarNameplatePositioning = nullptr;
    void* CVarNameplateDistance = nullptr;
    void* CVarNameplateStacking = nullptr;
    void* CVarNameplateClampTop = nullptr;
    void* CVarNameplateRaiseSpeed = nullptr;
    void* CVarNameplateLowerSpeed = nullptr;
    void* CVarNameplatePullDistance = nullptr;
    void* CVarNameplatePlacement = nullptr;
    void* CVarNameplateMouseMode = nullptr;
    void* CVarNameplateBandX = nullptr;
    void* CVarNameplateBandY = nullptr;
    void* CVarNameplateHitboxAnchor = nullptr;
    void* CVarNameplateHitboxWidthE = nullptr;
    void* CVarNameplateHitboxHeightE = nullptr;
    void* CVarNameplateHitboxWidthF = nullptr;
    void* CVarNameplateHitboxHeightF = nullptr;
    void* CVarNameplatePullSpeed = nullptr;
    void* CVarNameplateRaiseDistance = nullptr;
    void* CVarNameplateClampTopOffset = nullptr;
    void* CVarNameplateOcclusionAlpha = nullptr;
    void* CVarNameplateNonTargetAlpha = nullptr;
    void* CVarNameplateAlphaSpeed = nullptr;
    void* CVarNameplateInertia = nullptr;
    void* CVarNameplateHysteresisDecay = nullptr;

    volatile LONG NameplateApiEnabled = 1;
    volatile LONG NameplateStackingMode = 0;
    volatile LONG NameplateDistance = 41;
    volatile LONG NameplateRaiseSpeed = 100;
    volatile LONG NameplateLowerSpeed = 100;
    volatile LONG NameplatePullDistanceMilli = 250;
    volatile LONG NameplateRaiseDistanceMilli = 8000;
    volatile LONG NameplateBandXMilli = 700;
    volatile LONG NameplateBandYMilli = 1000;
    volatile LONG NameplatePlacementMilli = 0;
    volatile LONG NameplateActiveCount = 0;
    volatile LONG NameplateCreatedCount = 0;
    volatile LONG NameplateRemovedCount = 0;
    volatile LONG NameplateUpdateCount = 0;
    volatile LONG NameplateHooksInstalled = 0;
    volatile LONG NameplateCVarsRegistered = 0;
    NamePlateRecord NameplateRecords[kMaxTrackedNameplates] = {};
    DWORD LastStatusLogTick = 0;

    int ClampInt(int value, int minimum, int maximum) {
        if (value < minimum) return minimum;
        if (value > maximum) return maximum;
        return value;
    }

    bool StringToBool(const char* value) {
        if (!value) return false;
        return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' ||
            value[0] == 't' || value[0] == 'T';
    }

    const char* ReadCVarString(void* cvar) {
        if (!cvar) return nullptr;
        return *reinterpret_cast<const char**>(static_cast<BYTE*>(cvar) + 0x28);
    }

    int ReadCVarInt(void* cvar, int fallback, int minimum, int maximum) {
        const char* value = ReadCVarString(cvar);
        if (!value) return fallback;
        return ClampInt(atoi(value), minimum, maximum);
    }

    int ReadCVarFloatMilli(void* cvar, int fallback, int minimum, int maximum) {
        const char* value = ReadCVarString(cvar);
        if (!value) return fallback;
        return ClampInt(static_cast<int>((atof(value) * 1000.0) + 0.5), minimum, maximum);
    }

    bool IsReadable(const void* address, size_t size) {
        if (!address || !size) return false;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(address, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;

        const uintptr_t start = reinterpret_cast<uintptr_t>(address);
        const uintptr_t end = start + size;
        const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return end >= start && end <= regionEnd;
    }

    float ReadFloat(void* base, size_t offset, float fallback = 0.0f) {
        const auto* address = static_cast<BYTE*>(base) + offset;
        if (!IsReadable(address, sizeof(float))) return fallback;
        return *reinterpret_cast<const float*>(address);
    }

    uint32_t ReadU32(void* base, size_t offset, uint32_t fallback = 0) {
        const auto* address = static_cast<BYTE*>(base) + offset;
        if (!IsReadable(address, sizeof(uint32_t))) return fallback;
        return *reinterpret_cast<const uint32_t*>(address);
    }

    void* RegisterNameplateCVar(const char* name, const char* defaultValue) {
        if (void* existing = CVarGet(name)) {
            return existing;
        }

        void* cvar = CVarRegister(name, nullptr, 1, defaultValue, nullptr, 0, 0, 0, 0);
        char line[192];
        sprintf_s(line, "NamePlateAPI registered CVar name=%s ptr=0x%p default=%s", name, cvar, defaultValue);
        Log(line);
        return cvar;
    }

    bool RegisterNameplateCVars(const char* reason) {
        if (InterlockedCompareExchange(const_cast<LONG*>(&NameplateCVarsRegistered), 1, 0) != 0) {
            return true;
        }

        CVarNameplateApi = RegisterNameplateCVar("grimfallNameplateApi", "1");
        CVarNameplatePositioning = RegisterNameplateCVar("grimfallNameplatePositioning", "0");
        CVarNameplateStacking = RegisterNameplateCVar("grimfallNameplateStacking", "0");
        CVarNameplateClampTop = RegisterNameplateCVar("grimfallNameplateClampTop", "0");
        CVarNameplateDistance = RegisterNameplateCVar("grimfallNameplateDistance", "41");
        CVarNameplateRaiseSpeed = RegisterNameplateCVar("grimfallNameplateRaiseSpeed", "100");
        CVarNameplateLowerSpeed = RegisterNameplateCVar("grimfallNameplateLowerSpeed", "100");
        CVarNameplatePullDistance = RegisterNameplateCVar("grimfallNameplatePullDistance", "0.25");
        CVarNameplatePlacement = RegisterNameplateCVar("grimfallNameplatePlacement", "0.0");
        CVarNameplateMouseMode = RegisterNameplateCVar("grimfallNameplateMouseMode", "0");
        CVarNameplateBandX = RegisterNameplateCVar("grimfallNameplateBandX", "0.7");
        CVarNameplateBandY = RegisterNameplateCVar("grimfallNameplateBandY", "1.0");
        CVarNameplateHitboxAnchor = RegisterNameplateCVar("grimfallNameplateHitboxAnchor", "1");
        CVarNameplateHitboxWidthE = RegisterNameplateCVar("grimfallNameplateHitboxWidthE", "1.0");
        CVarNameplateHitboxHeightE = RegisterNameplateCVar("grimfallNameplateHitboxHeightE", "1.0");
        CVarNameplateHitboxWidthF = RegisterNameplateCVar("grimfallNameplateHitboxWidthF", "1.0");
        CVarNameplateHitboxHeightF = RegisterNameplateCVar("grimfallNameplateHitboxHeightF", "1.0");
        CVarNameplatePullSpeed = RegisterNameplateCVar("grimfallNameplatePullSpeed", "50");
        CVarNameplateRaiseDistance = RegisterNameplateCVar("grimfallNameplateRaiseDistance", "8.0");
        CVarNameplateClampTopOffset = RegisterNameplateCVar("grimfallNameplateClampTopOffset", "0.1");
        CVarNameplateOcclusionAlpha = RegisterNameplateCVar("grimfallNameplateOcclusionAlpha", "1.0");
        CVarNameplateNonTargetAlpha = RegisterNameplateCVar("grimfallNameplateNonTargetAlpha", "0.5");
        CVarNameplateAlphaSpeed = RegisterNameplateCVar("grimfallNameplateAlphaSpeed", "0.25");
        CVarNameplateInertia = RegisterNameplateCVar("grimfallNameplateInertia", "1.0");
        CVarNameplateHysteresisDecay = RegisterNameplateCVar("grimfallNameplateHysteresisDecay", "1.0");

        if (!CVarNameplateApi || !CVarNameplateDistance) {
            InterlockedExchange(const_cast<LONG*>(&NameplateCVarsRegistered), 0);
            Log("NamePlateAPI CVar registration failed: critical CVar unavailable");
            return false;
        }

        char line[160];
        sprintf_s(line, "NamePlateAPI CVar registration completed reason=%s", reason ? reason : "unknown");
        Log(line);
        return true;
    }

    void DirtyWorldFrame() {
        void* worldFrame = *reinterpret_cast<void**>(kWorldFrameGlobal);
        if (!worldFrame) return;
        auto* flags = reinterpret_cast<uint32_t*>(static_cast<BYTE*>(worldFrame) + kWorldFrameRenderDirtyFlagsOffset);
        *flags |= 1;
    }

    void ApplyNameplateDistance(int apiEnabled, int distance) {
        int effectiveDistance = apiEnabled ? distance : 41;
        float value = static_cast<float>(effectiveDistance);
        *reinterpret_cast<float*>(kNamePlateDistanceSquared) = value * value;
        DirtyWorldFrame();
    }

    void PollNameplateCVars(bool forceLog = false) {
        int apiEnabled = StringToBool(ReadCVarString(CVarNameplateApi)) ? 1 : 0;
        int positioningEnabled = StringToBool(ReadCVarString(CVarNameplatePositioning)) ? 1 : 0;
        int distance = ReadCVarInt(CVarNameplateDistance, 41, 0, 100);
        int stacking = ReadCVarInt(CVarNameplateStacking, 0, 0, 3);
        int clampTop = ReadCVarInt(CVarNameplateClampTop, 0, 0, 2);
        int raiseSpeed = ReadCVarInt(CVarNameplateRaiseSpeed, 100, 1, 250);
        int lowerSpeed = ReadCVarInt(CVarNameplateLowerSpeed, 100, 1, 250);
        int pullDistance = ReadCVarFloatMilli(CVarNameplatePullDistance, 250, 0, 2000);
        int raiseDistance = ReadCVarFloatMilli(CVarNameplateRaiseDistance, 8000, 1000, 20000);
        int bandX = ReadCVarFloatMilli(CVarNameplateBandX, 700, 100, 1000);
        int bandY = ReadCVarFloatMilli(CVarNameplateBandY, 1000, 100, 1500);
        int placement = ReadCVarFloatMilli(CVarNameplatePlacement, 0, -1000, 2000);

        LONG oldApi = InterlockedExchange(const_cast<LONG*>(&NameplateApiEnabled), apiEnabled);
        LONG oldStacking = InterlockedExchange(const_cast<LONG*>(&NameplateStackingMode), stacking);
        LONG oldDistance = InterlockedExchange(const_cast<LONG*>(&NameplateDistance), distance);
        InterlockedExchange(const_cast<LONG*>(&NameplateRaiseSpeed), raiseSpeed);
        InterlockedExchange(const_cast<LONG*>(&NameplateLowerSpeed), lowerSpeed);
        InterlockedExchange(const_cast<LONG*>(&NameplatePullDistanceMilli), pullDistance);
        InterlockedExchange(const_cast<LONG*>(&NameplateRaiseDistanceMilli), raiseDistance);
        InterlockedExchange(const_cast<LONG*>(&NameplateBandXMilli), bandX);
        InterlockedExchange(const_cast<LONG*>(&NameplateBandYMilli), bandY);
        LONG oldPlacement = InterlockedExchange(const_cast<LONG*>(&NameplatePlacementMilli), placement);
        if (forceLog || oldApi != apiEnabled || oldDistance != distance) {
            ApplyNameplateDistance(apiEnabled, distance);
        }

        if (forceLog || oldApi != apiEnabled || oldStacking != stacking || oldDistance != distance || oldPlacement != placement) {
            char line[320];
            sprintf_s(line,
                "NamePlateAPI CVar poll api=%d positioning=%d distance=%d stacking=%d clampTop=%d raiseSpeed=%d lowerSpeed=%d pullMilli=%d placementMilli=%d",
                apiEnabled, positioningEnabled, distance, stacking, clampTop, raiseSpeed, lowerSpeed, pullDistance, placement);
            Log(line);
        }
    }

    guid_t ReadNamePlateGuid(void* plate) {
        if (!plate) return 0;
        return *reinterpret_cast<guid_t*>(static_cast<BYTE*>(plate) + 0x2A8);
    }

    void* ReadUnitNamePlate(void* unit) {
        if (!unit) return nullptr;
        return *reinterpret_cast<void**>(static_cast<BYTE*>(unit) + 0xC38);
    }

    void TrackNamePlateCreated(void* plate, guid_t guid, const UnitInfoLite* unitInfo) {
        if (!plate) return;

        DWORD now = GetTickCount();
        int freeIndex = -1;
        for (int i = 0; i < kMaxTrackedNameplates; ++i) {
            NamePlateRecord& record = NameplateRecords[i];
            if (record.plate == plate) {
                if (!record.active) {
                    InterlockedIncrement(const_cast<LONG*>(&NameplateActiveCount));
                    InterlockedIncrement(const_cast<LONG*>(&NameplateCreatedCount));
                }
                record.guid = guid;
                record.lastSeenTick = now;
                if (unitInfo) {
                    record.friendly = unitInfo->friendly;
                    record.hasUnitInfo = true;
                }
                record.active = true;
                return;
            }
            if (freeIndex < 0 && !record.active && !record.plate) {
                freeIndex = i;
            }
        }

        if (freeIndex >= 0) {
            NameplateRecords[freeIndex] = {
                plate,
                guid,
                now,
                0.0f,
                0.0f,
                unitInfo ? unitInfo->friendly : false,
                unitInfo != nullptr,
                true
            };
            InterlockedIncrement(const_cast<LONG*>(&NameplateActiveCount));
            InterlockedIncrement(const_cast<LONG*>(&NameplateCreatedCount));
        }
    }

    void TrackNamePlateRemoved(void* plate) {
        if (!plate) return;

        for (int i = 0; i < kMaxTrackedNameplates; ++i) {
            NamePlateRecord& record = NameplateRecords[i];
            if (record.plate == plate && record.active) {
                record.active = false;
                record.lastSeenTick = GetTickCount();
                InterlockedDecrement(const_cast<LONG*>(&NameplateActiveCount));
                InterlockedIncrement(const_cast<LONG*>(&NameplateRemovedCount));
                return;
            }
        }
    }

    bool ShouldStackRecord(const NamePlateRecord& record, int stackingMode) {
        if (stackingMode <= 0) return false;
        if (stackingMode == 1) return true;
        if (!record.hasUnitInfo) return true;
        if (stackingMode == 2) return !record.friendly;
        if (stackingMode == 3) return record.friendly;
        return false;
    }

    void ResetStackOffsets(NamePlateRecord& record) {
        record.stackOffsetX = 0.0f;
        record.stackOffsetY = 0.0f;
    }

    void ApplyNameplateStacking(void* worldFrame) {
        if (!worldFrame) return;
        if (!InterlockedCompareExchange(const_cast<LONG*>(&NameplateApiEnabled), 0, 0)) return;

        int stackingMode = InterlockedCompareExchange(const_cast<LONG*>(&NameplateStackingMode), 0, 0);
        if (stackingMode <= 0) {
            for (int i = 0; i < kMaxTrackedNameplates; ++i) {
                ResetStackOffsets(NameplateRecords[i]);
            }
            return;
        }

        NamePlateRecord* plates[kMaxTrackedNameplates];
        int count = 0;
        for (int i = 0; i < kMaxTrackedNameplates; ++i) {
            NamePlateRecord& record = NameplateRecords[i];
            if (!record.active || !record.plate) continue;
            if (!ShouldStackRecord(record, stackingMode)) {
                ResetStackOffsets(record);
                continue;
            }
            if (!IsReadable(record.plate, 0x300)) continue;
            if (!ReadU32(record.plate, kFrameShownOffset)) continue;
            plates[count++] = &record;
            if (count >= kMaxTrackedNameplates) break;
        }

        if (count <= 1) return;

        for (int i = 1; i < count; ++i) {
            NamePlateRecord* key = plates[i];
            float keyY = ReadFloat(key->plate, kPlateNdcOffset + sizeof(float));
            int j = i - 1;
            while (j >= 0 && ReadFloat(plates[j]->plate, kPlateNdcOffset + sizeof(float)) < keyY) {
                plates[j + 1] = plates[j];
                --j;
            }
            plates[j + 1] = key;
        }

        float bandX = static_cast<float>(InterlockedCompareExchange(const_cast<LONG*>(&NameplateBandXMilli), 0, 0)) / 1000.0f;
        float bandY = static_cast<float>(InterlockedCompareExchange(const_cast<LONG*>(&NameplateBandYMilli), 0, 0)) / 1000.0f;
        float pullDistance = static_cast<float>(InterlockedCompareExchange(const_cast<LONG*>(&NameplatePullDistanceMilli), 0, 0)) / 1000.0f;
        float raiseDistance = static_cast<float>(InterlockedCompareExchange(const_cast<LONG*>(&NameplateRaiseDistanceMilli), 0, 0)) / 1000.0f;
        float raiseSpeed = static_cast<float>(InterlockedCompareExchange(const_cast<LONG*>(&NameplateRaiseSpeed), 0, 0));
        float lowerSpeed = static_cast<float>(InterlockedCompareExchange(const_cast<LONG*>(&NameplateLowerSpeed), 0, 0));
        float sceneTime = 0.016f;
        if (IsReadable(static_cast<BYTE*>(worldFrame) + 0x18, sizeof(float))) {
            float candidate = ReadFloat(worldFrame, 0x18, sceneTime);
            if (candidate > 0.0f && candidate < 0.1f) {
                sceneTime = candidate;
            }
        }

        float targetY[kMaxTrackedNameplates];
        for (int i = 0; i < count; ++i) {
            targetY[i] = 0.0f;
        }

        for (int i = 0; i < count; ++i) {
            void* plate = plates[i]->plate;
            float baseX = ReadFloat(plate, kPlateNdcOffset);
            float baseY = ReadFloat(plate, kPlateNdcOffset + sizeof(float));
            float width = ReadFloat(plate, kFrameWidthOffset, 0.1f);
            float height = ReadFloat(plate, kFrameHeightOffset, 0.025f);
            float maxRaise = height * raiseDistance;

            for (int j = 0; j < i; ++j) {
                void* other = plates[j]->plate;
                float otherX = ReadFloat(other, kPlateNdcOffset);
                float otherY = ReadFloat(other, kPlateNdcOffset + sizeof(float)) + targetY[j];
                float otherWidth = ReadFloat(other, kFrameWidthOffset, 0.1f);
                float otherHeight = ReadFloat(other, kFrameHeightOffset, 0.025f);
                float minX = ((width + otherWidth) * 0.5f) * bandX;
                if (fabsf(baseX - otherX) > minX) continue;

                float minY = ((height + otherHeight) * 0.5f) * bandY;
                float requiredY = otherY + minY - baseY;
                if (requiredY > targetY[i]) {
                    targetY[i] = requiredY;
                }
            }

            if (targetY[i] > maxRaise) {
                targetY[i] = maxRaise;
            }
        }

        uint32_t frameLevel = static_cast<uint32_t>(count * 10);
        for (int i = 0; i < count; ++i) {
            NamePlateRecord& record = *plates[i];
            void* plate = record.plate;
            float speed = (targetY[i] > record.stackOffsetY) ? raiseSpeed : lowerSpeed;
            float alpha = speed * sceneTime;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;

            record.stackOffsetY += (targetY[i] - record.stackOffsetY) * alpha;
            float maxPull = ReadFloat(plate, kFrameWidthOffset, 0.1f) * pullDistance;
            if (record.stackOffsetX > maxPull) record.stackOffsetX = maxPull;
            if (record.stackOffsetX < -maxPull) record.stackOffsetX = -maxPull;

            float x = ReadFloat(plate, kPlateNdcOffset) + record.stackOffsetX;
            float y = ReadFloat(plate, kPlateNdcOffset + sizeof(float)) + record.stackOffsetY;
            SetPoint(plate, 1, worldFrame, 6, x, y, 1);

            float depth = ReadFloat(plate, kPlateDepthZOffset) - ReadFloat(worldFrame, kFrameDepthOffset);
            SetFrameDepth(plate, depth, 1);
            SetFrameLevel(plate, frameLevel, 1);
            if (frameLevel >= 10) frameLevel -= 10;
        }
        DirtyWorldFrame();
    }

    void LogNameplateStatusIfNeeded() {
        DWORD now = GetTickCount();
        if (LastStatusLogTick != 0 && now - LastStatusLogTick < kStatusLogIntervalMs) {
            return;
        }
        LastStatusLogTick = now;

        char line[192];
        sprintf_s(line, "NamePlateAPI status active=%ld created=%ld removed=%ld updates=%ld hooks=%ld",
            InterlockedCompareExchange(const_cast<LONG*>(&NameplateActiveCount), 0, 0),
            InterlockedCompareExchange(const_cast<LONG*>(&NameplateCreatedCount), 0, 0),
            InterlockedCompareExchange(const_cast<LONG*>(&NameplateRemovedCount), 0, 0),
            InterlockedCompareExchange(const_cast<LONG*>(&NameplateUpdateCount), 0, 0),
            InterlockedCompareExchange(const_cast<LONG*>(&NameplateHooksInstalled), 0, 0));
        Log(line);
    }

    void __cdecl CVarInitializeHook() {
        OriginalCVarInitialize();
        if (RegisterNameplateCVars("CVarInitialize")) {
            PollNameplateCVars(true);
        }
    }

    int __fastcall NamePlateInitializeHook(void* plate, void*, void* unit) {
        int result = OriginalNamePlateInitialize(plate, unit);
        guid_t guid = ReadNamePlateGuid(plate);

        LONG created = InterlockedCompareExchange(const_cast<LONG*>(&NameplateCreatedCount), 0, 0);
        UnitInfoLite info = {};
        bool hasInfo = TryGetUnitInfo(unit, info);
        TrackNamePlateCreated(plate, guid, hasInfo ? &info : nullptr);

        if (created <= 25 || (created % 100) == 0) {
            char line[320];
            if (hasInfo) {
                sprintf_s(line,
                    "NamePlateAPI created plate=0x%p unit=0x%p guid=0x%08X%08X rank=%d reaction=%d enemy=%d friendly=%d flags=0x%08X npcFlags=0x%08X active=%ld",
                    plate,
                    unit,
                    static_cast<unsigned>(info.guid >> 32),
                    static_cast<unsigned>(info.guid & 0xFFFFFFFF),
                    info.rank,
                    info.reaction,
                    info.enemy ? 1 : 0,
                    info.friendly ? 1 : 0,
                    info.flags,
                    info.npcFlags,
                    InterlockedCompareExchange(const_cast<LONG*>(&NameplateActiveCount), 0, 0));
            }
            else {
                sprintf_s(line,
                    "NamePlateAPI created plate=0x%p unit=0x%p guid=0x%08X%08X unitInfo=unavailable active=%ld",
                    plate,
                    unit,
                    static_cast<unsigned>(guid >> 32),
                    static_cast<unsigned>(guid & 0xFFFFFFFF),
                    InterlockedCompareExchange(const_cast<LONG*>(&NameplateActiveCount), 0, 0));
            }
            Log(line);
        }
        return result;
    }

    void* __fastcall HideNamePlateHook(void* unit, void*) {
        void* plate = ReadUnitNamePlate(unit);
        void* result = OriginalHideNamePlate(unit);
        TrackNamePlateRemoved(plate);
        return result;
    }

    int __cdecl UpdateNamePlatePositionsHook(void* worldFrame) {
        InterlockedIncrement(const_cast<LONG*>(&NameplateUpdateCount));
        PollNameplateCVars();
        int result = OriginalUpdateNamePlatePositions(worldFrame);
        ApplyNameplateStacking(worldFrame);
        LogNameplateStatusIfNeeded();
        return result;
    }

}

void InstallNameplateApiHooks() {
    const BYTE cvarInitializeExpected[] = { 0xC6, 0x05, 0xFA, 0x19, 0xCA, 0x00, 0x01, 0xC3 };
    if (InstallJumpHook("CVar::Initialize", kCVarInitialize, cvarInitializeExpected, sizeof(cvarInitializeExpected),
        reinterpret_cast<void*>(&CVarInitializeHook), reinterpret_cast<void**>(&OriginalCVarInitialize))) {
        InterlockedIncrement(const_cast<LONG*>(&NameplateHooksInstalled));
    }

    const BYTE updateExpected[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18, 0x53, 0x8B, 0x1D, 0x88, 0xAA, 0xAD, 0x00 };
    if (InstallJumpHook("UpdateNamePlatePositions", kUpdateNamePlatePositions, updateExpected, sizeof(updateExpected),
        reinterpret_cast<void*>(&UpdateNamePlatePositionsHook), reinterpret_cast<void**>(&OriginalUpdateNamePlatePositions))) {
        InterlockedIncrement(const_cast<LONG*>(&NameplateHooksInstalled));
    }

    const BYTE initializeExpected[] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C, 0x53, 0x8B, 0x5D, 0x08 };
    if (InstallJumpHook("NamePlateInitialize", kNamePlateInitialize, initializeExpected, sizeof(initializeExpected),
        reinterpret_cast<void*>(&NamePlateInitializeHook), reinterpret_cast<void**>(&OriginalNamePlateInitialize))) {
        InterlockedIncrement(const_cast<LONG*>(&NameplateHooksInstalled));
    }

    const BYTE hideExpected[] = { 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x38, 0x0C, 0x00, 0x00 };
    if (InstallJumpHook("HideNamePlate", kHideNamePlate, hideExpected, sizeof(hideExpected),
        reinterpret_cast<void*>(&HideNamePlateHook), reinterpret_cast<void**>(&OriginalHideNamePlate))) {
        InterlockedIncrement(const_cast<LONG*>(&NameplateHooksInstalled));
    }

    if (RegisterNameplateCVars("fallback")) {
        PollNameplateCVars(true);
    }
}
}
