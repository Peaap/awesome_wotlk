project(AwesomeMacroLite)

add_library(${PROJECT_NAME} SHARED
    "Entry.cpp"
    "Log.cpp"
    "MacroParser.cpp"
    "MacroTargetState.cpp"
    "StanceFormFix.cpp"
    "TerrainTargeting.cpp"
    "X86Hook.cpp"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    kernel32
)
