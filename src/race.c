#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "race.h"
#include "car.h"
#include "track.h"

static Team InitTeam(char carSymbol, int carsInTeam) {
	static const int TEAM_COLORS[] = {
		31,  // Красный
		32,  // Зеленый
		33,  // Желтый
		34,  // Синий
		35,  // Оранжевый
		36   // Фиолетовый
	};
	Team team;
	team.id = (carSymbol - 'A') / carsInTeam;
	team.color = TEAM_COLORS[team.id % MAX_TEAMS];
	return team;
}

static void InitCars(Race* race) {
	int used[MAX_CARS] = {0};
	
	for (int i = 0; i < race->carCount; ++i) {
		char symbol = race->rules->startOrder[i];
		char index = symbol - 'A';

		if (index >= race->carCount || symbol < 'A') {
			dprintf(2, "Expected symbols between '%c' and '%c', got: '%c'\n", 
					'A', 'A' + race->carCount - 1, symbol);
			exit(2);
		}
		
		if (used[index]) {
			dprintf(2, "Dublicate symbol '%c' in start order\n", symbol);
			exit(2);
		}
		used[index] = 1;
		
		Team team = InitTeam(symbol, race->rules->carsInTeam);
		InitCar(&race->cars[i], symbol, team, &race->track->grid, i + 1);
		
		Point pos = race->cars[i].pos;
		if (!race->track->map[pos.y][pos.x].clear) {
			dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, race->carCount);
			exit(2);
		}
		race->track->map[pos.y][pos.x].clear = 0;
	}
}

static int IsCarAt(const Race* race, Point pos, const Car** res) {
	for (int i = 0; i < race->carCount; ++i) {
		if (race->cars[i].pos.x == pos.x && race->cars[i].pos.y == pos.y) {
			*res = &race->cars[i];
			return 1;
		}
	}
	return 0;
}

void InitRace(Race* race, Track* track, const Rules* rules, const Judje* judje) {
	race->track = track;
	race->rules = rules;
	race->judje = judje;
	race->carCount = rules->teamCount * rules->carsInTeam;
	if (race->carCount > MAX_CARS) {
		dprintf(2, "Cars limit exceeded, max: %d, got: %d\n", MAX_CARS, race->carCount);
		exit(2);
	}
	if (rules->teamCount > MAX_TEAMS) {
		dprintf(2, "Teams limit exceeded, max: %d, got: %d\n", MAX_TEAMS, rules->teamCount);
		exit(2);
	}
	
	int size = strlen(rules->startOrder);
	if (size != race->carCount) {
		dprintf(2, "Invalid start order size, need: %d, got: %d\n", race->carCount, size);
		exit(2);
	}
	
	InitCars(race);
}

void ClearScreen() {
	dprintf(1, "\x1b[2J");    // Очистить весь экран
	dprintf(1, "\x1b[0;0f");  // Переместить курсор в левый верхний угол
}

void StartRace(Race* race) {
	while (true) {
		for (int i = 0; i < race->carCount; ++i) {
			ClearScreen();
			DrawRace(race);
			MoveCar(&race->cars[i], race->track);
			usleep(1000 * 100);
		}
	}
}

void DrawRace(const Race* race) {
	const Car* car;
	
	for (int i = 0; i < race->track->height; ++i) {
		for (int j = 0; j < race->track->width; ++j) {
			if (IsCarAt(race, (Point){j, i}, &car)) {
				dprintf(1, "\x1b[%d;1m🏎 \x1b[0m", car->team.color);
			} else {
				CellType type = race->track->map[i][j].type;
				dprintf(1, "%s", GetCellInfo(type)->outSymbol);
			}
		}
		dprintf(1, "\n");
	}
}
