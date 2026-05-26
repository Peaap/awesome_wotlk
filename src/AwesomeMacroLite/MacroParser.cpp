#include "MacroParser.h"

#include "GameClientLite.h"
#include "Log.h"
#include "MacroTargetState.h"
#include "X86Hook.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr size_t kSecurePatchSize = 6;
    constexpr LONG kRewriteLogLimit = 25;

    SecureCmdOptionParseFn g_originalSecureCmdOptionParse = nullptr;

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

        int top = LuaGetTop(luaState);
        if (top < 3) {
            return;
        }
        if (LuaType(luaState, 2) != LuaStringType || LuaType(luaState, 3) != LuaStringType) {
            return;
        }

        size_t targetLength = 0;
        const char* target = LuaToLString(luaState, 3, &targetLength);
        if (!target) {
            return;
        }

        bool isCursor = EqualsLiteral(target, targetLength, "cursor");
        bool isPlayerLocation = EqualsLiteral(target, targetLength, "playerlocation");
        if (!isCursor && !isPlayerLocation) {
            return;
        }

        size_t originalLength = 0;
        size_t parsedLength = 0;
        const char* original = LuaToLString(luaState, 1, &originalLength);
        const char* parsed = LuaToLString(luaState, 2, &parsedLength);
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

        LuaSetTop(luaState, 0);
        LuaPushLString(luaState, original, originalLength);
        LuaPushLString(luaState, parsed, parsedLength);
        LuaPushNil(luaState);
        ArmMacroTarget(isCursor ? MacroTarget::Cursor : MacroTarget::PlayerLocation);
    }

    int __cdecl SecureCmdOptionParseHook(void* luaState) {
        static LONG calls = 0;
        LONG call = InterlockedIncrement(&calls);

        int result = g_originalSecureCmdOptionParse(luaState);
        RewriteSecureReturnIfNeeded(luaState, call);
        return result;
    }
}

bool InstallMacroParserHook() {
    BYTE expected[kSecurePatchSize] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10 };
    return InstallJumpHook(
        "SecureCmdOptionParse",
        Addresses::SecureCmdOptionParse,
        expected,
        kSecurePatchSize,
        reinterpret_cast<void*>(&SecureCmdOptionParseHook),
        reinterpret_cast<void**>(&g_originalSecureCmdOptionParse));
}
}
