cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(CMAKE_BUILD_TYPE BUILD)

add_executable(racing
    main.c
    
    cli.h
    cli.c

	race.h
	race.c
	
	track.h
	track.c
	
	cell.h
	cell.c
	
	judge.h
	judge.c
	
	car.h
	car.c
)
