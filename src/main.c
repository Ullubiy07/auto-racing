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
		.teamCount = 4,
		.maxLaps = 3,
		.maxRounds = 4,
		.startOrder = "ACEGBDFH"
	};
	
	LoadTrack("data/track1", &track);
	InitRace(&race, &track, &rules);
	
	StartRace(&race);
	return 0;
}
