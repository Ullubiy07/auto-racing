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
	// ClearScreen();
	Race race;
    LoadTrack("data/track1", &race.track);
	printf("%d %c\n", race.track.start.pos.x, GetCellSymbol(race.track.map[race.track.start.pos.y][race.track.start.pos.x]));
	return 0;
}
