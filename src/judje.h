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
	
	bool IsRaceOver;
	
	Car* moveOrder[MAX_CARS];
	int carCount;
} Judje;

void InitJudje(Judje* judje, Car* cars, int carCount, const JudjeRules* rules);

void StartRound(Judje* judje);

bool IsRaceOver(Judje* judje);

#endif
