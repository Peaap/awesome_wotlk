#include "CameraApiLite.h"

#include "GameClientLite.h"
#include "Log.h"
#include "X86Hook.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr uintptr_t kCVarGet = 0x00767460;
    constexpr uintptr_t kCVarRegister = 0x00767FC0;
    constexpr uintptr_t kGetActiveCamera = 0x004F5960;
    constexpr uintptr_t kWorldFrameIntersect = 0x0077F310;
    constexpr uintptr_t kIntersectCallSite = 0x006060E6;
    constexpr uintptr_t kIntersectCallJumpBack = 0x00606103;
    constexpr uintptr_t kIterateCollisionListSite = 0x007A279D;
    constexpr uintptr_t kIterateCollisionListJumpBack = 0x007A27A5;
    constexpr uintptr_t kIterateCollisionListSkip = 0x007A2943;
    constexpr uintptr_t kIterateWorldObjCollisionListSite = 0x007A2A1C;
    constexpr uintptr_t kIterateWorldObjCollisionListJumpBack = 0x007A2A23;
    constexpr uintptr_t kIterateWorldObjCollisionListSkip = 0x007A2A8A;
    constexpr size_t kCVarValueOffset = 0x28;
    constexpr size_t kCameraFovOffset = 0x40;
    constexpr size_t kPlayerScaleXOffset = 0x98;
    constexpr size_t kModelAlphaOffset = 0x17C;
    constexpr DWORD kPollIntervalMs = 250;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr int kMaxIndirectModels = 64;

    using CVarHandlerFn = int(__cdecl*)(void* cvar, const char* previousValue, const char* newValue, void* userData);
    using CVarGetFn = void* (__cdecl*)(const char* name);
    using CVarRegisterFn = void* (__cdecl*)(const char* name, const char* description, unsigned flags, const char* defaultValue,
        CVarHandlerFn callback, int a6, int a7, int a8, int a9);
    using GetActiveCameraFn = void* (__cdecl*)();
    using WorldFrameIntersectFn = char(__cdecl*)(C3Vector* playerPos, C3Vector* cameraPos, C3Vector* hitPoint,
        float* hitDistance, uint32_t hitFlags, uintptr_t buffer);

    auto CVarGet = reinterpret_cast<CVarGetFn>(kCVarGet);
    auto CVarRegister = reinterpret_cast<CVarRegisterFn>(kCVarRegister);
    auto GetActiveCamera = reinterpret_cast<GetActiveCameraFn>(kGetActiveCamera);
    auto WorldFrameIntersect = reinterpret_cast<WorldFrameIntersectFn>(kWorldFrameIntersect);

    void* CVarCameraFov = nullptr;
    void* CVarCameraIndirectVisibility = nullptr;
    void* CVarCameraIndirectAlpha = nullptr;
    void* CVarShowPlayer = nullptr;
    void* CVarCameraProbeSeq = nullptr;
    volatile LONG CameraCVarsRegistered = 0;
    DWORD LastPollTick = 0;
    int LastFovMilli = 0;
    int LastShowPlayer = -1;
    int LastProbeSeq = -1;
    int LastIndirectVisibility = -1;
    int LastIndirectAlphaMilli = 0;
    volatile LONG CameraIndirectHooksInstalled = 0;
    volatile LONG CameraIndirectVisibilityEnabled = 0;
    float CameraIndirectAlpha = 1.0f;
    float ActualCameraDistance = 1.0f;
    void* OriginalIntersectCall = nullptr;
    void* OriginalIterateCollisionList = nullptr;
    void* OriginalIterateWorldObjCollisionList = nullptr;

    struct IndirectModelRecord {
        void* model;
        float originalAlpha;
        uint32_t grace;
        bool fading;
        bool current;
    };

    IndirectModelRecord IndirectModels[kMaxIndirectModels] = {};

    bool IsReadable(void* address, size_t size) {
        if (!address || !size) return false;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(address, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;

        DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if ((mbi.Protect & readable) == 0) return false;

        uintptr_t start = reinterpret_cast<uintptr_t>(address);
        uintptr_t end = start + size;
        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return end >= start && end <= regionEnd;
    }

    bool IsWritable(void* address, size_t size) {
        if (!IsReadable(address, size)) return false;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(address, &mbi, sizeof(mbi))) return false;

        DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if ((mbi.Protect & writable) == 0) return false;

        uintptr_t start = reinterpret_cast<uintptr_t>(address);
        uintptr_t end = start + size;
        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return end >= start && end <= regionEnd;
    }

    int ClampInt(int value, int minimum, int maximum) {
        if (value < minimum) return minimum;
        if (value > maximum) return maximum;
        return value;
    }

    const char* ReadCVarString(void* cvar) {
        if (!cvar) return nullptr;
        return *reinterpret_cast<const char**>(static_cast<BYTE*>(cvar) + kCVarValueOffset);
    }

    int ReadCVarFloatMilli(void* cvar, int fallback, int minimum, int maximum) {
        const char* value = ReadCVarString(cvar);
        if (!value) return fallback;
        return ClampInt(static_cast<int>((atof(value) * 1000.0) + 0.5), minimum, maximum);
    }

    int ReadCVarInt(void* cvar, int fallback, int minimum, int maximum) {
        const char* value = ReadCVarString(cvar);
        if (!value) return fallback;
        return ClampInt(atoi(value), minimum, maximum);
    }

    float ReadCVarFloat(void* cvar, float fallback, float minimum, float maximum) {
        const char* value = ReadCVarString(cvar);
        if (!value) return fallback;
        float parsed = static_cast<float>(atof(value));
        if (parsed < minimum) return minimum;
        if (parsed > maximum) return maximum;
        return parsed;
    }

    void* RegisterCameraCVar(const char* name, const char* defaultValue) {
        if (void* existing = CVarGet(name)) {
            return existing;
        }

        void* cvar = CVarRegister(name, nullptr, 1, defaultValue, nullptr, 0, 0, 0, 0);
        char line[160];
        sprintf_s(line, "CameraAPI registered CVar name=%s ptr=0x%p default=%s", name, cvar, defaultValue);
        Log(line);
        return cvar;
    }

    void ApplyCameraFov(int fovMilli) {
        void* camera = GetActiveCamera();
        if (!camera) return;

        auto* fov = reinterpret_cast<float*>(static_cast<BYTE*>(camera) + kCameraFovOffset);
        if (!IsWritable(fov, sizeof(float))) return;

        float degrees = static_cast<float>(fovMilli) / 1000.0f;
        *fov = (kPi / 200.0f) * degrees;
    }

    void ApplyShowPlayer(int showPlayer) {
        guid_t playerGuid = GetPlayerGuid();
        if (!playerGuid) return;

        CGObjectLite* player = ObjectMgrGet(playerGuid, TypeMaskPlayer);
        if (!player) return;

        auto* scaleX = reinterpret_cast<float*>(reinterpret_cast<BYTE*>(player) + kPlayerScaleXOffset);
        if (!IsWritable(scaleX, sizeof(float))) return;

        *scaleX = showPlayer ? 1.0f : 0.0f;
    }

    float* ModelAlpha(void* model) {
        if (!model) return nullptr;
        auto* alpha = reinterpret_cast<float*>(static_cast<BYTE*>(model) + kModelAlphaOffset);
        if (!IsWritable(alpha, sizeof(float))) return nullptr;
        return alpha;
    }

    IndirectModelRecord* FindIndirectModel(void* model) {
        if (!model) return nullptr;
        for (int i = 0; i < kMaxIndirectModels; ++i) {
            if (IndirectModels[i].model == model) {
                return &IndirectModels[i];
            }
        }
        return nullptr;
    }

    IndirectModelRecord* GetOrAddIndirectModel(void* model) {
        if (IndirectModelRecord* existing = FindIndirectModel(model)) {
            return existing;
        }

        float* alpha = ModelAlpha(model);
        if (!alpha) return nullptr;

        for (int i = 0; i < kMaxIndirectModels; ++i) {
            if (!IndirectModels[i].model) {
                IndirectModels[i] = { model, *alpha, 0, true, false };
                return &IndirectModels[i];
            }
        }
        return nullptr;
    }

    bool IsCurrentIndirectModel(void* model) {
        if (IndirectModelRecord* record = FindIndirectModel(model)) {
            return record->current;
        }
        return false;
    }

    void ClearCurrentIndirectModels() {
        for (int i = 0; i < kMaxIndirectModels; ++i) {
            IndirectModels[i].current = false;
        }
    }

    void RestoreIndirectModels() {
        for (int i = 0; i < kMaxIndirectModels; ++i) {
            IndirectModelRecord& record = IndirectModels[i];
            if (!record.model) continue;
            if (float* alpha = ModelAlpha(record.model)) {
                *alpha = record.originalAlpha;
            }
            record = {};
        }
        ActualCameraDistance = 1.0f;
    }

    void TickIndirectModelFade() {
        uint32_t cleanupTimer = 1;
        for (int i = 0; i < kMaxIndirectModels; ++i) {
            if (IndirectModels[i].model) {
                ++cleanupTimer;
            }
        }

        for (int i = 0; i < kMaxIndirectModels; ++i) {
            IndirectModelRecord& record = IndirectModels[i];
            if (!record.model) continue;

            float* alpha = ModelAlpha(record.model);
            if (!alpha) {
                record = {};
                continue;
            }

            if (record.current) {
                *alpha += (CameraIndirectAlpha - *alpha) * 0.25f;
            }
            else if (record.grace > cleanupTimer) {
                *alpha += (record.originalAlpha - *alpha) * 0.25f;
                if (fabsf(*alpha - record.originalAlpha) < 0.01f) {
                    *alpha = record.originalAlpha;
                    record = {};
                    continue;
                }
            }

            ++record.grace;
        }
    }

    char __cdecl CameraIntersectHook(C3Vector* playerPos, C3Vector* cameraPos, C3Vector* hitPoint,
        float* hitDistance, uint32_t hitFlags, uintptr_t buffer) {
        memset(reinterpret_cast<void*>(buffer), 0, 2048);

        char result = WorldFrameIntersect(playerPos, cameraPos, hitPoint, hitDistance,
            hitFlags + 1 + 0x8000000, buffer);
        if (result) {
            const auto* bufferData = reinterpret_cast<const uint32_t*>(buffer);
            if ((bufferData[0] == 1 || bufferData[0] == 2) && bufferData[1] > 0) {
                void* model = *reinterpret_cast<void**>(buffer + 12 + 80);
                if (IndirectModelRecord* record = GetOrAddIndirectModel(model)) {
                    record->current = true;
                    record->fading = true;
                    record->grace = 0;

                    if (float* alpha = ModelAlpha(model)) {
                        *alpha += (CameraIndirectAlpha - *alpha) * 0.25f;
                    }

                    if (ActualCameraDistance < 1.0f) {
                        *hitDistance = ActualCameraDistance;
                    }
                    else {
                        result = 0;
                    }
                }
            }
            else {
                ClearCurrentIndirectModels();
                ActualCameraDistance = *hitDistance;
            }
        }
        else {
            ClearCurrentIndirectModels();
            ActualCameraDistance = 1.0f;
        }

        TickIndirectModelFade();
        return result;
    }

    bool __cdecl CameraCollisionFilter(void* model) {
        if (!InterlockedCompareExchange(const_cast<LONG*>(&CameraIndirectVisibilityEnabled), 0, 0)) {
            return true;
        }
        return !IsCurrentIndirectModel(model);
    }

    void __declspec(naked) IntersectCallHook() {
        __asm {
            cmp dword ptr [CameraIndirectVisibilityEnabled], 0
            jne indirect_enabled

            fld1
            push 0
            push ebx
            fstp dword ptr [ebp + 0x18]
            lea ecx, [ebp + 0x18]
            push ecx
            lea edx, [ebp - 0x48]
            push edx
            lea eax, [ebp - 0x18]
            push eax
            lea ecx, [ebp - 0x24]
            push ecx
            mov eax, kWorldFrameIntersect
            call eax
            push kIntersectCallJumpBack
            ret

        indirect_enabled:
            sub esp, 2048
            lea eax, [esp]
            fld1
            push eax
            and ebx, ~1
            push ebx
            fstp dword ptr [ebp + 0x18]
            lea ecx, [ebp + 0x18]
            push ecx
            lea edx, [ebp - 0x48]
            push edx
            lea eax, [ebp - 0x18]
            push eax
            lea ecx, [ebp - 0x24]
            push ecx
            call CameraIntersectHook
            add esp, 2048
            push kIntersectCallJumpBack
            ret
        }
    }

    void __declspec(naked) IterateCollisionListHook() {
        __asm {
            cmp dword ptr [CameraIndirectVisibilityEnabled], 0
            jne indirect_enabled

            mov esi, [edx + 4]
            je original_skip
            cmp byte ptr [esi + 0x25], bl
            push kIterateCollisionListJumpBack
            ret

        original_skip:
            push kIterateCollisionListJumpBack
            ret

        indirect_enabled:
            mov esi, [edx + 4]
            pushfd
            push eax
            push ecx
            push edx
            mov eax, [ebp + 0x0C]
            and eax, 0x8000000
            jz run_collision_processing
            mov eax, [esi + 0x34]
            test eax, eax
            jz skip_collision_processing
            push eax
            call CameraCollisionFilter
            add esp, 4
            test al, al
            je skip_collision_processing

        run_collision_processing:
            pop edx
            pop ecx
            pop eax
            popfd
            cmp byte ptr [esi + 0x25], bl
            push kIterateCollisionListJumpBack
            ret

        skip_collision_processing:
            pop edx
            pop ecx
            pop eax
            popfd
            push kIterateCollisionListSkip
            ret
        }
    }

    void __declspec(naked) IterateWorldObjCollisionListHook() {
        __asm {
            cmp dword ptr [CameraIndirectVisibilityEnabled], 0
            jne indirect_enabled

            cmp dword ptr [eax + 0x2D8], 0
            push kIterateWorldObjCollisionListJumpBack
            ret

        indirect_enabled:
            mov edx, [ebp + 0x0C]
            test edx, 0x8000000
            jz run_collision_processing
            push eax
            call CameraCollisionFilter
            add esp, 4
            test al, al
            je skip_collision_processing

        run_collision_processing:
            mov eax, [ebp - 0x68]
            cmp dword ptr [eax + 0x2D8], 0
            push kIterateWorldObjCollisionListJumpBack
            ret

        skip_collision_processing:
            push kIterateWorldObjCollisionListSkip
            ret
        }
    }

    bool InstallCameraIndirectHooks() {
        if (InterlockedCompareExchange(const_cast<LONG*>(&CameraIndirectHooksInstalled), 1, 0) != 0) {
            return true;
        }

        const BYTE intersectExpected[] = {
            0xD9, 0xE8, 0x6A, 0x00, 0x53, 0xD9, 0x5D, 0x18,
            0x8D, 0x4D, 0x18, 0x51, 0x8D, 0x55, 0xB8, 0x52,
            0x8D, 0x45, 0xE8, 0x50, 0x8D, 0x4D, 0xDC, 0x51,
            0xE8, 0x0D, 0x92, 0x17, 0x00
        };
        const BYTE iterateExpected[] = { 0x8B, 0x72, 0x04, 0x74, 0x09, 0x38, 0x5E, 0x25 };
        const BYTE iterateWorldExpected[] = { 0x83, 0xB8, 0xD8, 0x02, 0x00, 0x00, 0x00 };

        bool ok = true;
        ok = InstallJumpHook("Camera IntersectCall", kIntersectCallSite, intersectExpected, sizeof(intersectExpected),
            reinterpret_cast<void*>(&IntersectCallHook), &OriginalIntersectCall) && ok;
        ok = InstallJumpHook("Camera IterateCollisionList", kIterateCollisionListSite, iterateExpected, sizeof(iterateExpected),
            reinterpret_cast<void*>(&IterateCollisionListHook), &OriginalIterateCollisionList) && ok;
        ok = InstallJumpHook("Camera IterateWorldObjCollisionList", kIterateWorldObjCollisionListSite, iterateWorldExpected, sizeof(iterateWorldExpected),
            reinterpret_cast<void*>(&IterateWorldObjCollisionListHook), &OriginalIterateWorldObjCollisionList) && ok;

        if (!ok) {
            InterlockedExchange(const_cast<LONG*>(&CameraIndirectHooksInstalled), 0);
            InterlockedExchange(const_cast<LONG*>(&CameraIndirectVisibilityEnabled), 0);
            Log("CameraAPI indirect visibility hook install failed");
            return false;
        }

        Log("CameraAPI indirect visibility hooks installed");
        return true;
    }

    bool ReadFloatField(void* base, size_t offset, float& value) {
        auto* field = reinterpret_cast<float*>(static_cast<BYTE*>(base) + offset);
        if (!IsReadable(field, sizeof(float))) return false;
        value = *field;
        return true;
    }

    bool ReadDwordField(void* base, size_t offset, uint32_t& value) {
        auto* field = reinterpret_cast<uint32_t*>(static_cast<BYTE*>(base) + offset);
        if (!IsReadable(field, sizeof(uint32_t))) return false;
        value = *field;
        return true;
    }

    void LogCameraProbe(const char* reason, int seq) {
        guid_t playerGuid = GetPlayerGuid();
        CGObjectLite* player = ObjectMgrGet(playerGuid, TypeMaskPlayer);
        void* camera = GetActiveCamera();

        char line[256];
        sprintf_s(line,
            "CameraProbe reason=%s seq=%d playerGuid=0x%08X%08X player=0x%p camera=0x%p",
            reason ? reason : "unknown",
            seq,
            static_cast<unsigned>(playerGuid >> 32),
            static_cast<unsigned>(playerGuid & 0xFFFFFFFF),
            player,
            camera);
        Log(line);

        if (camera) {
            float fov = 0.0f;
            if (ReadFloatField(camera, kCameraFovOffset, fov)) {
                sprintf_s(line, "CameraProbe camera+0x%02X fovRadians=%.6f fovDegrees=%.3f",
                    static_cast<unsigned>(kCameraFovOffset),
                    fov,
                    fov * 200.0f / kPi);
                Log(line);
            }
        }

        if (!player) {
            Log("CameraProbe player object unavailable");
            return;
        }

        for (size_t offset = 0x80; offset <= 0xC0; offset += 0x10) {
            float f0 = 0.0f;
            float f4 = 0.0f;
            float f8 = 0.0f;
            float fC = 0.0f;
            uint32_t d0 = 0;
            uint32_t d4 = 0;
            uint32_t d8 = 0;
            uint32_t dC = 0;
            if (!ReadFloatField(player, offset, f0) ||
                !ReadFloatField(player, offset + 4, f4) ||
                !ReadFloatField(player, offset + 8, f8) ||
                !ReadFloatField(player, offset + 12, fC) ||
                !ReadDwordField(player, offset, d0) ||
                !ReadDwordField(player, offset + 4, d4) ||
                !ReadDwordField(player, offset + 8, d8) ||
                !ReadDwordField(player, offset + 12, dC)) {
                continue;
            }

            sprintf_s(line,
                "CameraProbe player+0x%03X floats=[%.6f %.6f %.6f %.6f] dwords=[%08X %08X %08X %08X]",
                static_cast<unsigned>(offset),
                f0,
                f4,
                f8,
                fC,
                d0,
                d4,
                d8,
                dC);
            Log(line);
        }
    }
}

bool RegisterCameraApiCVars(const char* reason) {
    if (InterlockedCompareExchange(const_cast<LONG*>(&CameraCVarsRegistered), 1, 0) != 0) {
        return true;
    }

    CVarCameraFov = RegisterCameraCVar("cameraFov", "100");
    CVarCameraIndirectVisibility = RegisterCameraCVar("cameraIndirectVisibility", "0");
    CVarCameraIndirectAlpha = RegisterCameraCVar("cameraIndirectAlpha", "0.6");
    CVarShowPlayer = RegisterCameraCVar("showPlayer", "1");
    CVarCameraProbeSeq = RegisterCameraCVar("grimfallCameraProbeSeq", "0");

    if (!CVarCameraFov) {
        InterlockedExchange(const_cast<LONG*>(&CameraCVarsRegistered), 0);
        Log("CameraAPI CVar registration failed: cameraFov unavailable");
        return false;
    }

    char line[160];
    sprintf_s(line, "CameraAPI CVar registration completed reason=%s", reason ? reason : "unknown");
    Log(line);
    return true;
}

void PollCameraApi() {
    if (!CameraCVarsRegistered && !RegisterCameraApiCVars("poll")) {
        return;
    }

    DWORD now = GetTickCount();
    if (LastPollTick != 0 && now - LastPollTick < kPollIntervalMs) {
        return;
    }
    LastPollTick = now;

    int fovMilli = ReadCVarFloatMilli(CVarCameraFov, 100000, 60000, 150000);
    if (fovMilli != LastFovMilli) {
        LastFovMilli = fovMilli;
        char line[96];
        sprintf_s(line, "CameraAPI fov=%d", fovMilli);
        Log(line);
    }
    ApplyCameraFov(fovMilli);

    int showPlayer = ReadCVarInt(CVarShowPlayer, 1, 0, 1);
    if (showPlayer != LastShowPlayer) {
        LastShowPlayer = showPlayer;
        char line[96];
        sprintf_s(line, "CameraAPI showPlayer=%d", showPlayer);
        Log(line);
    }
    ApplyShowPlayer(showPlayer);

    int probeSeq = ReadCVarInt(CVarCameraProbeSeq, 0, 0, 0x7FFFFFFF);
    if (probeSeq > 0 && probeSeq != LastProbeSeq) {
        LastProbeSeq = probeSeq;
        LogCameraProbe("probe-seq", probeSeq);
    }

    int indirectVisibility = ReadCVarInt(CVarCameraIndirectVisibility, 0, 0, 1);
    int indirectAlphaMilli = ReadCVarFloatMilli(CVarCameraIndirectAlpha, 600, 600, 1000);
    CameraIndirectAlpha = static_cast<float>(indirectAlphaMilli) / 1000.0f;
    if (indirectAlphaMilli != LastIndirectAlphaMilli) {
        LastIndirectAlphaMilli = indirectAlphaMilli;
        char line[96];
        sprintf_s(line, "CameraAPI indirectAlpha=%d", indirectAlphaMilli);
        Log(line);
    }

    if (indirectVisibility != LastIndirectVisibility) {
        LastIndirectVisibility = indirectVisibility;
        if (indirectVisibility) {
            if (InstallCameraIndirectHooks()) {
                InterlockedExchange(const_cast<LONG*>(&CameraIndirectVisibilityEnabled), 1);
                Log("CameraAPI indirectVisibility=1");
            }
        }
        else {
            InterlockedExchange(const_cast<LONG*>(&CameraIndirectVisibilityEnabled), 0);
            RestoreIndirectModels();
            Log("CameraAPI indirectVisibility=0");
        }
    }
}
}
