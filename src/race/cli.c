#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <arpa/inet.h>
#include "race.h"

#include "cli.h"

enum {
    MAX_COLOR_SIZE = 10,
};

enum Options { OPT_CAR_DELAY = 1000, OPT_MAP_ONLY };

enum Defaults {
    DEFAULT_PORT = 8080,
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

static void PrintHelp(const char* program) {
    dprintf(1,
            "Usage: %s [OPTION]...\n"
            "A console racing simulator on a grid-based circuit.\n\n"

            "Options:\n"
            "  -c, --cars N\n"
            "         Set the number of cars to wait for (required)\n"
            "  -a, --ip ADDRESS\n"
            "         Set the server IPv4 bind address (default: 0.0.0.0)\n"
            "  -p, --port PORT\n"
            "         Set the server port, from 1 to 65535 (default: %d)\n"
            "  -l, --laps N\n"
            "         Set the number of laps to win (default: %d)\n"
            "  -d, --draw-delay MS\n"
            "         Set the interval between screen updates (default: %d ms)\n"
            "      --car-delay MIN:MAX\n"
            "         Set the random delay between car moves in ms (default: %d:%d)\n"
            "  -m, --map FILE\n"
            "         Load a track map from the specified file\n"
            "      --map-only\n"
            "         Draw only the track map\n"
            "  -o, --output FILE\n"
            "         Save the race report to the specified file\n"
            "  -h, --help\n"
            "         Print this help message\n\n"

            "Example:\n"
            "  %s --cars 2\n",
            program, DEFAULT_PORT, DEFAULT_MAX_LAPS, DEFAULT_DRAW_DELAY, DEFAULT_CAR_MIN_DELAY, DEFAULT_CAR_MAX_DELAY,
            program);
}

bool ParseCLIArgs(RaceSettings* settings, int argc, char* argv[]) {
    bool hasMap = false;

    static struct option longOptions[] = {{"cars", required_argument, NULL, 'c'},
                                          {"ip", required_argument, NULL, 'a'},
                                          {"port", required_argument, NULL, 'p'},
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
    strcpy(settings->ip, "0.0.0.0");
    settings->port = DEFAULT_PORT;
    settings->rules.maxLaps = DEFAULT_MAX_LAPS;
    settings->drawDelayMs = DEFAULT_DRAW_DELAY;
    settings->carMinDelayMs = DEFAULT_CAR_MIN_DELAY;
    settings->carMaxDelayMs = DEFAULT_CAR_MAX_DELAY;

    time_t now = time(NULL);
    strftime(settings->logFile, sizeof(settings->logFile), "logs/race_%d-%m-%Y_%H-%M-%S.log", localtime(&now));

    while ((opt = getopt_long(argc, argv, "c:a:p:l:d:m:o:h", longOptions, NULL)) != -1) {
        switch (opt) {
        case 'a': {
            struct in_addr address;
            if (inet_pton(AF_INET, optarg, &address) != 1 || strlen(optarg) >= sizeof(settings->ip)) {
                dprintf(2, "Invalid server IPv4 address: %s\n", optarg);
                return false;
            }
            strcpy(settings->ip, optarg);
            break;
        }

        case 'p': {
            int port = atoi(optarg);
            if (port < 1 || port > 65535) {
                dprintf(2, "Server port must be an integer between 1 and 65535\n");
                return false;
            }
            settings->port = port;
            break;
        }

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
            if (laps <= 0) {
                dprintf(2, "Max laps must be positive\n");
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
            PrintHelp(argv[0]);
            exit(0);

        case '?':
            return false;

        default:
            return false;
        }
    }

    if (!hasMap) {
        ParseMapFile(settings, "data/twisty");
    }
    if (settings->carCount == 0) {
        dprintf(2, "Specify the number of cars with --cars N\n");
        return false;
    }
    return true;
}
