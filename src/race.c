#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "race.h"
#include "track.h"

static const int TEAM_COLORS[MAX_TEAMS] = {
	31,  // Красный
	32,  // Зеленый
	33,  // Желтый
	34,  // Синий
	35,  // Оранжевый
	36   // Фиолетовый
};

static int IsWall(CellType type) {
	return type == CELL_WALL_VERT || type == CELL_WALL_HOR ||
		   type == CELL_WALL_TOP_LEFT || type == CELL_WALL_TOP_RIGHT ||
		   type == CELL_WALL_BOT_LEFT || type == CELL_WALL_BOT_RIGHT;
}

static void InitCars(Race* r) {
	Team team;
	CellType type;
	Point pos = r->track->start.pos;
	int used[MAX_CARS] = {0};
	
	for (int i = 0; i < r->carCount; ++i) {
		type = r->track->map[pos.y][pos.x];
		if (IsWall(type)) {
			dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, r->carCount);
			exit(2);
		}
		
		char symbol = r->rules->startOrder[i];
		if (symbol > 'Z' || symbol < 'A' || symbol - 'A' >= r->carCount) {
			dprintf(2, "Wrong start order symbol: %c\n", symbol);
			exit(2);
		}
		
		int carIndex = symbol - 'A';
		if (carIndex >= r->carCount) {
			dprintf(2, "Start order symbol '%c' exceeds car count %d\n", symbol, r->carCount);
			exit(2);
		}
		
		if (used[carIndex]) {
			dprintf(2, "Start order symbol '%c' must be unique\n", 'A' + carIndex);
			exit(2);
		}
		used[carIndex] = 1;
		
		team.id = (symbol - 'A') / r->rules->carsInTeam;
		team.color = TEAM_COLORS[team.id % MAX_TEAMS];
	
		r->cars[i] = (Car) {
			.id = symbol,
			.lane = r->track->start.lane,
			.dir = r->track->start.dir,
			.pos = pos,
			.team = team
		};
		pos.x -= r->track->start.dir.x;
		pos.y -= r->track->start.dir.y;
	}
}

void CreateRace(Race* r, const Track* track, const Rules* rules) {
	assert(r);
	assert(track);
	assert(rules);
	
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
	
	InitCars(r);
}

static int IsCarAt(const Race* r, Point pos, const Car** res) {
	assert(res);

	for (int i = 0; i < r->carCount; ++i) {
		if (r->cars[i].pos.x == pos.x && r->cars[i].pos.y == pos.y) {
			*res = &r->cars[i];
			return 1;
		}
	}
	return 0;
}

void DrawRace(const Race* r) {
	assert(r);
	assert(r->track);
	
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
