#include "AddonBridge.h"

#include "GameClientLite.h"
#include "Log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    constexpr uintptr_t kCVarGet = 0x00767460;
    constexpr uintptr_t kCVarRegister = 0x00767FC0;
    constexpr size_t kCVarValueOffset = 0x28;
    constexpr DWORD kPollIntervalMs = 100;
    constexpr int kMaxBridgeBytes = 255;
    constexpr uint32_t kTypeMaskObject = 0x1;
    constexpr uint32_t kTypeMaskItem = 0x2;
    constexpr uint32_t kTypeMaskContainer = 0x4;
    constexpr uint32_t kTypeMaskUnit = 0x8;
    constexpr uint32_t kTypeMaskGameObject = 0x20;
    constexpr uint32_t kTypeMaskDynamicObject = 0x40;
    constexpr uint32_t kTypeMaskCorpse = 0x80;
    constexpr uint32_t kTypeMaskAny = kTypeMaskObject | kTypeMaskItem | kTypeMaskContainer |
        kTypeMaskUnit | TypeMaskPlayer | kTypeMaskGameObject | kTypeMaskDynamicObject | kTypeMaskCorpse;

    using CVarHandlerFn = int(__cdecl*)(void* cvar, const char* previousValue, const char* newValue, void* userData);
    using CVarGetFn = void* (__cdecl*)(const char* name);
    using CVarRegisterFn = void* (__cdecl*)(const char* name, const char* description, unsigned flags, const char* defaultValue,
        CVarHandlerFn callback, int a6, int a7, int a8, int a9);

    auto CVarGet = reinterpret_cast<CVarGetFn>(kCVarGet);
    auto CVarRegister = reinterpret_cast<CVarRegisterFn>(kCVarRegister);

    void* CVarBridgeEnabled = nullptr;
    void* CVarBridgeTrace = nullptr;
    void* CVarBridgeMaxBytes = nullptr;
    void* CVarBridgeSeq = nullptr;
    void* CVarBridgeCommand = nullptr;
    void* CVarBridgePayload = nullptr;
    void* CVarBridgeAckSeq = nullptr;
    void* CVarBridgeAckStatus = nullptr;
    void* CVarBridgeAckPayload = nullptr;

    volatile LONG BridgeCVarsRegistered = 0;
    int LastProcessedSeq = 0;
    DWORD LastPollTick = 0;
    char AckSeqBuffer[32] = "0";
    char AckStatusBuffer[32] = "pending";
    char AckPayloadBuffer[256] = "";

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
        return *reinterpret_cast<const char**>(static_cast<BYTE*>(cvar) + kCVarValueOffset);
    }

    int ReadCVarInt(void* cvar, int fallback, int minimum, int maximum) {
        const char* value = ReadCVarString(cvar);
        if (!value) return fallback;
        return ClampInt(atoi(value), minimum, maximum);
    }

    void WriteCVarString(void* cvar, const char* value) {
        if (!cvar || !value) return;
        *reinterpret_cast<const char**>(static_cast<BYTE*>(cvar) + kCVarValueOffset) = value;
    }

    void* RegisterBridgeCVar(const char* name, const char* defaultValue) {
        if (void* existing = CVarGet(name)) {
            return existing;
        }

        void* cvar = CVarRegister(name, nullptr, 1, defaultValue, nullptr, 0, 0, 0, 0);
        char line[192];
        sprintf_s(line, "AddonBridge registered CVar name=%s ptr=0x%p default=%s", name, cvar, defaultValue);
        Log(line);
        return cvar;
    }

    bool Equals(const char* left, const char* right) {
        if (!left || !right) return false;
        return strcmp(left, right) == 0;
    }

    void FormatGuid(char* output, size_t outputSize, guid_t guid) {
        if (!guid) {
            sprintf_s(output, outputSize, "nil");
            return;
        }

        sprintf_s(output, outputSize, "%08X%08X",
            static_cast<unsigned>(guid >> 32),
            static_cast<unsigned>(guid & 0xFFFFFFFF));
    }

    bool TryParseGuid(const char* value, guid_t& guid) {
        guid = 0;
        if (!value || !value[0]) {
            return false;
        }

        size_t length = strlen(value);
        size_t offset = 0;
        if (length > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
            offset = 2;
        }
        if (length - offset == 0 || length - offset > 16) {
            return false;
        }

        guid_t parsed = 0;
        for (size_t i = offset; i < length; ++i) {
            char c = value[i];
            unsigned digit = 0;
            if (c >= '0' && c <= '9') {
                digit = static_cast<unsigned>(c - '0');
            }
            else if (c >= 'a' && c <= 'f') {
                digit = static_cast<unsigned>(c - 'a' + 10);
            }
            else if (c >= 'A' && c <= 'F') {
                digit = static_cast<unsigned>(c - 'A' + 10);
            }
            else {
                return false;
            }
            parsed = (parsed << 4) | digit;
        }

        guid = parsed;
        return guid != 0;
    }

    void SetAck(int seq, const char* status, const char* payload) {
        sprintf_s(AckSeqBuffer, "%d", seq);
        sprintf_s(AckStatusBuffer, "%s", status ? status : "ok");
        sprintf_s(AckPayloadBuffer, "%s", payload ? payload : "");
        WriteCVarString(CVarBridgeAckSeq, AckSeqBuffer);
        WriteCVarString(CVarBridgeAckStatus, AckStatusBuffer);
        WriteCVarString(CVarBridgeAckPayload, AckPayloadBuffer);
    }

    void ProcessCommand(int seq, const char* command, const char* payload, bool trace) {
        const char* safeCommand = command ? command : "";
        const char* safePayload = payload ? payload : "";

        if (trace) {
            char line[384];
            sprintf_s(line, "AddonBridge recv seq=%d command=%s payload=%s", seq, safeCommand, safePayload);
            Log(line);
        }

        if (Equals(safeCommand, "GML") || Equals(safeCommand, "GML/ping")) {
            SetAck(seq, "ok", "pong");
            return;
        }

        if (Equals(safeCommand, "GML/status")) {
            SetAck(seq, "ok", "core,nameplates,macro_conditionals,spell,addon_bridge,object_target");
            return;
        }

        if (Equals(safeCommand, "GML/config")) {
            SetAck(seq, "ok", "config-applied");
            return;
        }

        if (Equals(safeCommand, "GML/macro-test")) {
            SetAck(seq, "ok", "macro-conditionals-active");
            return;
        }

        if (Equals(safeCommand, "GML/object/player")) {
            char guid[32];
            FormatGuid(guid, sizeof(guid), GetPlayerGuid());
            SetAck(seq, "ok", guid);
            return;
        }

        if (Equals(safeCommand, "GML/object/target")) {
            char guid[32];
            FormatGuid(guid, sizeof(guid), GetGuidByKeyword("target"));
            SetAck(seq, "ok", guid);
            return;
        }

        if (Equals(safeCommand, "GML/object/mouseover")) {
            guid_t guidValue = 0;
            char guid[32];
            if (!GetMouseoverGuid(&guidValue)) {
                guidValue = 0;
            }
            FormatGuid(guid, sizeof(guid), guidValue);
            SetAck(seq, "ok", guid);
            return;
        }

        if (Equals(safeCommand, "GML/object/exists")) {
            guid_t guid = 0;
            if (!TryParseGuid(safePayload, guid)) {
                SetAck(seq, "rejected", "invalid-guid");
                return;
            }

            SetAck(seq, "ok", ObjectMgrGet(guid, kTypeMaskAny) ? "1" : "0");
            return;
        }

        SetAck(seq, "unknown_command", safeCommand);
    }
}

bool RegisterAddonBridgeCVars(const char* reason) {
    if (InterlockedCompareExchange(const_cast<LONG*>(&BridgeCVarsRegistered), 1, 0) != 0) {
        return true;
    }

    CVarBridgeEnabled = RegisterBridgeCVar("grimfallAddonBridge", "0");
    CVarBridgeTrace = RegisterBridgeCVar("grimfallAddonBridgeTrace", "0");
    CVarBridgeMaxBytes = RegisterBridgeCVar("grimfallAddonBridgeMaxBytes", "255");
    CVarBridgeSeq = RegisterBridgeCVar("grimfallAddonBridgeSeq", "0");
    CVarBridgeCommand = RegisterBridgeCVar("grimfallAddonBridgeCommand", "");
    CVarBridgePayload = RegisterBridgeCVar("grimfallAddonBridgePayload", "");
    CVarBridgeAckSeq = RegisterBridgeCVar("grimfallAddonBridgeAckSeq", "0");
    CVarBridgeAckStatus = RegisterBridgeCVar("grimfallAddonBridgeAckStatus", "pending");
    CVarBridgeAckPayload = RegisterBridgeCVar("grimfallAddonBridgeAckPayload", "");

    if (!CVarBridgeEnabled || !CVarBridgeSeq || !CVarBridgeCommand || !CVarBridgePayload ||
        !CVarBridgeAckSeq || !CVarBridgeAckStatus || !CVarBridgeAckPayload) {
        InterlockedExchange(const_cast<LONG*>(&BridgeCVarsRegistered), 0);
        Log("AddonBridge CVar registration failed: critical CVar unavailable");
        return false;
    }

    WriteCVarString(CVarBridgeAckSeq, AckSeqBuffer);
    WriteCVarString(CVarBridgeAckStatus, AckStatusBuffer);
    WriteCVarString(CVarBridgeAckPayload, AckPayloadBuffer);

    char line[160];
    sprintf_s(line, "AddonBridge CVar registration completed reason=%s", reason ? reason : "unknown");
    Log(line);
    return true;
}

void PollAddonBridge() {
    if (!BridgeCVarsRegistered && !RegisterAddonBridgeCVars("poll")) {
        return;
    }

    DWORD now = GetTickCount();
    if (LastPollTick != 0 && now - LastPollTick < kPollIntervalMs) {
        return;
    }
    LastPollTick = now;

    if (!StringToBool(ReadCVarString(CVarBridgeEnabled))) {
        return;
    }

    int seq = ReadCVarInt(CVarBridgeSeq, 0, 0, 0x7FFFFFFF);
    if (seq <= 0 || seq == LastProcessedSeq) {
        return;
    }

    LastProcessedSeq = seq;
    int maxBytes = ReadCVarInt(CVarBridgeMaxBytes, kMaxBridgeBytes, 1, kMaxBridgeBytes);
    const char* command = ReadCVarString(CVarBridgeCommand);
    const char* payload = ReadCVarString(CVarBridgePayload);
    if (!command || strlen(command) > kMaxBridgeBytes || (payload && static_cast<int>(strlen(payload)) > maxBytes)) {
        SetAck(seq, "rejected", "invalid-payload");
        return;
    }

    ProcessCommand(seq, command, payload, StringToBool(ReadCVarString(CVarBridgeTrace)));
}
}
