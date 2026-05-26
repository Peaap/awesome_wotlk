#pragma once

#include <Windows.h>
#include <charconv>
#include <fstream>
#include <string>
#include <vector>

inline bool readFile(const char* path, std::vector<char>& content) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    content.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

inline bool writeFile(const char* path, const std::vector<char>& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    file.write(content.data(), content.size());
    return true;
}

inline unsigned virtualAddress2RawOffset(char* image, unsigned address) {
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(&image[dosHeader->e_lfanew]);
    auto* header = IMAGE_FIRST_SECTION(ntHeaders);
    for (size_t i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        DWORD begin = ntHeaders->OptionalHeader.ImageBase + header->VirtualAddress;
        DWORD end = begin + header->Misc.VirtualSize;
        if (address >= begin && address < end) {
            return address - begin + header->PointerToRawData;
        }
        header++;
    }
    return 0;
}

inline bool convHexString2ByteArray(const char* hex, std::vector<char>& result) {
    size_t hexLen = strlen(hex);
    result.clear();
    result.reserve(hexLen / 2);
    for (size_t i = 0; (i + 1) < hexLen; i += 2) {
        int value = 0;
        if (std::from_chars(&hex[i], &hex[i + 2], value, 0x10).ec == std::errc{}) {
            result.push_back(static_cast<char>(value & 0xFF));
        }
    }
    return result.capacity() == result.size();
}

inline bool applyLAA(std::vector<char>& image) {
    if (image.size() < sizeof(IMAGE_DOS_HEADER)) {
        return false;
    }

    auto* dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(image.data());
    if (dosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS) > image.size()) {
        return false;
    }

    auto* ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(&image[dosHeader->e_lfanew]);
    ntHeaders->FileHeader.Characteristics |= IMAGE_FILE_LARGE_ADDRESS_AWARE;
    return true;
}

inline bool bytesEqual(const std::vector<char>& image, unsigned offset, const std::vector<char>& bytes) {
    if (offset == 0 || offset + bytes.size() > image.size()) {
        return false;
    }
    return memcmp(&image[offset], bytes.data(), bytes.size()) == 0;
}

inline std::string windowsErrorMessage(DWORD error) {
    char* raw = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&raw),
        0,
        nullptr);

    std::string message = raw ? raw : "unknown error";
    if (raw) {
        LocalFree(raw);
    }
    return message;
}
