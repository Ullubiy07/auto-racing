#include <stdio.h>

#include "track.h"
#include "race.h"

void ClearScreen() {
	dprintf(1, "\x1b[2J");    // Очистить весь экран
	dprintf(1, "\x1b[0;0f");  // Переместить курсор в левый верхний угол
}

int main() {
	Race race;
	Track track;
	Rules rules = (Rules){
		.carsInTeam = 2,
		.teamCount = 4,
		.maxLaps = 3,
		.maxRounds = 100,
		.startOrder = "ACEGBDFH"
	};
	
	LoadTrack("data/track1", &track);
	InitRace(&race, &track, &rules);
	DrawRace(&race);
	return 0;
}
