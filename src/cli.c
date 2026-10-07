#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "cli.h"

enum {
	MAX_COLOR_SIZE = 10
};

typedef struct {
	char* name;
	int code;
} Color;

static int ParseColor(const char* color) {
	Color colors[24] = {
		{"RED", 31}, {"red", 31}, {"Red", 31}, {"31", 31}, 
		{"GREEN", 32}, {"green", 32}, {"Green", 32}, {"32", 32}, 
		{"YELLOW", 33}, {"yellow", 33}, {"Yellow", 33}, {"33", 33}, 
		{"BLUE", 34}, {"blue", 34}, {"Blue", 34}, {"34", 34}, 
		{"MAGENTA", 35}, {"magenta", 35}, {"Magenta", 35}, {"35", 35}, 
		{"CYAN", 36}, {"cyan", 36}, {"Cyan", 36}, {"36", 36}
	};
	
	for (int i = 0; i < 24; ++i) {
		if (strcmp(color, colors[i].name) == 0) {
			return colors[i].code;
		}
	}
	return 37;
}

static bool ParseTeam(RaceSettings* settings, const char* arg) {
	if (settings->teamCount >= MAX_TEAMS) {
		dprintf(2, "Teams limit exceeded, max: %d\n", MAX_TEAMS);
		return false;
	}
	
	char buf[MAX_TEAM_NAME_SIZE + MAX_CARS_IN_TEAM * (MAX_TEAM_NAME_SIZE + 1) + MAX_COLOR_SIZE + 2];
	snprintf(buf, sizeof(buf), "%s", arg);
	
	char* teamName = strtok(buf, ":");
	if (!teamName) {
		return false;
	}
	
	size_t size = strlen(teamName);
	if (size >= MAX_TEAM_NAME_SIZE) {
		dprintf(2, "Team name is too long, max: %d, got: %zu\n", MAX_TEAM_NAME_SIZE - 1, size);
		return false;
	}
	
	TeamConfig* team = &settings->teams[settings->teamCount];
	snprintf(team->name, sizeof(team->name), "%s", teamName);
	
	char* color = strtok(NULL, ":");
	if (!color) {
		return false;
	}
	team->color = ParseColor(color);
	
	char* drivers = strtok(NULL, ":");
	if (!drivers) {
		return false;
	}
	team->driverCount = 0;
	
	for (char* driver = strtok(drivers, ","); driver; driver = strtok(NULL, ",")) {
		
		if (team->driverCount >= MAX_CARS_IN_TEAM) {
			dprintf(2, "Exceeded max drivers per team limit: %d\n", MAX_CARS_IN_TEAM);
			return false;
		}
		
		size_t size = strlen(driver);
		if (size >= MAX_DRIVER_NAME_SIZE) {
			dprintf(2, "Driver name is too long, max: %d, got: %zu\n", MAX_DRIVER_NAME_SIZE - 1, size);
			return false;
		}
		
		snprintf(team->drivers[team->driverCount], MAX_DRIVER_NAME_SIZE, "%s", driver);
		++team->driverCount;
		++settings->carCount;
	}

	++settings->teamCount;
	return true;
}

static bool ParseStartOrder(RaceSettings* settings, const char* arg) {
	char buf[MAX_CARS * (MAX_DRIVER_NAME_SIZE + 1)];
	int count = snprintf(buf, sizeof(buf), "%s", arg);
	
	settings->startOrderSize = 0;
	
	for (char* driver = strtok(buf, ","); driver; driver = strtok(NULL, ",")) {
		
		if (settings->startOrderSize >= MAX_CARS) {
			dprintf(2, "Start order size exceeds max number of cars: %d\n", MAX_CARS);
			return false;
		}
		
		size_t size = strlen(driver);
		if (size >= MAX_DRIVER_NAME_SIZE) {
			dprintf(2, "Driver name is too long, max: %d, got: %zu\n", MAX_DRIVER_NAME_SIZE - 1, size);
			return false;
		}
		
		snprintf(settings->startOrder[settings->startOrderSize], MAX_DRIVER_NAME_SIZE, "%s", driver);
		++settings->startOrderSize;
	}
	return true;
}

static void PrintHelpUsage(const char *progName) {
	printf("Usage: %s [OPTION]...\n", progName);
	printf("Options:\n");
	printf("  -t, --team \"NAME:COLOR:DRIVER1,DRIVER2,...\"  Add a team with drivers\n");
	printf("  -s, --start \"DRIVER1,DRIVER2,...\"            Specify start order by driver names\n");
	printf("  -r, --rounds N                                 Set max rounds\n");
	printf("  -l, --laps N                                   Set max laps\n");
	printf("  -h, --help                                     Print help message\n");
}

bool ParseCLIArgs(RaceSettings* settings, int argc, char* argv[]) {
	static struct option longOptions[] = {
		{"team",   required_argument, NULL, 't'},
		{"start",  required_argument, NULL, 's'},
		{"rounds", required_argument, NULL, 'r'},
		{"laps",   required_argument, NULL, 'l'},
		{"help",   no_argument,       NULL, 'h'},
		{NULL,     0,                 NULL, 0}
	};
	int opt;
	
	while ((opt = getopt_long(argc, argv, "t:s:r:l:h", longOptions, NULL)) != -1) {
		switch (opt) {
			case 't':
				if (!ParseTeam(settings, optarg)) {
					dprintf(2, "Invalid team format: %s\n", optarg);
					return false;
				}
				break;
				
			case 's':
				if (!ParseStartOrder(settings, optarg)) {
					dprintf(2, "Invalid start order format: %s\n", optarg);
					return false;
				}
				break;
				
			case 'r':
				settings->rules.maxRounds = atoi(optarg);
				break;
				
			case 'l':
				settings->rules.maxLaps = atoi(optarg);
				break;
				
			case 'h':
			case '?':
				PrintHelpUsage(argv[0]);
				return false;
				
			default:
				return false;
		}
	}
	
	if (settings->startOrderSize != settings->carCount) {
		dprintf(2, "Invalid start order size, need: %d, got: %d\n", settings->carCount, settings->startOrderSize);
		return false;
	}
	
	return true;
}
