#include "Patch.h"
#include "Utils.h"

#include <Windows.h>
#include <CommDlg.h>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {
    std::string s_appName;
    bool s_quietMode = false;

    std::string findGameClientExecutable(const char* appPath) {
        static const char* possibleNames[] = { "Wow.exe", "Project-Epoch.exe" };
        std::vector<std::filesystem::path> searchRoots = {
            std::filesystem::absolute(appPath).parent_path(),
            std::filesystem::current_path()
        };

        for (const auto& root : searchRoots) {
            for (const char* name : possibleNames) {
                std::filesystem::path candidate = root / name;
                if (std::filesystem::is_regular_file(candidate)) {
                    return candidate.string();
                }
            }
        }

        for (const char* name : possibleNames) {
            if (std::filesystem::is_regular_file(name)) {
                return name;
            }
        }
        return {};
    }

    std::string browseForGameClientExecutable(const char* appPath) {
        char selectedPath[MAX_PATH] = {};

        std::filesystem::path initialDir = std::filesystem::absolute(appPath).parent_path();
        std::string initialDirString = initialDir.string();

        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFilter =
            "World of Warcraft executable (Wow.exe)\0Wow.exe\0"
            "Executable files (*.exe)\0*.exe\0"
            "All files (*.*)\0*.*\0";
        ofn.lpstrFile = selectedPath;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = initialDirString.c_str();
        ofn.lpstrTitle = "Select Grimfall Wow.exe";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameA(&ofn)) {
            return {};
        }
        return selectedPath;
    }

    void message(DWORD icon, const std::string& text) {
        if (!s_quietMode) {
            MessageBoxA(nullptr, text.c_str(), s_appName.c_str(), icon | MB_OK);
        }
    }

    bool patchMatches(const std::vector<char>& image, const PatchDetails& patch, bool patched) {
        std::vector<char> bytes;
        if (!convHexString2ByteArray(patched ? patch.patchedHexBytes : patch.originalHexBytes, bytes)) {
            SetLastError(ERROR_BAD_ARGUMENTS);
            return false;
        }

        unsigned offset = virtualAddress2RawOffset(const_cast<char*>(image.data()), patch.virtualAddress);
        return bytesEqual(image, offset, bytes);
    }

    bool writePatchBytes(std::vector<char>& image, const PatchDetails& patch, bool applyPatch, std::string& error) {
        std::vector<char> expected;
        std::vector<char> replacement;
        if (!convHexString2ByteArray(applyPatch ? patch.originalHexBytes : patch.patchedHexBytes, expected) ||
            !convHexString2ByteArray(applyPatch ? patch.patchedHexBytes : patch.originalHexBytes, replacement) ||
            expected.empty() ||
            expected.size() != replacement.size()) {
            SetLastError(ERROR_BAD_ARGUMENTS);
            error = std::string("invalid patch definition: ") + patch.name;
            return false;
        }

        unsigned offset = virtualAddress2RawOffset(image.data(), patch.virtualAddress);
        if (!offset || offset + expected.size() > image.size()) {
            SetLastError(ERROR_INVALID_ADDRESS);
            error = std::string("patch address not found: ") + patch.name;
            return false;
        }

        if (bytesEqual(image, offset, replacement)) {
            return true;
        }

        if (!bytesEqual(image, offset, expected)) {
            SetLastError(ERROR_INVALID_DATA);
            error = std::string("unexpected bytes at patch site: ") + patch.name;
            return false;
        }

        memcpy(&image[offset], replacement.data(), replacement.size());
        return true;
    }

    enum class PatchStatus {
        Original,
        Patched,
        MixedOrUnknown
    };

    PatchStatus getStatus(const std::vector<char>& image) {
        bool allOriginal = true;
        bool allPatched = true;

        for (const auto& patch : s_patches) {
            allOriginal = allOriginal && patchMatches(image, patch, false);
            allPatched = allPatched && patchMatches(image, patch, true);
        }

        if (allPatched) {
            return PatchStatus::Patched;
        }
        if (allOriginal) {
            return PatchStatus::Original;
        }
        return PatchStatus::MixedOrUnknown;
    }

    bool createBackup(const std::filesystem::path& exePath, std::string& error) {
        std::filesystem::path backupPath = exePath;
        backupPath += ".grimfall-awesome.bak";
        if (std::filesystem::is_regular_file(backupPath)) {
            return true;
        }

        std::error_code ec;
        std::filesystem::copy_file(exePath, backupPath, std::filesystem::copy_options::none, ec);
        if (ec) {
            error = "failed to create backup: " + backupPath.string() + "\n" + ec.message();
            return false;
        }
        return true;
    }

    bool applyPatches(const char* path, bool unpatch, std::string& error) {
        std::vector<char> image;
        if (!readFile(path, image)) {
            error = "failed to read executable";
            return false;
        }

        if (!unpatch && !createBackup(path, error)) {
            return false;
        }

        if (!unpatch && !applyLAA(image)) {
            SetLastError(ERROR_INVALID_DATA);
            error = "failed to set large-address-aware flag";
            return false;
        }

        for (const auto& patch : s_patches) {
            if (!writePatchBytes(image, patch, !unpatch, error)) {
                return false;
            }
        }

        if (!writeFile(path, image)) {
            error = "failed to write executable";
            return false;
        }
        return true;
    }

    bool copyMacroLiteNextToWow(const char* exePath, const char* appPath) {
        std::filesystem::path libInGamePath = std::filesystem::path(exePath).parent_path() / kMacroLiteDllName;
        std::filesystem::path libInAppPath = std::filesystem::absolute(appPath).parent_path() / kMacroLiteDllName;
        if (std::filesystem::is_regular_file(libInGamePath)) {
            return true;
        }
        if (!std::filesystem::is_regular_file(libInAppPath)) {
            return false;
        }

        std::error_code ec;
        std::filesystem::copy_file(libInAppPath, libInGamePath, std::filesystem::copy_options::overwrite_existing, ec);
        return !ec;
    }
}

int main(int argc, char** argv) {
    s_appName = std::filesystem::path(argv[0]).filename().string();

    bool statusOnly = false;
    bool unpatch = false;
    bool browse = false;
    std::string exePath;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            s_quietMode = true;
        }
        else if (strcmp(argv[i], "--status") == 0) {
            statusOnly = true;
        }
        else if (strcmp(argv[i], "--unpatch") == 0) {
            unpatch = true;
        }
        else if (strcmp(argv[i], "--browse") == 0) {
            browse = true;
        }
        else if (exePath.empty()) {
            exePath = argv[i];
        }
    }

    if (exePath.empty() && !browse) {
        exePath = findGameClientExecutable(argv[0]);
    }
    if (exePath.empty() && !s_quietMode) {
        exePath = browseForGameClientExecutable(argv[0]);
    }
    if (exePath.empty()) {
        message(MB_ICONERROR,
            "World of Warcraft executable not found.\n\n"
            "Move this patcher next to Wow.exe and run it again, or select Wow.exe in the file picker.\n\n"
            "Advanced use:\n"
            + s_appName + " C:\\Path\\To\\Grimfall-WoW\\Wow.exe");
        return 1;
    }

    std::vector<char> image;
    if (!readFile(exePath.c_str(), image)) {
        message(MB_ICONERROR, "Failed to read " + exePath);
        return 1;
    }

    PatchStatus status = getStatus(image);
    if (statusOnly) {
        const char* statusText =
            status == PatchStatus::Patched ? "patched" :
            status == PatchStatus::Original ? "unpatched" :
            "mixed or unknown";
        message(MB_ICONINFORMATION, exePath + "\n\nStatus: " + statusText);
        return status == PatchStatus::MixedOrUnknown ? 2 : 0;
    }

    if (status == PatchStatus::MixedOrUnknown) {
        message(MB_ICONERROR,
            exePath +
            "\n\nUnexpected patch bytes found. Refusing to modify this executable.");
        return 1;
    }

    std::string error;
    if (!applyPatches(exePath.c_str(), unpatch, error)) {
        DWORD lastError = GetLastError();
        message(MB_ICONERROR,
            "Failed to " + std::string(unpatch ? "remove" : "apply") +
            " Grimfall MacroLite patch from " + exePath +
            "\n\n" + error +
            "\n\nWindows error " + std::to_string(lastError) + ": " + windowsErrorMessage(lastError));
        return 1;
    }

    if (unpatch) {
        message(MB_ICONINFORMATION, "Grimfall MacroLite patch removed from:\n" + exePath);
        return 0;
    }

    bool copied = copyMacroLiteNextToWow(exePath.c_str(), argv[0]);
    if (copied) {
        message(MB_ICONINFORMATION,
            "Grimfall MacroLite patch applied to:\n" + exePath +
            "\n\nYou can now run Wow.exe directly.");
    }
    else {
        message(MB_ICONWARNING,
            "Grimfall MacroLite patch applied to:\n" + exePath +
            "\n\nPlace " + kMacroLiteDllName + " next to Wow.exe before launching.");
    }
    return 0;
}
