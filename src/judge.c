#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "judge.h"

static int CompareCars(const void* a, const void* b) {
	const Car* carA = *(const Car**)a;
	const Car* carB = *(const Car**)b;
	
	Road* roadA = &carA->cell->road;
	Road* roadB = &carB->cell->road;
	
	if (carA->isOut != carB->isOut) {
		return carA->isOut - carB->isOut;
	}
	
	if ((!carA->hasPassedStart && carB->hasPassedStart)) {
		return 1;
	}
	if ((!carB->hasPassedStart && carA->hasPassedStart)) {
		return -1;
	}
	
	if (carA->laps != carB->laps) {
		return carB->laps - carA->laps;
	}
	if (roadA->distToFinish != roadB->distToFinish) {
		return roadA->distToFinish - roadB->distToFinish;
	}
	return roadA->lane - roadB->lane;
}

static void UpdateLeaderBoard(Judge* judje) {
	qsort(judje->leaderBoard, judje->carCount, sizeof(Car*), CompareCars);
}

void InitJudge(Judge* judje, Car* cars, int carCount, const JudgeRules* rules) {
	judje->rules = rules;
	judje->carCount = carCount;
	judje->activeCarCount = carCount;
	judje->carsInCurrentRound = carCount;
	
	for (int i = 0; i < carCount; ++i) {
		judje->leaderBoard[i] = &cars[i];
	}
	UpdateLeaderBoard(judje);
	
	judje->turn = 0;
	judje->roundsPlayed = 0;
}

Car* GetCurrentCar(const Judge* judje) {
	assert(judje->turn >= 0);
	assert(judje->turn < judje->carsInCurrentRound);
	assert(!judje->leaderBoard[judje->turn]->isOut);
	
	return judje->leaderBoard[judje->turn];
}

void NextTurn(Judge* judje) {
	++judje->turn;
}

void RegisterZeroMove(Judge* judje) {
	Car* car = GetCurrentCar(judje);
	
	assert(car);
	assert(!car->isOut);
	assert(judje->activeCarCount);
	
	car->isOut = true;
	--judje->activeCarCount;
}

void StartRound(Judge* judje) {
	judje->turn = 0;
	judje->carsInCurrentRound = judje->activeCarCount;
}

void EndRound(Judge* judje) {
	++judje->roundsPlayed;
	UpdateLeaderBoard(judje);
}

bool IsRoundOver(const Judge* judje) {
	return judje->turn >= judje->carsInCurrentRound;
}

bool IsRaceOver(const Judge* judje) {
	bool hasWinner = false;
	for (int i = 0; i < judje->carCount; ++i) {
		hasWinner |= (judje->leaderBoard[i]->laps >= judje->rules->lapsTotal);
	}
	
	return judje->activeCarCount == 0 ||
		   judje->roundsPlayed >= judje->rules->maxRounds ||
		   hasWinner;
}

void AnnounceWinner(const Judge* judje) {
	Car* winner = judje->leaderBoard[0];
	dprintf(2, "\n\x1b[%d;1mWinner: 🏎, (ID=%d)\x1b[0m\n",
			winner->team.color, winner->id);
}

void DrawLeaderBoard(const Judge* judje) {
	dprintf(2, "======================LEADER BOARD=====================\n");
	dprintf(2, "%-5s |  %-3s  | %-3s  | %-5s | %-5s | %-5s  | %-5s\n", 
			"Place", "Car", "ID", "Laps", "Lane", "Dist", "State");
	
	for (int i = 0; i < judje->carCount; ++i) {
		Car* car = judje->leaderBoard[i];
		
		dprintf(2, "  %-3d |  \x1b[%d;1m%-6s\x1b[0m  | %-4d | %-5d | %-5d |  %-5d | %-5s", 
				i + 1, 
				car->team.color, "🏎",
				car->id,
				car->laps, 
		        car->cell->road.lane,
				car->cell->road.distToFinish,
				car->isOut ? "OUT" : "IN RACE");
		
		if (i == judje->turn && !IsRoundOver(judje)) {
			dprintf(2, " <-");
		}
		dprintf(2, "\n");
	}
	dprintf(2, "\nRound played: %d\n", judje->roundsPlayed);
}
