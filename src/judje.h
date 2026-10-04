#ifndef JUDJE_H
#define JUDJE_H

#include <stdbool.h>

#include "car.h"

enum {
	MAX_CARS = 26,
	MAX_TEAMS = 6,
};

typedef struct {
	int maxRounds;  // Предельное число раундов
	int lapsTotal;  // Всего кругов
} JudjeRules;

typedef struct {
	const JudjeRules* rules;
	int roundsPlayed;  // Количество сыгранных раундов
	
	Car* leaderBoard[MAX_CARS];
	int carCount;
	int curCarIndex;
} Judje;

void InitJudje(Judje* judje, Car* cars, int carCount, const JudjeRules* rules);

void DrawLeaderBoard(const Judje* judje);

bool IsRaceOver(Judje* judje);

Car* GetCurrentCar(const Judje* judje);
bool NextTurn(Judje* judje);
void RegisterZeroMove(Judje* judje, Car* car);
void AnnounceWinner(const Judje* judje);

#endif
