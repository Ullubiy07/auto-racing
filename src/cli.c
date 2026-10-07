#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "cli.h"
#include "race.h"

enum {
    MAX_COLOR_SIZE = 10,
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
    int argSize = strlen(arg);

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
    if (snprintf(settings->mapFile, sizeof(settings->mapFile), "%s", arg) >= sizeof(settings->mapFile)) {
        dprintf(2, "Map file path is too long: %zu chars\n", sizeof(settings->mapFile) - 1);
        return false;
    }
    return true;
}

static void PrintHelpUsage(const char* progName) {
    printf("Usage: %s [OPTION]...\n", progName);
    printf("Options:\n");
    printf("  -t, --team  \"NAME:COLOR:DRIVER1,DRIVER2,...\"   Add a team with drivers\n");
    printf(
        "  -s, --start \"DRIVER1,DRIVER2,...\"              Specify start order by driver names (default: random)\n");
    printf("  -r, --rounds N                                 Set max rounds (default: 80)\n");
    printf("  -l, --laps N                                   Set max laps (default: 5)\n");
    printf("  -d, --delay N                                  Set draw delay in ms (default: 50)\n");
    printf("  -m, --map FILE                                 Specify track map\n");
    printf("  -h, --help                                     Print help message\n");
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
        offset +=
            snprintf(arg + offset, sizeof(arg) - offset, "%s%s", cars[i], (i < settings->carCount - 1) ? "," : "");
    }
    return ParseStartOrder(settings, arg);
}

bool ParseCLIArgs(RaceSettings* settings, int argc, char* argv[]) {
    bool hasTeams = false;
    bool hasStartOrder = false;
    bool hasMap = false;

    static struct option longOptions[] = {
        {"team", required_argument, NULL, 't'},   {"start", required_argument, NULL, 's'},
        {"rounds", required_argument, NULL, 'r'}, {"laps", required_argument, NULL, 'l'},
        {"delay", required_argument, NULL, 'd'},  {"map", required_argument, NULL, 'm'},
        {"help", no_argument, NULL, 'h'},         {NULL, 0, NULL, 0}};
    int opt;

    settings->rules.maxLaps = 5;
    settings->rules.maxRounds = 80;
    settings->delayMs = 50;

    while ((opt = getopt_long(argc, argv, "t:s:r:l:h", longOptions, NULL)) != -1) {
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

        case 'r':
            settings->rules.maxRounds = atoi(optarg);
            break;

        case 'l':
            settings->rules.maxLaps = atoi(optarg);
            break;

        case 'd':
            settings->delayMs = atoi(optarg);
            break;

        case 'm':
            if (!ParseMapFile(settings, optarg)) {
                return false;
            }
            hasMap = true;
            break;

        case 'h':
        case '?':
            PrintHelpUsage(argv[0]);
            return false;

        default:
            return false;
        }
    }

    if (!hasTeams) {
        ParseTeam(settings, "Red Bull:cyan:Max,Liam");
        ParseTeam(settings, "Ferrari:red:Charles,Lewis");
        ParseTeam(settings, "Mercedes:blue:George,Kimi");
        ParseTeam(settings, "McLaren:yellow:Lando,Oscar");
        ParseTeam(settings, "Aston Martin:green:Fernando,Lance");
        ParseTeam(settings, "Alpine:purple:Pierre,Esteban");
        ParseTeam(settings, "Williams:white:Alex,Carlos");
        MakeRandomStartOrder(settings);

    } else if (!hasStartOrder && !MakeRandomStartOrder(settings)) {
        return false;
    }

    if (!hasMap) {
        ParseMapFile(settings, "../data/track2");
    }

    if (settings->startOrderSize != settings->carCount) {
        dprintf(2, "Invalid start order size, need: %d, got: %d\n", settings->carCount, settings->startOrderSize);
        return false;
    }

    return true;
}
