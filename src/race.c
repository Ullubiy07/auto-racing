#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

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
    race->ctx.isOver = false;

    pthread_mutex_init(&race->ctx.mutex, NULL);

    pthread_condattr_t attr;
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&race->ctx.cond, &attr);

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
            Cell* cell = &race->track->map[i][j];
            Entity e = CellEntity(cell);

            if (e.type == ENTITY_CAR) {
                Car* car = e.data;
                dprintf(1, "\x1b[%d;1m🏎 \x1b[0m", car->team.color);

                // Машина сошла с дистанции но клетка не очищена
                assert(!(e.type == ENTITY_CAR && car->isOut));

            } else {
                dprintf(1, "\x1b[%dm%s\x1b[0m", cell->color, GetCellInfo(cell->type)->outSymbol);
            }
        }
        dprintf(1, "\n");
    }
    DrawLeaderBoard(&race->judge);
}

void PaintStartLine(Race* race, int color) {
    for (int i = 0; i < race->track->height; ++i) {
        for (int j = 0; j < race->track->width; ++j) {
            Cell* cell = &race->track->map[i][j];
            if (IsCellStart(cell->type)) {
                cell->color = color;
            }
        }
    }
}

typedef struct {
    Car* car;
    RaceContext* ctx;
} CarThreadArgs;

void* RunCar(void* arg) {
    CarThreadArgs* args = arg;
    RaceContext* ctx = args->ctx;
    Car* car = args->car;

    int baseSleepTime = 1000 * 80;
    unsigned int seed = 0;

    for (int i = 0; car->driverName[i] != '\0'; ++i) {
        seed = seed * 37 + car->driverName[i];
    }

    bool (*TryMoveFunc)(Car*);

    while (true) {
        pthread_mutex_lock(&ctx->mutex);
        if (ctx->isOver) {
            pthread_mutex_unlock(&ctx->mutex);
            break;
        }
        pthread_mutex_unlock(&ctx->mutex);

        TryMoveFunc = TryBestMove;
        if (rand() % 2) {
            TryMoveFunc = TryRandomMove;
        }

        int sleepTime = baseSleepTime + (rand_r(&seed) % 100) * 1000;
        if (!TryMoveFunc(car)) {
            sleepTime += (rand_r(&seed) % 50) * 1000;
        }

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_nsec += sleepTime * 1000;
        if (ts.tv_nsec >= 1000000000) {
            ++ts.tv_sec;
            ts.tv_nsec -= 1000000000;
        }

        pthread_mutex_lock(&ctx->mutex);
        while (!ctx->isOver) {
            if (pthread_cond_timedwait(&ctx->cond, &ctx->mutex, &ts) == ETIMEDOUT) {
                break;
            }
        }
        pthread_mutex_unlock(&ctx->mutex);
    }
    return NULL;
}

void StopRace(Race* race) {
    pthread_mutex_lock(&race->ctx.mutex);
    race->ctx.isOver = true;
    pthread_mutex_unlock(&race->ctx.mutex);
    pthread_cond_broadcast(&race->ctx.cond);
}

void RunRace(Race* race) {
    PaintStartLine(race, 31);
    DrawRace(race);
    sleep(1);

    PaintStartLine(race, 33);
    DrawRace(race);
    sleep(1);

    PaintStartLine(race, 32);
    DrawRace(race);

    CarThreadArgs args[MAX_CARS];
    for (int i = 0; i < race->carCount; ++i) {
        args[i].car = &race->cars[i];
        args[i].ctx = &race->ctx;
        if (pthread_create(&race->cars[i].thread, NULL, RunCar, &args[i])) {
            perror("create car thread failed");
            exit(2);
        }
    }

    while (!IsRaceOver(&race->judge) && !race->ctx.isOver) {
        DrawRace(race);
        usleep(race->settings->delayMs * 1000);
    }

    StopRace(race);
    for (int i = 0; i < race->carCount; ++i) {
        if (pthread_join(race->cars[i].thread, NULL)) {
            perror("join pthread failed");
        }
    }

    DrawRace(race);
    AnnounceWinner(&race->judge);
}
