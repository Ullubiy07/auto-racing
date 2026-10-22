#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

#include "track.h"
#include "race.h"
#include "cli.h"

Race race;

void RaceStopHandler(int sig) { StopRace(&race); }

int main(int argc, char* argv[]) {
    srand(time(NULL));
    signal(SIGINT, RaceStopHandler);

    Track track;
    RaceSettings settings;

    if (!ParseCLIArgs(&settings, argc, argv)) {
        return 1;
    }

    LoadTrack(&track, settings.mapFile);
    InitRace(&race, &track, &settings);
    RunRace(&race);
    return 0;
}
