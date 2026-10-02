#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

static void InitCars(Race* r) {
	int used[MAX_CARS] = {0};
	
	for (int i = 0; i < r->carCount; ++i) {
		char symbol = r->rules->startOrder[i];
		char index = symbol - 'A';

		if (index >= r->carCount || symbol < 'A') {
			dprintf(2, "Expected symbols between '%c' and '%c', got: '%c'\n", 
					'A', 'A' + r->carCount - 1, symbol);
			exit(2);
		}
		
		if (used[index]) {
			dprintf(2, "Dublicate symbol '%c' in start order\n", symbol);
			exit(2);
		}
		used[index] = 1;
		
		Team team = InitTeam(symbol, r->rules->carsInTeam);
		InitCar(&r->cars[i], symbol, team, &r->track->grid, i + 1);
		
		Point pos = r->cars[i].pos;
		if (!IsCellDriveable(r->track->map[pos.y][pos.x])) {
			dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, r->carCount);
			exit(2);
		}
	}
}

static int IsCarAt(const Race* r, Point pos, const Car** res) {
	for (int i = 0; i < r->carCount; ++i) {
		if (r->cars[i].pos.x == pos.x && r->cars[i].pos.y == pos.y) {
			*res = &r->cars[i];
			return 1;
		}
	}
	return 0;
}

void InitRace(Race* r, const Track* track, const Rules* rules) {
	r->track = track;
	r->rules = rules;
	r->carCount = rules->teamCount * rules->carsInTeam;
	if (r->carCount > MAX_CARS) {
		dprintf(2, "Cars limit exceeded, max: %d, got: %d\n", MAX_CARS, r->carCount);
		exit(2);
	}
	if (rules->teamCount > MAX_TEAMS) {
		dprintf(2, "Teams limit exceeded, max: %d, got: %d\n", MAX_TEAMS, rules->teamCount);
		exit(2);
	}
	
	int size = strlen(rules->startOrder);
	if (size != r->carCount) {
		dprintf(2, "Invalid start order size, need: %d, got: %d\n", r->carCount, size);
		exit(2);
	}
	
	InitCars(r);
}

void DrawRace(const Race* r) {
	CellType type;
	const Car* car;
	
	for (int i = 0; i < r->track->height; ++i) {
		for (int j = 0; j < r->track->width; ++j) {
			if (IsCarAt(r, (Point){j, i}, &car)) {
				dprintf(1, "\x1b[%d;1m%c\x1b[0m", car->team.color, car->id);
			} else {
				type = r->track->map[i][j];
				dprintf(1, "%s", GetCellInfo(type)->outputSymbol);
			}
		}
		dprintf(1, "\n");
	}
}
