cmake_minimum_required(VERSION 4.4.3)

project(race)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_STANDARD 23)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../bin")

add_subdirectory(src)
