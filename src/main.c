#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

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
		.carsInTeam = 4,
		.teamCount = 2,
		.maxLaps = 3,
		.maxRounds = 100,
		.startOrder = {1, 2, 3}
	};
	
	LoadTrack("data/track1", &track);
	CreateRace(&race, &track, &rules);
	return 0;
}
