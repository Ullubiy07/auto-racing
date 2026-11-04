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

    RaceSettings settings;
    if (!ParseCLIArgs(&settings, argc, argv)) {
        return 1;
    }
    if (InitRace(&race, &settings)) {
        return 1;
    }

    signal(SIGINT, RaceSignalHandler);
    int rc = RunRace(&race);
    signal(SIGINT, SIG_IGN);
    DestroyRace(&race);
    return rc;
}
