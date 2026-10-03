cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(CMAKE_BUILD_TYPE BUILD)

add_executable(racing
    main.c

	race.h
	race.c
	
	track.h
	track.c
	
	cell.h
	cell.c
	
	car.h
	car.c
)
