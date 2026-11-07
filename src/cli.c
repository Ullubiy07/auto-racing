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

typedef struct {
    char* name;
    int code;
} Color;

static int ParseColor(const char* color) {
    Color colors[28] = {
        {"RED", 31},  {"red", 31},    {"Red", 31},    {"31", 31},     {"GREEN", 32},  {"green", 32}, {"Green", 32},
        {"32", 32},   {"YELLOW", 33}, {"yellow", 33}, {"Yellow", 33}, {"33", 33},     {"BLUE", 34},  {"blue", 34},
        {"Blue", 34}, {"34", 34},     {"Purple", 35}, {"purple", 35}, {"Purple", 35}, {"35", 35},    {"CYAN", 36},
        {"cyan", 36}, {"Cyan", 36},   {"36", 36},     {"WHITE", 37},  {"white", 37},  {"White", 37}, {"37", 37},
    };

    for (int i = 0; i < 28; ++i) {
        if (strcmp(color, colors[i].name) == 0) {
            return colors[i].code;
        }
    }
    return -1;
}

static bool ParseTeam(RaceSettings* settings, const char* arg) {
    if (settings->teamCount >= MAX_TEAMS) {
        dprintf(2, "Teams limit exceeded, max: %d\n", MAX_TEAMS);
        return false;
    }

    size_t argSize = strlen(arg);
    char buf[MAX_TEAM_NAME_SIZE + MAX_CARS_IN_TEAM * (MAX_DRIVER_NAME_SIZE + 1) + MAX_COLOR_SIZE + 2];

    if (argSize >= sizeof(buf)) {
        dprintf(2, "Team argument string is too long\n");
        return false;
    }

    memcpy(buf, arg, argSize + 1);

    // Название команды
    char* teamName = strtok(buf, ":");
    if (!teamName) {
        dprintf(2, "Invalid team format. Expected \"NAME:COLOR:DRIVER1,DRIVER2,...\"\n");
        return false;
    }

    size_t teamNameSize = strlen(teamName);
    if (teamNameSize >= MAX_TEAM_NAME_SIZE) {
        dprintf(2, "Team name is too long, max: %d, got: %zu\n", MAX_TEAM_NAME_SIZE - 1, teamNameSize);
        return false;
    }

    TeamConfig* team = &settings->teams[settings->teamCount];
    memcpy(team->name, teamName, teamNameSize + 1);

    // Цвет команды
    char* color = strtok(NULL, ":");
    if (!color) {
        dprintf(2, "Missing team color\n");
        return false;
    }
    team->color = ParseColor(color);
    if (team->color == -1) {
        dprintf(2, "Invalid color: %s. Available colors: red, green, yellow, blue, purple, cyan, white\n", color);
        return false;
    }

    // Водители команды
    char* drivers = strtok(NULL, ":");
    if (!drivers) {
        dprintf(2, "Missing drivers\n");
        return false;
    }
    team->driverCount = 0;

    for (char* driver = strtok(drivers, ","); driver; driver = strtok(NULL, ",")) {
        if (team->driverCount >= MAX_CARS_IN_TEAM) {
            dprintf(2, "Exceeded max drivers per team limit: %d\n", MAX_CARS_IN_TEAM);
            return false;
        }

        size_t size = strlen(driver);
        if (size >= MAX_DRIVER_NAME_SIZE) {
            dprintf(2, "Driver name is too long, max: %d, got: %zu\n", MAX_DRIVER_NAME_SIZE - 1, size);
            return false;
        }

        memcpy(team->drivers[team->driverCount], driver, size + 1);
        ++team->driverCount;
        ++settings->carCount;
    }

    ++settings->teamCount;
    return true;
}

static bool ParseStartOrder(RaceSettings* settings, const char* arg) {
    char buf[MAX_CARS * (MAX_DRIVER_NAME_SIZE + 1)];
    size_t argSize = strlen(arg);

    if (argSize >= sizeof(buf)) {
        dprintf(2, "Start argument string is too long\n");
        return false;
    }

    memcpy(buf, arg, argSize + 1);
    settings->startOrderSize = 0;

    for (char* driver = strtok(buf, ","); driver; driver = strtok(NULL, ",")) {
        if (settings->startOrderSize >= MAX_CARS) {
            dprintf(2, "Start order size exceeds max number of cars: %d\n", MAX_CARS);
            return false;
        }

        size_t size = strlen(driver);
        if (size >= MAX_DRIVER_NAME_SIZE) {
            dprintf(2, "Driver name is too long, max: %d, got: %zu\n", MAX_DRIVER_NAME_SIZE - 1, size);
            return false;
        }

        memcpy(settings->startOrder[settings->startOrderSize], driver, size + 1);
        ++settings->startOrderSize;
    }
    return true;
}

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

    PrintfBold("  -t, --team \"NAME:COLOR:DRIVER1,DRIVER2,...\"\n");
    dprintf(1, "         Add a team with drivers of the same color\n");

    PrintfBold("  -s, --start \"DRIVER1,DRIVER2,...\"\n");
    dprintf(1, "         Arrange drivers on the starting grid (default: random)\n");

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

static bool MakeRandomStartOrder(RaceSettings* settings) {
    char cars[MAX_CARS][MAX_DRIVER_NAME_SIZE];

    for (int i = 0, k = 0; i < settings->teamCount; ++i) {
        for (int j = 0; j < settings->teams[i].driverCount; ++j, ++k) {
            char* driver = settings->teams[i].drivers[j];
            size_t size = strlen(driver);

            if (size >= MAX_DRIVER_NAME_SIZE) {
                dprintf(2, "Driver name is too long, max: %d, got: %zu\n", MAX_DRIVER_NAME_SIZE - 1, size);
                return false;
            }
            memcpy(cars[k], driver, size + 1);
        }
    }

    char temp[MAX_DRIVER_NAME_SIZE];

    for (int i = settings->carCount - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        memcpy(temp, cars[j], MAX_DRIVER_NAME_SIZE);
        memcpy(cars[j], cars[i], MAX_DRIVER_NAME_SIZE);
        memcpy(cars[i], temp, MAX_DRIVER_NAME_SIZE);
    }

    char arg[MAX_CARS * (MAX_DRIVER_NAME_SIZE + 1)] = {0};
    int offset = 0;

    for (int i = 0; i < settings->carCount; ++i) {
        int size =
            snprintf(arg + offset, sizeof(arg) - offset, "%s%s", cars[i], (i < settings->carCount - 1) ? "," : "");
        if (size < 0) {
            return false;
        }
        offset += size;
    }
    return ParseStartOrder(settings, arg);
}

bool ParseCLIArgs(RaceSettings* settings, int argc, char* argv[]) {
    bool hasTeams = false;
    bool hasStartOrder = false;
    bool hasMap = false;

    static struct option longOptions[] = {
        {"team", required_argument, NULL, 't'},        {"start", required_argument, NULL, 's'},
        {"laps", required_argument, NULL, 'l'},        {"draw-delay", required_argument, NULL, 'd'},
        {"map", required_argument, NULL, 'm'},         {"help", no_argument, NULL, 'h'},
        {"output", required_argument, NULL, 'o'},      {"car-delay", required_argument, NULL, OPT_CAR_DELAY},
        {"map-only", no_argument, NULL, OPT_MAP_ONLY}, {NULL, 0, NULL, 0}};
    int opt;

    memset(settings, 0, sizeof(RaceSettings));
    settings->rules.maxLaps = DEFAULT_MAX_LAPS;
    settings->drawDelayMs = DEFAULT_DRAW_DELAY;
    settings->carMinDelayMs = DEFAULT_CAR_MIN_DELAY;
    settings->carMaxDelayMs = DEFAULT_CAR_MAX_DELAY;

    time_t now = time(NULL);
    strftime(settings->logFile, sizeof(settings->logFile), "../logs/race_%d-%m-%Y_%H-%M-%S.log", localtime(&now));

    while ((opt = getopt_long(argc, argv, "t:s:l:d:m:o:h", longOptions, NULL)) != -1) {
        switch (opt) {
        case 't':
            if (!ParseTeam(settings, optarg)) {
                return false;
            }
            hasTeams = true;
            break;

        case 's':
            if (!ParseStartOrder(settings, optarg)) {
                return false;
            }
            hasStartOrder = true;
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

    if (!hasTeams) {
        ParseTeam(settings, "Red Bull:cyan:Max,Liam");
        // ParseTeam(settings, "Ferrari:red:Charles,Lewis");
        // ParseTeam(settings, "Mercedes:blue:George,Kimi");
        // ParseTeam(settings, "McLaren:yellow:Lando,Oscar");
        // ParseTeam(settings, "Aston Martin:green:Fernando,Lance");
        // ParseTeam(settings, "Alpine:purple:Pierre,Esteban");
        // ParseTeam(settings, "Williams:white:Alex,Carlos");

        if (!hasStartOrder) {
            MakeRandomStartOrder(settings);
        }

    } else if (!hasStartOrder && !MakeRandomStartOrder(settings)) {
        return false;
    }

    if (!hasMap) {
        ParseMapFile(settings, "../data/twisty");
    }

    if (settings->startOrderSize != settings->carCount) {
        dprintf(2, "Invalid start order size, need: %d, got: %d\n", settings->carCount, settings->startOrderSize);
        return false;
    }

    return true;
}
