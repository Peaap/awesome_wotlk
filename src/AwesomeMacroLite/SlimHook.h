#pragma once

#include <stdint.h>
#include <windows.h>

namespace AwesomeMacroLite {
    bool InstallSlimDetoursHook(
        const char* name,
        uintptr_t targetAddress,
        const BYTE* expectedBytes,
        size_t expectedSize,
        void* hookFunction,
        void** originalFunction);
}
