#include <assert.h>
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

        start = start->road.back;
        if (!start || !IsCellFree(start)) {
            dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, race->carCount);
            exit(2);
        }

        InitCar(&race->cars[i], (char*) driver, team, start);
        SetCellEntity(start, ENTITY_CAR, &race->cars[i]);
    }
}

void InitRace(Race* race, Track* track, const RaceSettings* settings) {
    race->track = track;
    race->settings = settings;
    race->carCount = settings->carCount;

    InitCars(race);
    InitJudge(&race->judge, race->cars, race->carCount, &race->settings->rules);
}

void ClearScreen() {
    dprintf(1, "\x1b[2J");   // Очистить весь экран
    dprintf(1, "\x1b[0;0f"); // Переместить курсор в левый верхний угол
}

static void DrawRace(const Race* race) {
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
    usleep(race->settings->delayMs * 1000);
}

static void PlayRound(Race* race) {
    Judge* judje = &race->judge;

    StartRound(judje);
    DrawRace(race);

    MoveType moves[3 * MAX_CARS];
    int prevMoveCost = 0;

    while (!IsRoundOver(judje)) {
        Car* car = GetCurrentCar(judje);

        int maxBudget = (judje->turn == 0 ? 4 : judje->turn == 1 ? prevMoveCost + 2 : prevMoveCost + 1);
        int actualCost = 0;

        size_t movesDone = BuildRoute(car, moves, 3 * MAX_CARS, maxBudget);
        for (int i = 0; i < movesDone; ++i) {
            actualCost += GetMoveCost(car->cell, moves[i]);
            MakeMove(car, moves[i]);
            DrawRace(race);
        }

        prevMoveCost = actualCost;
        if (movesDone == 0) {
            RegisterZeroMove(judje);
        }

        NextTurn(judje);
    }

    EndRound(judje);
    DrawRace(race);
}

void StartRace(Race* race) {
    while (!IsRaceOver(&race->judge)) {
        PlayRound(race);
    }
    AnnounceWinner(&race->judge);
}
