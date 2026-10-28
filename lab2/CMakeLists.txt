cmake_minimum_required(VERSION 3.16)

project(lab2_variant17 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(parent src/parent.c)
add_executable(child src/child.c)

target_compile_options(parent PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(child PRIVATE -Wall -Wextra -Wpedantic)