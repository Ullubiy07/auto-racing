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

    struct sigaction action = {0};
    action.sa_handler = RaceSignalHandler;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);

    int rc = RunRace(&race);

    action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &action, NULL);

    DestroyRace(&race);
    return rc;
}
