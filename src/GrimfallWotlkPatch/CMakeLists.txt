cmake_minimum_required(VERSION 3.15)
project(GrimfallWotlkPatch)

add_executable(${PROJECT_NAME}
    Main.cpp
    Patch.h
    Utils.h
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_NAME="${PROJECT_NAME}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    comdlg32
)

if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
        "/SUBSYSTEM:WINDOWS"
        "/ENTRY:mainCRTStartup"
    )
endif()
