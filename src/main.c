#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

#include "track.h"
#include "race.h"
#include "cli.h"

Race race;

void RaceSignalHandler(int sig) {
    if (sig == SIGINT) {
        TerminateRace(&race);
    }
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    signal(SIGINT, RaceSignalHandler);

    Track track;
    RaceSettings settings;

    if (!ParseCLIArgs(&settings, argc, argv)) {
        return 1;
    }

    LoadTrack(&track, settings.mapFile);
    InitRace(&race, &track, &settings);
    RunRace(&race);
    SaveRaceLog(&race);
    return 0;
}
