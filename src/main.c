#include <time.h>
#include <stdlib.h>

#include "track.h"
#include "race.h"
#include "cli.h"

int main(int argc, char* argv[]) {
    srand(time(NULL));

    Race race;
    Track track;
    RaceSettings settings;

    if (!ParseCLIArgs(&settings, argc, argv)) {
        exit(2);
    }

    LoadTrack(&track, settings.mapFile);
    InitRace(&race, &track, &settings);
    StartRace(&race);
    return 0;
}
