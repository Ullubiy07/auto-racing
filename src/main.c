#include <time.h>
#include <stdlib.h>

#include "judje.h"
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
		
		.rules = (JudjeRules) {
			.lapsTotal = 3,
			.maxRounds = 20
		}
	};
	
	LoadTrack(&track, "data/track1");
	InitRace(&race, &track, &settings);
	
	StartRace(&race);
	return 0;
}
