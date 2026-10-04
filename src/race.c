#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "race.h"
#include "car.h"
#include "judje.h"

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
		char symbol = race->settings->startOrder[i];
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
		
		Team team = InitTeam(symbol, race->settings->carsInTeam);
		InitCar(&race->cars[i], symbol, team, race->track, i + 1);
		
		Point pos = race->cars[i].pos;
		Cell* cell = &race->track->map[pos.y][pos.x];
		if (!IsCellDriveable(cell->type)) {
			dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, race->carCount);
			exit(2);
		}
		SetCellEntity(cell, ENTITY_CAR, &race->cars[i]);
	}
}

void InitRace(Race* race, Track* track, const RaceSettings* settings) {
	race->track = track;
	race->settings = settings;
	race->carCount = settings->teamCount * settings->carsInTeam;
	if (race->carCount > MAX_CARS) {
		dprintf(2, "Cars limit exceeded, max: %d, got: %d\n", MAX_CARS, race->carCount);
		exit(2);
	}
	if (settings->teamCount > MAX_TEAMS) {
		dprintf(2, "Teams limit exceeded, max: %d, got: %d\n", MAX_TEAMS, settings->teamCount);
		exit(2);
	}
	
	int size = strlen(settings->startOrder);
	if (size != race->carCount) {
		dprintf(2, "Invalid start order size, need: %d, got: %d\n", race->carCount, size);
		exit(2);
	}
	
	InitCars(race);
	InitJudje(&race->judje, race->cars, race->carCount, &race->settings->rules);
}

void ClearScreen() {
	dprintf(1, "\x1b[2J");    // Очистить весь экран
	dprintf(1, "\x1b[0;0f");  // Переместить курсор в левый верхний угол
}

static void DrawRace(const Race* race) {
	ClearScreen();
	for (int i = 0; i < race->track->height; ++i) {
		for (int j = 0; j < race->track->width; ++j) {
			Cell cell = race->track->map[i][j];
			Car* car = cell.entity.data;
			if (cell.entity.type == ENTITY_CAR && !car->isOut) {
				dprintf(1, "\x1b[%d;1m🏎 \x1b[0m", car->team.color);
			} else {
				dprintf(1, "%s", GetCellInfo(cell.type)->outSymbol);
			}
		}
		dprintf(1, "\n");
	}
	DrawLeaderBoard(&race->judje);
	usleep(1000 * 50);
}

static void PlayRound(Race* race) {
	int prevMoves = 0;
	Judje* judje = &race->judje;

	int i = 0;
	do {
		Car* car = GetCurrentCar(judje);
		
		int movesLimit = (i == 0 ? 4 : i == 1 ? prevMoves + 2 : prevMoves + 1);
		int movesDone = 0;
		
		while (movesDone < movesLimit && MoveCar(car, race->track)) {
			++movesDone;
			DrawRace(race);
		}
		
		prevMoves = movesDone;
		if (!movesDone) {
			RegisterZeroMove(judje, car);
		}
		++i;
	} while (NextTurn(judje));
}

void StartRace(Race* race) {
	DrawRace(race);
	while (!IsRaceOver(&race->judje)) {
		PlayRound(race);
	}
	AnnounceWinner(&race->judje);
}
