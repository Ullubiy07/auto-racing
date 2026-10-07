#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "race.h"
#include "cell.h"

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
	Point pos = race->track->grid.pos;
	Cell* start = &race->track->map[pos.y][pos.x];
	
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
				
		
		start = start->road.back;
		if (!start || !IsCellFree(start)) {
			dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, race->carCount);
			exit(2);
		}
		
		InitCar(&race->cars[i], symbol, team, start);
		SetCellEntity(start, ENTITY_CAR, &race->cars[i]);
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
	InitJudge(&race->judge, race->cars, race->carCount, &race->settings->rules);
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
	DrawLeaderBoard(&race->judge);
	usleep(1000 * 50);
}

static void PlayRound(Race* race) {
	Judge* judje = &race->judge;
	
	StartRound(judje);
	DrawRace(race);
	
	MoveType moves[3 * MAX_CARS];
	int prevMoveCost = 0;
	
	while (!IsRoundOver(judje)) {
		Car* car = GetCurrentCar(judje);
		
		int maxBudget = (judje->turn == 0 ? 4 : judje->turn == 1 ? prevMoveCost + 2 : prevMoveCost + 1);
		int actualCost = 0;
		
		size_t movesDone = BuildRoute(car, moves, 3 * MAX_CARS, maxBudget);
		for (int i = 0; i < movesDone; ++i) {
			actualCost += GetMoveCost(car->cell, moves[i]);
			MakeMove(car, moves[i]);
			DrawRace(race);
		}
		
		prevMoveCost = actualCost;
		if (movesDone == 0) {
			RegisterZeroMove(judje);
		}
		
		NextTurn(judje);
	}
	
	EndRound(judje);
	DrawRace(race);
}

void StartRace(Race* race) {
	while (!IsRaceOver(&race->judge)) {
		PlayRound(race);
	}
	AnnounceWinner(&race->judge);
}
