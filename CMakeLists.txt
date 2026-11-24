cmake_minimum_required(VERSION 4.4.3)

project(race)
find_package(Threads REQUIRED)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_STANDARD 23)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../bin")

include(cmake/CompilerOptions.cmake)

file(GLOB_RECURSE CAR_SOURCE CONFIGURE_DEPENDS "src/car/*.c" "src/common/*.c")
add_executable(car ${CAR_SOURCE})
target_include_directories(car PRIVATE src/common)
target_link_libraries(car PRIVATE Threads::Threads)
configure_target(car)

file(GLOB_RECURSE RACE_SOURCE CONFIGURE_DEPENDS "src/race/*.c" "src/common/*.c")
add_executable(race ${RACE_SOURCE})
target_include_directories(race PRIVATE src/common)
target_link_libraries(race PRIVATE Threads::Threads)
configure_target(race)

file(GLOB_RECURSE VIEWER_SOURCE CONFIGURE_DEPENDS "src/viewer/*.c" "src/common/*.c")
add_executable(viewer ${VIEWER_SOURCE})
target_include_directories(viewer PRIVATE src/common)
target_link_libraries(viewer PRIVATE Threads::Threads)
configure_target(viewer)
