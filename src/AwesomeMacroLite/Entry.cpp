#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace {
    constexpr uintptr_t kSecureCmdOptionParse = 0x00564AE0;
    constexpr size_t kSecurePatchSize = 6;
    constexpr uintptr_t kOnLayerTrackTerrain = 0x004F66C0;
    constexpr size_t kTerrainPatchSize = 9;
    constexpr LONG kRewriteLogLimit = 25;
    constexpr DWORD kMacroTargetTimeoutMs = 1500;

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

    constexpr int kLuaString = 4;
    constexpr uint32_t kTypeMaskPlayer = 0x10;

    SecureCmdOptionParseFn g_originalSecureCmdOptionParse = nullptr;
    OnLayerTrackTerrainFn g_originalOnLayerTrackTerrain = nullptr;
    auto g_luaGetTop = reinterpret_cast<LuaGetTopFn>(0x0084DBD0);
    auto g_luaType = reinterpret_cast<LuaTypeFn>(0x0084DEB0);
    auto g_luaToLString = reinterpret_cast<LuaToLStringFn>(0x0084E0E0);
    auto g_luaSetTop = reinterpret_cast<LuaSetTopFn>(0x0084DBF0);
    auto g_luaPushLString = reinterpret_cast<LuaPushLStringFn>(0x0084E300);
    auto g_luaPushNil = reinterpret_cast<LuaPushNilFn>(0x0084E280);
    auto g_getPlayerGuid = reinterpret_cast<GetPlayerGuidFn>(0x004D3790);
    auto g_objectMgrGet = reinterpret_cast<ObjectMgrGetFn>(0x004D4DB0);
    auto g_handleTerrainClick = reinterpret_cast<HandleTerrainClickFn>(0x00527830);
    BYTE g_originalSecureBytes[kSecurePatchSize] = {};
    BYTE g_originalTerrainBytes[kTerrainPatchSize] = {};
    volatile LONG g_cursorKeywordActive = 0;
    volatile LONG g_playerLocationKeywordActive = 0;
    volatile LONG g_macroTargetArmedTick = 0;
    WCHAR g_logDirectory[MAX_PATH] = {};
    WCHAR g_logPath[MAX_PATH] = {};

    bool BuildPath(WCHAR* destination, size_t destinationCount, const WCHAR* base, const WCHAR* suffix) {
        int written = swprintf_s(destination, destinationCount, L"%s%s", base, suffix);
        return written > 0 && static_cast<size_t>(written) < destinationCount;
    }

    bool InitializeLogPath() {
        if (g_logPath[0] != L'\0') {
            return true;
        }

        WCHAR exePath[MAX_PATH] = {};
        DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) {
            return false;
        }

        for (DWORD i = length; i > 0; --i) {
            if (exePath[i - 1] == L'\\' || exePath[i - 1] == L'/') {
                exePath[i] = L'\0';
                break;
            }
        }

        WCHAR toolsPath[MAX_PATH] = {};
        WCHAR reportsPath[MAX_PATH] = {};
        if (!BuildPath(toolsPath, MAX_PATH, exePath, L"_mpq_tools") ||
            !BuildPath(reportsPath, MAX_PATH, exePath, L"_mpq_tools\\reports") ||
            !BuildPath(g_logDirectory, MAX_PATH, exePath, L"_mpq_tools\\reports\\macro_lite") ||
            !BuildPath(g_logPath, MAX_PATH, exePath, L"_mpq_tools\\reports\\macro_lite\\awesome_macro_lite.log")) {
            g_logPath[0] = L'\0';
            return false;
        }

        CreateDirectoryW(toolsPath, nullptr);
        CreateDirectoryW(reportsPath, nullptr);
        CreateDirectoryW(g_logDirectory, nullptr);
        return true;
    }

    void Log(const char* message) {
        if (!InitializeLogPath()) {
            return;
        }

        FILE* file = nullptr;
        _wfopen_s(&file, g_logPath, L"ab");
        if (!file) {
            return;
        }

        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u.%03u [tid=%lu] %s\r\n",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            GetCurrentThreadId(),
            message ? message : "");
        fclose(file);
    }

    bool EqualsLiteral(const char* value, size_t length, const char* literal) {
        size_t literalLength = strlen(literal);
        if (length != literalLength) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            char a = value[i];
            char b = literal[i];
            if (a >= 'A' && a <= 'Z') {
                a = static_cast<char>(a + ('a' - 'A'));
            }
            if (b >= 'A' && b <= 'Z') {
                b = static_cast<char>(b + ('a' - 'A'));
            }
            if (a != b) {
                return false;
            }
        }
        return true;
    }

    void TerrainClick(float x, float y, float z) {
        TerrainClickEvent event = {};
        event.guid = 0;
        event.pos = { x, y, z };
        event.button = 1;
        g_handleTerrainClick(&event);
    }

    bool GetObjectPosition(CGObjectLite* object, C3Vector& pos) {
        if (!object || !object->vtable || !object->vtable[11]) {
            return false;
        }

        auto getPosition = reinterpret_cast<ObjectGetPositionFn>(object->vtable[11]);
        getPosition(object, pos);
        return true;
    }

    void ClearMacroTargetFlags(const char* reason) {
        LONG hadCursor = InterlockedExchange(const_cast<LONG*>(&g_cursorKeywordActive), 0);
        LONG hadPlayerLocation = InterlockedExchange(const_cast<LONG*>(&g_playerLocationKeywordActive), 0);
        InterlockedExchange(const_cast<LONG*>(&g_macroTargetArmedTick), 0);

        if (hadCursor != 0 || hadPlayerLocation != 0) {
            char line[192];
            sprintf_s(line, "Macro target flags cleared reason=%s cursor=%ld playerlocation=%ld",
                reason ? reason : "unknown",
                hadCursor,
                hadPlayerLocation);
            Log(line);
        }
    }

    bool MacroTargetFlagsExpired() {
        LONG armedTick = InterlockedCompareExchange(const_cast<LONG*>(&g_macroTargetArmedTick), 0, 0);
        if (armedTick == 0) {
            return false;
        }

        DWORD elapsed = GetTickCount() - static_cast<DWORD>(armedTick);
        return elapsed > kMacroTargetTimeoutMs;
    }

    void RewriteSecureReturnIfNeeded(void* luaState, LONG call) {
        static LONG rewrites = 0;

        int top = g_luaGetTop(luaState);
        if (top < 3) {
            return;
        }
        if (g_luaType(luaState, 2) != kLuaString || g_luaType(luaState, 3) != kLuaString) {
            return;
        }

        size_t targetLength = 0;
        const char* target = g_luaToLString(luaState, 3, &targetLength);
        if (!target) {
            return;
        }

        bool isCursor = EqualsLiteral(target, targetLength, "cursor");
        bool isPlayerLocation = EqualsLiteral(target, targetLength, "playerlocation");
        if (isCursor || isPlayerLocation) {
            size_t originalLength = 0;
            size_t parsedLength = 0;
            const char* original = g_luaToLString(luaState, 1, &originalLength);
            const char* parsed = g_luaToLString(luaState, 2, &parsedLength);
            if (!original || !parsed) {
                return;
            }

            LONG rewrite = InterlockedIncrement(&rewrites);
            if (rewrite <= kRewriteLogLimit || (rewrite % 1000) == 0) {
                char line[224];
                sprintf_s(line, "SecureCmdOptionParseHook rewriting target rewrite=%ld call=%ld target=%.*s top=%d",
                    rewrite,
                    call,
                    static_cast<int>(targetLength),
                    target,
                    top);
                Log(line);
            }

            g_luaSetTop(luaState, 0);
            g_luaPushLString(luaState, original, originalLength);
            g_luaPushLString(luaState, parsed, parsedLength);
            g_luaPushNil(luaState);
            InterlockedExchange(const_cast<LONG*>(&g_cursorKeywordActive), isCursor ? 1 : 0);
            InterlockedExchange(const_cast<LONG*>(&g_playerLocationKeywordActive), isPlayerLocation ? 1 : 0);
            InterlockedExchange(const_cast<LONG*>(&g_macroTargetArmedTick), static_cast<LONG>(GetTickCount()));
        }
    }

    bool WriteJump(void* source, void* destination, size_t patchSize) {
        if (patchSize < 5) {
            return false;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(source, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            Log("VirtualProtect failed while writing jump");
            return false;
        }

        auto* bytes = static_cast<BYTE*>(source);
        bytes[0] = 0xE9;
        *reinterpret_cast<int32_t*>(bytes + 1) =
            static_cast<int32_t>(reinterpret_cast<BYTE*>(destination) - (bytes + 5));
        for (size_t i = 5; i < patchSize; ++i) {
            bytes[i] = 0x90;
        }

        FlushInstructionCache(GetCurrentProcess(), source, patchSize);
        VirtualProtect(source, patchSize, oldProtect, &oldProtect);
        return true;
    }

    int __cdecl SecureCmdOptionParseHook(void* luaState) {
        static LONG calls = 0;
        LONG call = InterlockedIncrement(&calls);

        int result = g_originalSecureCmdOptionParse(luaState);
        RewriteSecureReturnIfNeeded(luaState, call);
        return result;
    }

    int __fastcall OnLayerTrackTerrainHook(void* worldFrame, void*, int* terrainArgs) {
        static LONG calls = 0;
        LONG call = InterlockedIncrement(&calls);

        if (MacroTargetFlagsExpired()) {
            ClearMacroTargetFlags("timeout");
        }

        if (InterlockedCompareExchange(const_cast<LONG*>(&g_playerLocationKeywordActive), 0, 0) != 0) {
            CGObjectLite* player = g_objectMgrGet(g_getPlayerGuid(), kTypeMaskPlayer);
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
        else if (InterlockedCompareExchange(const_cast<LONG*>(&g_cursorKeywordActive), 0, 0) != 0) {
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

    bool InstallSecureCmdOptionParseHook() {
        auto* target = reinterpret_cast<BYTE*>(kSecureCmdOptionParse);
        BYTE expected[kSecurePatchSize] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10 };
        if (memcmp(target, expected, sizeof(expected)) != 0) {
            char line[256];
            sprintf_s(line, "SecureCmdOptionParse bytes unexpected: %02X %02X %02X %02X %02X %02X",
                target[0], target[1], target[2], target[3], target[4], target[5]);
            Log(line);
            return false;
        }

        memcpy(g_originalSecureBytes, target, kSecurePatchSize);

        BYTE* trampoline = static_cast<BYTE*>(VirtualAlloc(
            nullptr,
            kSecurePatchSize + 5,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
        if (!trampoline) {
            Log("VirtualAlloc trampoline failed");
            return false;
        }

        memcpy(trampoline, g_originalSecureBytes, kSecurePatchSize);
        if (!WriteJump(trampoline + kSecurePatchSize, target + kSecurePatchSize, 5)) {
            Log("failed to write trampoline jumpback");
            return false;
        }

        g_originalSecureCmdOptionParse = reinterpret_cast<SecureCmdOptionParseFn>(trampoline);

        if (!WriteJump(target, reinterpret_cast<void*>(&SecureCmdOptionParseHook), kSecurePatchSize)) {
            Log("failed to patch SecureCmdOptionParse");
            return false;
        }

        char line[256];
        sprintf_s(line, "SecureCmdOptionParse manual hook installed target=0x%p trampoline=0x%p hook=0x%p",
            target, trampoline, &SecureCmdOptionParseHook);
        Log(line);
        return true;
    }

    bool InstallOnLayerTrackTerrainHook() {
        auto* target = reinterpret_cast<BYTE*>(kOnLayerTrackTerrain);
        BYTE expected[kTerrainPatchSize] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xA4, 0x00, 0x00, 0x00 };
        if (memcmp(target, expected, sizeof(expected)) != 0) {
            char line[320];
            sprintf_s(line, "OnLayerTrackTerrain bytes unexpected: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                target[0], target[1], target[2], target[3], target[4],
                target[5], target[6], target[7], target[8]);
            Log(line);
            return false;
        }

        memcpy(g_originalTerrainBytes, target, kTerrainPatchSize);

        BYTE* trampoline = static_cast<BYTE*>(VirtualAlloc(
            nullptr,
            kTerrainPatchSize + 5,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
        if (!trampoline) {
            Log("VirtualAlloc terrain trampoline failed");
            return false;
        }

        memcpy(trampoline, g_originalTerrainBytes, kTerrainPatchSize);
        if (!WriteJump(trampoline + kTerrainPatchSize, target + kTerrainPatchSize, 5)) {
            Log("failed to write terrain trampoline jumpback");
            return false;
        }

        g_originalOnLayerTrackTerrain = reinterpret_cast<OnLayerTrackTerrainFn>(trampoline);

        if (!WriteJump(target, reinterpret_cast<void*>(&OnLayerTrackTerrainHook), kTerrainPatchSize)) {
            Log("failed to patch OnLayerTrackTerrain");
            return false;
        }

        char line[256];
        sprintf_s(line, "OnLayerTrackTerrain manual hook installed target=0x%p trampoline=0x%p hook=0x%p",
            target, trampoline, &OnLayerTrackTerrainHook);
        Log(line);
        return true;
    }

    DWORD WINAPI InitThread(LPVOID) {
        Log("InitThread begin");
        Sleep(1000);
        Log("InitThread after sleep");
        InstallSecureCmdOptionParseHook();
        InstallOnLayerTrackTerrainHook();
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        Log("DllMain DLL_PROCESS_ATTACH");
        HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
