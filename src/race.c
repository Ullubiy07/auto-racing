#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "race.h"

static const TeamConfig* FindTeamByDriver(const RaceSettings* settings, const char* driverName) {
    for (int i = 0; i < settings->teamCount; ++i) {
        const TeamConfig* team = &settings->teams[i];

        for (int j = 0; j < team->driverCount; ++j) {
            if (strcmp(team->drivers[j], driverName) == 0) {
                return team;
            }
        }
    }
    return NULL;
}

static void InitCars(Race* race) {
    int used[MAX_CARS] = {0};
    Point pos = race->track->grid.pos;
    Cell* start = &race->track->map[pos.y][pos.x];

    for (int i = 0; i < race->carCount; ++i) {
        const char* driver = race->settings->startOrder[i];
        const TeamConfig* teamCfg = FindTeamByDriver(race->settings, driver);

        if (!teamCfg) {
            dprintf(2, "Driver %s has no team\n", driver);
            exit(2);
        }

        Team team = (Team){.color = teamCfg->color, .name = (char*) teamCfg->name};
        Entity car = {.data = &race->cars[i], .type = ENTITY_CAR};

        start = start->road.back;
        if (!start || !TryTakeCell(start, car)) {
            dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, race->carCount);
            exit(2);
        }

        InitCar(&race->cars[i], (char*) driver, team, start);
    }
}

void InitRace(Race* race, Track* track, const RaceSettings* settings) {
    race->track = track;
    race->settings = settings;
    race->carCount = settings->carCount;
    race->isStopped = false;

    InitCars(race);
    InitJudge(&race->judge, race->cars, race->carCount, &race->settings->rules);
}

void ClearScreen() {
    dprintf(1, "\x1b[2J");   // Очистить весь экран
    dprintf(1, "\x1b[0;0f"); // Переместить курсор в левый верхний угол
}

static void DrawRace(Race* race) {
    ClearScreen();
    for (int i = 0; i < race->track->height; ++i) {
        for (int j = 0; j < race->track->width; ++j) {
            Cell cell = race->track->map[i][j];
            Car* car = cell.entity.data;

            // Машина сошла с дистанции но клетка не очищена
            assert(!(cell.entity.type == ENTITY_CAR && car->isOut));

            if (cell.entity.type == ENTITY_CAR && !car->isOut) {
                dprintf(1, "\x1b[%d;1m🏎 \x1b[0m", car->team.color);
            } else {
                dprintf(1, "%s", GetCellInfo(cell.type)->outSymbol);
            }
        }
        dprintf(1, "\n");
    }
    DrawLeaderBoard(&race->judge);
}

void StopRace(Race* race) { race->isStopped = true; }

void RunRace(Race* race) {
    DrawRace(race);
    usleep(1000 * 1000);

    for (int i = 0; i < race->carCount; ++i) {
        if (pthread_create(&race->cars[i].thread, NULL, RunCar, &race->cars[i])) {
            perror("create car thread failed");
            exit(2);
        }
    }

    while (!IsRaceOver(&race->judge) && !race->isStopped) {
        DrawRace(race);
        usleep(race->settings->delayMs * 1000);
    }

    for (int i = 0; i < race->carCount; ++i) {
        StopCar(&race->cars[i]);
        if (pthread_join(race->cars[i].thread, NULL)) {
            perror("join pthread failed");
        }
    }

    DrawRace(race);
    AnnounceWinner(&race->judge);
}
