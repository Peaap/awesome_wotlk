#pragma once

#include <stdint.h>
#include <windows.h>

namespace AwesomeMacroLite {
    bool InstallJumpHook(
        const char* name,
        uintptr_t targetAddress,
        const BYTE* expectedBytes,
        size_t patchSize,
        void* hookFunction,
        void** originalFunction);
}
