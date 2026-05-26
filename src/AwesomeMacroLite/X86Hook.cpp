#include "X86Hook.h"

#include "Log.h"

#include <stdio.h>
#include <string.h>

namespace AwesomeMacroLite {
namespace {
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

    void LogUnexpectedBytes(const char* name, const BYTE* target, size_t patchSize) {
        char line[512] = {};
        int written = sprintf_s(line, "%s bytes unexpected:", name ? name : "hook target");
        for (size_t i = 0; i < patchSize && i < 16 && written > 0 && written < static_cast<int>(sizeof(line)); ++i) {
            written += sprintf_s(line + written, sizeof(line) - written, " %02X", target[i]);
        }
        Log(line);
    }
}

bool InstallJumpHook(
    const char* name,
    uintptr_t targetAddress,
    const BYTE* expectedBytes,
    size_t patchSize,
    void* hookFunction,
    void** originalFunction) {
    auto* target = reinterpret_cast<BYTE*>(targetAddress);
    if (!target || !expectedBytes || !hookFunction || !originalFunction || patchSize < 5) {
        Log("InstallJumpHook invalid arguments");
        return false;
    }

    if (memcmp(target, expectedBytes, patchSize) != 0) {
        LogUnexpectedBytes(name, target, patchSize);
        return false;
    }

    BYTE* trampoline = static_cast<BYTE*>(VirtualAlloc(
        nullptr,
        patchSize + 5,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE));
    if (!trampoline) {
        Log("VirtualAlloc trampoline failed");
        return false;
    }

    memcpy(trampoline, target, patchSize);
    if (!WriteJump(trampoline + patchSize, target + patchSize, 5)) {
        Log("failed to write trampoline jumpback");
        return false;
    }

    *originalFunction = trampoline;

    if (!WriteJump(target, hookFunction, patchSize)) {
        Log("failed to patch hook target");
        return false;
    }

    char line[256];
    sprintf_s(line, "%s manual hook installed target=0x%p trampoline=0x%p hook=0x%p",
        name ? name : "hook",
        target,
        trampoline,
        hookFunction);
    Log(line);
    return true;
}
}
