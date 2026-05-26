project(KNSoft.SlimDetours C)

add_library(${PROJECT_NAME} STATIC
    "Disassembler.c"
    "InlineHook.c"
    "Instruction.c"
    "Memory.c"
    "Thread.c"
    "Trampoline.c"
    "Transaction.c"
    "SlimDetours.h"
    "SlimDetours.inl"
    "SlimDetours.NDK.inl"
)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/deps/KNSoft.NDK/Include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    _USE_KNSOFT_NDK
    _X86_
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    ntdll
)

if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /wd4005 /wd4201 /FIstdlib.h /FIlimits.h)
endif()
