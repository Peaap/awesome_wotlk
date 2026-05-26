#include "Log.h"

#include <stdio.h>
#include <windows.h>

namespace AwesomeMacroLite {
namespace {
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
}
