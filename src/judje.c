#include "judje.h"

void InitJudje(Judje* judje, Car* cars, int carCount, const JudjeRules* rules) {
	judje->rules = rules;
	judje->carCount = carCount;
	
	for (int i = 0; i < carCount; ++i) {
		judje->moveOrder[i] = &cars[i];
	}
	
	judje->roundsPlayed = 0;
}

void StartRound(Judje* judje) {
	++judje->roundsPlayed;
}

bool IsRaceOver(Judje* judje) {
	return judje->roundsPlayed >= judje->rules->maxRounds || judje->IsRaceOver;
}
