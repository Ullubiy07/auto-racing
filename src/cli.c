#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "race.h"

#include "cli.h"

enum {
    MAX_COLOR_SIZE = 10,
};

enum Options { OPT_CAR_DELAY = 1000, OPT_MAP_ONLY };

enum Defaults {
    DEFAULT_MAX_LAPS = 1,
    DEFAULT_DRAW_DELAY = 50,
    DEFAULT_CAR_MIN_DELAY = 80,
    DEFAULT_CAR_MAX_DELAY = 180
};

static bool ParseMapFile(RaceSettings* settings, const char* arg) {
    int size = snprintf(settings->mapFile, sizeof(settings->mapFile), "%s", arg);
    if (size < 0) {
        return false;
    }
    if ((size_t) size >= sizeof(settings->mapFile)) {
        dprintf(2, "Map file path is too long: %zu chars\n", sizeof(settings->mapFile) - 1);
        return false;
    }
    return true;
}

static bool ParseLogFile(RaceSettings* settings, const char* arg) {
    int size = snprintf(settings->logFile, sizeof(settings->logFile), "%s", arg);
    if (size < 0) {
        return false;
    }
    if ((size_t) size >= sizeof(settings->logFile)) {
        dprintf(2, "Log file path is too long: %zu chars\n", sizeof(settings->logFile) - 1);
        return false;
    }
    return true;
}

static bool ParseCarDelay(RaceSettings* settings) {
    char* minArg = strtok(optarg, ":");
    char* maxArg = strtok(NULL, ":");
    if (!minArg || !maxArg) {
        dprintf(2, "Expected car delay as MIN:MAX");
        return false;
    }

    int minDelay = atoi(minArg);
    int maxDelay = atoi(maxArg);

    if (minDelay < 0 || maxDelay < 0) {
        dprintf(2, "Car delay must be non-negative");
        return false;
    }
    if (minDelay > maxDelay) {
        dprintf(2, "Car min delay must be less than or equal max delay");
        return false;
    }
    settings->carMinDelayMs = minDelay;
    settings->carMaxDelayMs = maxDelay;
    return true;
}

static void PrintfBold(const char* text) {
    dprintf(1, "\033[1m%s\033[0m", text);
}

static void PrintHelpUsage(const char* progName) {
    dprintf(1, "Usage: %s [OPTION]...\n", progName);
    dprintf(1, "A console racing simulator on a grid-based circuit.\n\n");
    dprintf(1, "Options:\n");

    PrintfBold("  -c, --cars N\n");
    dprintf(1, "         Set the number of cars to wait for (required)\n");

    PrintfBold("  -l, --laps N\n");
    dprintf(1, "         Set the number of laps to win (default: %d)\n", DEFAULT_MAX_LAPS);

    PrintfBold("  -d, --draw-delay MS\n");
    dprintf(1, "         Set the interval between screen updates (default: %d ms)\n", DEFAULT_DRAW_DELAY);
    PrintfBold("      --car-delay MIN:MAX\n");
    dprintf(1, "         Set the random delay between car moves in ms (default: %d:%d)\n", DEFAULT_CAR_MIN_DELAY,
            DEFAULT_CAR_MAX_DELAY);

    PrintfBold("  -m, --map FILE\n");
    dprintf(1, "         Load a track map from the specified file\n");
    PrintfBold("      --map-only\n");
    dprintf(1, "         Draw only track map\n");

    PrintfBold("  -o, --output FILE\n");
    dprintf(1, "         Save the race report to the specified file\n");

    PrintfBold("  -h, --help\n");
    dprintf(1, "         Print help message\n");
}

bool ParseCLIArgs(RaceSettings* settings, int argc, char* argv[]) {
    bool hasMap = false;

    static struct option longOptions[] = {{"cars", required_argument, NULL, 'c'},
                                          {"laps", required_argument, NULL, 'l'},
                                          {"draw-delay", required_argument, NULL, 'd'},
                                          {"map", required_argument, NULL, 'm'},
                                          {"help", no_argument, NULL, 'h'},
                                          {"output", required_argument, NULL, 'o'},
                                          {"car-delay", required_argument, NULL, OPT_CAR_DELAY},
                                          {"map-only", no_argument, NULL, OPT_MAP_ONLY},
                                          {NULL, 0, NULL, 0}};
    int opt;

    memset(settings, 0, sizeof(RaceSettings));
    settings->rules.maxLaps = DEFAULT_MAX_LAPS;
    settings->drawDelayMs = DEFAULT_DRAW_DELAY;
    settings->carMinDelayMs = DEFAULT_CAR_MIN_DELAY;
    settings->carMaxDelayMs = DEFAULT_CAR_MAX_DELAY;

    time_t now = time(NULL);
    strftime(settings->logFile, sizeof(settings->logFile), "../logs/race_%d-%m-%Y_%H-%M-%S.log", localtime(&now));

    while ((opt = getopt_long(argc, argv, "c:l:d:m:o:h", longOptions, NULL)) != -1) {
        switch (opt) {
        case 'c':
            int cars = atoi(optarg);
            if (cars <= 0 || cars > MAX_CARS) {
                dprintf(2, "Cars number must be between 1 and %d\n", MAX_CARS);
                return false;
            }
            settings->carCount = cars;
            break;

        case 'l':
            int laps = atoi(optarg);
            if (laps < 0) {
                dprintf(2, "Max laps must be non-negative");
                return false;
            }
            settings->rules.maxLaps = laps;
            break;

        case 'd':
            int delay = atoi(optarg);
            if (delay <= 0) {
                dprintf(2, "Draw delay must be positive");
                return false;
            }
            settings->drawDelayMs = delay;
            break;

        case OPT_CAR_DELAY:
            if (!ParseCarDelay(settings)) {
                return false;
            }
            break;

        case OPT_MAP_ONLY:
            settings->mapOnly = true;
            break;

        case 'm':
            if (!ParseMapFile(settings, optarg)) {
                return false;
            }
            hasMap = true;
            break;

        case 'o':
            if (!ParseLogFile(settings, optarg)) {
                return false;
            }
            break;

        case 'h':
        case '?':
            PrintHelpUsage(argv[0]);
            exit(0);
            break;

        default:
            return false;
        }
    }

    if (!hasMap) {
        ParseMapFile(settings, "../data/twisty");
    }
    if (settings->carCount == 0) {
        dprintf(2, "Specify the number of cars with --cars N\n");
        return false;
    }
    return true;
}
