#pragma once

struct PatchDetails {
    const char* name;
    unsigned virtualAddress;
    const char* originalHexBytes;
    const char* patchedHexBytes;
};

inline constexpr const char* kMacroLiteDllName = "AwesomeMacroLite.dll";

inline PatchDetails s_patches[] = {
    {
        "lua_ScanDllStart disable",
        0x004DCCF0,
        "558BEC568B75",
        "B8" "00000000"
        "C3"
    },
    {
        "ScanDllStart MacroLite loader",
        0x004E5CB0,
        "558BEC5633F639356CB4B6000F85DB010000393568B4B6000F85CF01000033C0B968B4B6008701566A5468F8659F006A18E85A8828006860659F00A380B4B600E81BBEF7FF",
        "B8" "01000000"
        "A3" "74B4B600"
        "68" "E05C4E00"
        "E8" "1C683800"
        "83C4" "04"
        "55"
        "8BEC"
        "E8" "A110F2FF"
        "E9" "045BF2FF"
        "CCCCCCCCCCCCCCCCCCCCCCCC"
        "417765736F6D654D6163726F4C6974652E646C6C00"
    },
    {
        "StartAddress loader jump",
        0x0040B7D0,
        "558BECE898B5FFFF",
        "E9" "DBA40D00"
        "909090"
    }
};
