cmake_minimum_required(VERSION 3.20)

add_executable(race
    main.c
    cli.h cli.c
    race.h race.c
	track.h track.c
	cell.h cell.c
	judge.h judge.c
	car.h car.c
)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_options(race PRIVATE -g -fno-omit-frame-pointer -Wall -Wextra -Werror)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "AsanDebug")
    message(STATUS "Configuring for AddressSanitizer...")
    target_compile_options(race PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
    target_link_options(race PRIVATE -fsanitize=address,undefined)

elseif (CMAKE_BUILD_TYPE STREQUAL "TsanDebug")
    message(STATUS "Configuring for ThreadSanitizer...")
    target_compile_options(race PRIVATE -fsanitize=thread)
    target_link_options(race PRIVATE -fsanitize=thread)
endif()
