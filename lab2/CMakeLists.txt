cmake_minimum_required(VERSION 3.16)
project(os_lab2_variant6 C)

if(NOT WIN32)
    message(FATAL_ERROR "This project targets Windows (Win32 API).")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(parent parent.c)
add_executable(child child.c)

if(MSVC)
    target_compile_options(parent PRIVATE /W4)
    target_compile_options(child PRIVATE /W4)
else()
    target_compile_options(parent PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(child PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Put both executables in the same directory on single- and multi-config generators.
foreach(target parent child)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}")
endforeach()
