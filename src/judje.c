#include "judje.h"

void InitJudje(Judje* judje, Car* cars, int carCount, int maxRounds) {
	for (int i = 0; i < carCount; ++i) {
		judje->moveOrder[i] = &cars[i];
	}
		
	judje->carCount = carCount;
	judje->maxRounds = maxRounds;
	judje->curRound = 0;
}

void StartRound(Judje* judje) {
	++judje->curRound;
}

bool IsRaceOver(Judje* judje) {
	return judje->curRound > judje->maxRounds;
}
