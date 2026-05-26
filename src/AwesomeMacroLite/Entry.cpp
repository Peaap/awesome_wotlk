#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace {
    constexpr uintptr_t kSecureCmdOptionParse = 0x00564AE0;
    constexpr size_t kPatchSize = 6;
    constexpr LONG kRewriteLogLimit = 25;

    using SecureCmdOptionParseFn = int(__cdecl*)(void* luaState);
    using LuaGetTopFn = int(__cdecl*)(void* luaState);
    using LuaTypeFn = int(__cdecl*)(void* luaState, int index);
    using LuaToLStringFn = const char* (__cdecl*)(void* luaState, int index, size_t* length);
    using LuaSetTopFn = void(__cdecl*)(void* luaState, int index);
    using LuaPushLStringFn = void(__cdecl*)(void* luaState, const char* value, size_t length);
    using LuaPushNilFn = void(__cdecl*)(void* luaState);

    constexpr int kLuaString = 4;

    SecureCmdOptionParseFn g_originalSecureCmdOptionParse = nullptr;
    auto g_luaGetTop = reinterpret_cast<LuaGetTopFn>(0x0084DBD0);
    auto g_luaType = reinterpret_cast<LuaTypeFn>(0x0084DEB0);
    auto g_luaToLString = reinterpret_cast<LuaToLStringFn>(0x0084E0E0);
    auto g_luaSetTop = reinterpret_cast<LuaSetTopFn>(0x0084DBF0);
    auto g_luaPushLString = reinterpret_cast<LuaPushLStringFn>(0x0084E300);
    auto g_luaPushNil = reinterpret_cast<LuaPushNilFn>(0x0084E280);
    BYTE g_originalSecureBytes[kPatchSize] = {};
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

        if (EqualsLiteral(target, targetLength, "cursor") || EqualsLiteral(target, targetLength, "playerlocation")) {
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

    bool InstallSecureCmdOptionParseHook() {
        auto* target = reinterpret_cast<BYTE*>(kSecureCmdOptionParse);
        BYTE expected[kPatchSize] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10 };
        if (memcmp(target, expected, sizeof(expected)) != 0) {
            char line[256];
            sprintf_s(line, "SecureCmdOptionParse bytes unexpected: %02X %02X %02X %02X %02X %02X",
                target[0], target[1], target[2], target[3], target[4], target[5]);
            Log(line);
            return false;
        }

        memcpy(g_originalSecureBytes, target, kPatchSize);

        BYTE* trampoline = static_cast<BYTE*>(VirtualAlloc(
            nullptr,
            kPatchSize + 5,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
        if (!trampoline) {
            Log("VirtualAlloc trampoline failed");
            return false;
        }

        memcpy(trampoline, g_originalSecureBytes, kPatchSize);
        if (!WriteJump(trampoline + kPatchSize, target + kPatchSize, 5)) {
            Log("failed to write trampoline jumpback");
            return false;
        }

        g_originalSecureCmdOptionParse = reinterpret_cast<SecureCmdOptionParseFn>(trampoline);

        if (!WriteJump(target, reinterpret_cast<void*>(&SecureCmdOptionParseHook), kPatchSize)) {
            Log("failed to patch SecureCmdOptionParse");
            return false;
        }

        char line[256];
        sprintf_s(line, "SecureCmdOptionParse manual hook installed target=0x%p trampoline=0x%p hook=0x%p",
            target, trampoline, &SecureCmdOptionParseHook);
        Log(line);
        return true;
    }

    DWORD WINAPI InitThread(LPVOID) {
        Log("InitThread begin");
        Sleep(1000);
        Log("InitThread after sleep");
        InstallSecureCmdOptionParseHook();
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
