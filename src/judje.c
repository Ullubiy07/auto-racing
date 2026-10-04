#include <stdio.h>
#include <stdlib.h>

#include "judje.h"

void InitJudje(Judje* judje, Car* cars, int carCount, const JudjeRules* rules) {
	judje->rules = rules;
	judje->carCount = carCount;
	
	for (int i = 0; i < carCount; ++i) {
		judje->leaderBoard[i] = &cars[i];
	}
	
	judje->curCarIndex = 0;
	judje->roundsPlayed = 0;
}

static int CompareCars(const void* a, const void* b) {
	const Car* car1 = *(const Car**)a;
	const Car* car2 = *(const Car**)b;
	
	if (car1->isOut != car2->isOut) {
		return car1->isOut ? 1 : -1;
	}
	if (car1->laps != car2->laps) {
		return car2->laps - car1->laps;
	}
	if (car1->distToFinish != car2->distToFinish) {
		return car1->distToFinish - car2->distToFinish;
	}
	return car1->lane - car2->lane;
}

static void UpdateLeaderBoard(Judje* judje) {
	qsort(judje->leaderBoard, judje->carCount, sizeof(Car*), CompareCars);
}

void RegisterZeroMove(Judje* judje, Car* car) {
	car->isOut = true;
}

Car* GetCurrentCar(const Judje* judje) {
	return judje->leaderBoard[judje->curCarIndex];
}

bool NextTurn(Judje* judje) {
	judje->curCarIndex++;
	
	if (judje->curCarIndex >= judje->carCount ||
		GetCurrentCar(judje)->isOut
	) {
		judje->curCarIndex = 0;
		judje->roundsPlayed++;
		UpdateLeaderBoard(judje);
		return false;
	}
	return true;
}

void AnnounceWinner(const Judje* judje) {
	Car* winner = judje->leaderBoard[0];
	dprintf(2, "\n\x1b[%d;1mWinner: 🏎, (ID=%d)\x1b[0m\n",
			winner->team.color, winner->id);
}

void DrawLeaderBoard(const Judje* judje) {
	dprintf(2, "=============LEADER BOARD===============\n");
	dprintf(2, "%-5s |  %-3s  | %-3s  | %-5s | %-5s | %-5s  | %-5s\n", 
			"Place", "Car", "ID", "Laps", "Lane", "Dist", "State");
	
	for (int i = 0; i < judje->carCount; ++i) {
		Car* car = judje->leaderBoard[i];
		
		dprintf(2, "  %-3d |  \x1b[%d;1m%-6s\x1b[0m  | %-4d | %-5d | %-5d |  %-5d | %-5s", 
				i + 1, 
				car->team.color, "🏎",
				car->id,
				car->laps, 
		        car->lane,
				car->distToFinish,
				car->isOut ? "OUT" : "IN RACE");
		
		if (i == judje->curCarIndex) {
			dprintf(2, " <-");
		}
		dprintf(2, "\n");
	}
}

bool IsRaceOver(Judje* judje) {
	if (judje->roundsPlayed >= judje->rules->maxRounds) {
		return true;
	}
	
	bool active = false;
	for (int i = 0; i < judje->carCount; ++i) {
		if (judje->leaderBoard[i]->laps >= judje->rules->lapsTotal) {
			return true;
		}
		active |= !judje->leaderBoard[i]->isOut;
	}
	return !active;
}
