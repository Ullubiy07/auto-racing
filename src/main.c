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
		.carsInTeam = 3,
		.teamCount = 3,
		.startOrder = "ACEBDFGHI",
		
		.rules = (JudgeRules) {
			.lapsTotal = 1,
			.maxRounds = 100
		}
	};
	
	LoadTrack(&track, "data/track1");
	InitRace(&race, &track, &settings);
	
	StartRace(&race);
	return 0;
}
