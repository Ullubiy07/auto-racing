#include "judje.h"

void InitJudje(Judje* judje, Car* cars, int carCount, int maxRounds) {
	for (int i = 0; i < carCount; ++i) {
		judje->moveOrder[i] = &cars[i];
	}
		
	judje->carCount = carCount;
	judje->maxRounds = maxRounds;
	judje->roundsPlayed = 0;
}

void StartRound(Judje* judje) {
	++judje->roundsPlayed;
}

bool IsRaceOver(Judje* judje) {
	return judje->roundsPlayed >= judje->maxRounds;
}
