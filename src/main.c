#include <time.h>
#include <stdlib.h>

#include "track.h"
#include "race.h"
#include "judje.h"

int main() {
	srand(time(0));
	
	Race race;
	Track track;
	Rules rules = (Rules){
		.carsInTeam = 2,
		.teamCount = 4,
		.maxLaps = 3,
		.maxRounds = 100,
		.startOrder = "ACEGBDFH"
	};
	Judje judje;
	
	LoadTrack("data/track1", &track);
	InitRace(&race, &track, &rules, &judje);
	DrawRace(&race);
	StartRace(&race);
	return 0;
}
