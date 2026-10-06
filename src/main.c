#include <time.h>
#include <stdlib.h>

#include "judge.h"
#include "track.h"
#include "race.h"

int main() {
	srand(time(0));
	
	Race race;
	Track track;
	
	RaceSettings settings = {
		.carsInTeam = 2,
		.teamCount = 3,
		.startOrder = "ACEBDF",
		
		.rules = (JudgeRules) {
			.lapsTotal = 1,
			.maxRounds = 100
		}
	};
	
	LoadTrack(&track, "data/track2");
	InitRace(&race, &track, &settings);
	
	StartRace(&race);
	return 0;
}
