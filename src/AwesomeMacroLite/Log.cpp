#include "Log.h"

#include <stdio.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
    WCHAR g_logDirectory[MAX_PATH] = {};
    WCHAR g_logPath[MAX_PATH] = {};

    bool StartsWith(const char* value, const char* prefix) {
        if (!value || !prefix) return false;
        while (*prefix) {
            if (*value++ != *prefix++) return false;
        }
        return true;
    }

    char LowerAscii(char value) {
        if (value >= 'A' && value <= 'Z') {
            return static_cast<char>(value + ('a' - 'A'));
        }
        return value;
    }

    bool ContainsNoCase(const char* value, const char* needle) {
        if (!value || !needle || !needle[0]) return false;

        for (const char* cursor = value; *cursor; ++cursor) {
            const char* left = cursor;
            const char* right = needle;
            while (*left && *right && LowerAscii(*left) == LowerAscii(*right)) {
                ++left;
                ++right;
            }
            if (!*right) {
                return true;
            }
        }
        return false;
    }

    bool ShouldWriteLog(const char* message) {
        if (!message || !message[0]) return false;

        if (StartsWith(message, "CameraProbe")) return true;
        if (StartsWith(message, "AddonBridge recv")) return true;
        if (StartsWith(message, "NamePlateAPI created")) return true;
        if (StartsWith(message, "NamePlateAPI status")) return true;
        if (StartsWith(message, "NamePlateAPI stacking calc")) return true;

        return ContainsNoCase(message, "fail") ||
            ContainsNoCase(message, "unexpected") ||
            ContainsNoCase(message, "invalid") ||
            ContainsNoCase(message, "timed out") ||
            ContainsNoCase(message, "unavailable") ||
            ContainsNoCase(message, "exception") ||
            ContainsNoCase(message, "refusing");
    }

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
}

void Log(const char* message) {
    if (!ShouldWriteLog(message)) {
        return;
    }

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
}
