#include <time.h>
#include <stdlib.h>

#include "track.h"
#include "race.h"

int main() {
	srand(time(0));
	
	Race race;
	Track track;
	Rules rules = (Rules){
		.carsInTeam = 2,
		.teamCount = 3,
		.maxLaps = 3,
		.maxRounds = 20,
		.startOrder = "ACEBDF"
	};
	
	LoadTrack(&track, "data/track1");
	InitRace(&race, &track, &rules);
	
	StartRace(&race);
	return 0;
}
