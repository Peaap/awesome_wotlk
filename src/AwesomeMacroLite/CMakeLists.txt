project(AwesomeMacroLite)

add_library(${PROJECT_NAME} SHARED
    "AddonBridge.cpp"
    "AddonBridge.h"
    "Entry.cpp"
    "Log.cpp"
    "MacroParser.cpp"
    "MacroTargetState.cpp"
    "NameplateApi.cpp"
    "NameplateApi.h"
    "SlimHook.cpp"
    "SpellStanceFix.cpp"
    "TerrainTargeting.cpp"
    "UnitApiLite.cpp"
    "UnitApiLite.h"
    "X86Hook.cpp"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    KNSoft.SlimDetours
    kernel32
)
