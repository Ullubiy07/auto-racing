cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(CMAKE_BUILD_TYPE BUILD)

add_executable(racing
	car.h
	track.h

	main.c
	track.c
)
