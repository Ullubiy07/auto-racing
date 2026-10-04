#ifndef JUDJE_H
#define JUDJE_H

#include <stdbool.h>

#include "car.h"

enum {
	MAX_CARS = 26,
	MAX_TEAMS = 6,
};

typedef struct {
	int roundsPlayed;  // Количество сыгранных раундов
	int maxRounds;     // Предельное число раундов
	
	Car* moveOrder[MAX_CARS];
	int carCount;
} Judje;

void InitJudje(Judje* judje, Car* cars, int carCount,  int maxRounds);

void StartRound(Judje* judje);

bool IsRaceOver(Judje* judje);

#endif
