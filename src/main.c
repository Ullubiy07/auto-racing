#include <stdlib.h>
#include <signal.h>

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

    RaceSettings settings;
    if (!ParseCLIArgs(&settings, argc, argv)) {
        return 1;
    }

    InitRace(&race, &settings);
    RunRace(&race);
    return 0;
}
