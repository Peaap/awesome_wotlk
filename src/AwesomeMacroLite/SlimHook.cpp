#include "SlimHook.h"

#include "Log.h"

#include <SlimDetours.h>

#include <stdio.h>
#include <string.h>

namespace AwesomeMacroLite {
namespace {
    void LogUnexpectedBytes(const char* name, const BYTE* target, size_t expectedSize) {
        char line[512] = {};
        int written = sprintf_s(line, "%s bytes unexpected:", name ? name : "hook target");
        for (size_t i = 0; i < expectedSize && i < 16 && written > 0 && written < static_cast<int>(sizeof(line)); ++i) {
            written += sprintf_s(line + written, sizeof(line) - written, " %02X", target[i]);
        }
        Log(line);
    }
}

bool InstallSlimDetoursHook(
    const char* name,
    uintptr_t targetAddress,
    const BYTE* expectedBytes,
    size_t expectedSize,
    void* hookFunction,
    void** originalFunction) {
    auto* target = reinterpret_cast<BYTE*>(targetAddress);
    if (!target || !expectedBytes || expectedSize == 0 || !hookFunction || !originalFunction) {
        Log("InstallSlimDetoursHook invalid arguments");
        return false;
    }

    if (memcmp(target, expectedBytes, expectedSize) != 0) {
        LogUnexpectedBytes(name, target, expectedSize);
        return false;
    }

    *originalFunction = target;
    HRESULT hr = SlimDetoursInlineHook(TRUE, reinterpret_cast<PVOID*>(originalFunction), hookFunction);
    if (FAILED(hr)) {
        char line[256];
        sprintf_s(line, "%s SlimDetours hook failed hr=0x%08lX target=0x%p hook=0x%p",
            name ? name : "hook",
            static_cast<unsigned long>(hr),
            target,
            hookFunction);
        Log(line);
        return false;
    }

    char line[256];
    sprintf_s(line, "%s SlimDetours hook installed target=0x%p trampoline=0x%p hook=0x%p",
        name ? name : "hook",
        target,
        *originalFunction,
        hookFunction);
    Log(line);
    return true;
}
}
