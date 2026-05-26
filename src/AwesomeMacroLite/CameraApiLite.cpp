#include "CameraApiLite.h"

#include "GameClientLite.h"
#include "Log.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr uintptr_t kCVarGet = 0x00767460;
    constexpr uintptr_t kCVarRegister = 0x00767FC0;
    constexpr uintptr_t kGetActiveCamera = 0x004F5960;
    constexpr size_t kCVarValueOffset = 0x28;
    constexpr size_t kCameraFovOffset = 0x40;
    constexpr DWORD kPollIntervalMs = 250;
    constexpr float kPi = 3.14159265358979323846f;

    using CVarHandlerFn = int(__cdecl*)(void* cvar, const char* previousValue, const char* newValue, void* userData);
    using CVarGetFn = void* (__cdecl*)(const char* name);
    using CVarRegisterFn = void* (__cdecl*)(const char* name, const char* description, unsigned flags, const char* defaultValue,
        CVarHandlerFn callback, int a6, int a7, int a8, int a9);
    using GetActiveCameraFn = void* (__cdecl*)();

    auto CVarGet = reinterpret_cast<CVarGetFn>(kCVarGet);
    auto CVarRegister = reinterpret_cast<CVarRegisterFn>(kCVarRegister);
    auto GetActiveCamera = reinterpret_cast<GetActiveCameraFn>(kGetActiveCamera);

    void* CVarCameraFov = nullptr;
    void* CVarCameraIndirectVisibility = nullptr;
    void* CVarCameraIndirectAlpha = nullptr;
    void* CVarShowPlayer = nullptr;
    volatile LONG CameraCVarsRegistered = 0;
    DWORD LastPollTick = 0;
    int LastFovMilli = 0;
    int LastShowPlayer = -1;

    bool IsWritable(void* address, size_t size) {
        if (!address || !size) return false;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(address, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;

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
}

bool RegisterCameraApiCVars(const char* reason) {
    if (InterlockedCompareExchange(const_cast<LONG*>(&CameraCVarsRegistered), 1, 0) != 0) {
        return true;
    }

    CVarCameraFov = RegisterCameraCVar("cameraFov", "100");
    CVarCameraIndirectVisibility = RegisterCameraCVar("cameraIndirectVisibility", "0");
    CVarCameraIndirectAlpha = RegisterCameraCVar("cameraIndirectAlpha", "0.6");
    CVarShowPlayer = RegisterCameraCVar("showPlayer", "1");

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
        sprintf_s(line, "CameraAPI showPlayer=%d registered-only", showPlayer);
        Log(line);
    }
}
}
