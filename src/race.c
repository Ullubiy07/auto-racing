#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

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

static void ResetStats(Race* race) {
    race->stats.duration = 0.0;
}

void InitRace(Race* race, Track* track, const RaceSettings* settings) {
    race->track = track;
    race->settings = settings;
    race->carCount = settings->carCount;
    race->ctx.isOver = false;
    race->ctx.isInterrupted = false;

    pthread_mutex_init(&race->ctx.mutex, NULL);

    pthread_condattr_t attr;
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&race->ctx.cond, &attr);

    ResetStats(race);
    InitCars(race);
    InitJudge(&race->judge, race->cars, race->carCount, &race->settings->rules);
}

static void ClearScreen() {
    dprintf(1, "\x1b[2J");    // Очистить весь экран
    dprintf(1, "\x1b[0;0f");  // Переместить курсор в левый верхний угол
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
    DrawLeaderBoard(&race->judge, 1);
}

static void PaintStartLine(Race* race, int color) {
    for (int i = 0; i < race->track->height; ++i) {
        for (int j = 0; j < race->track->width; ++j) {
            Cell* cell = &race->track->map[i][j];
            if (IsCellStart(cell->type)) {
                cell->color = color;
            }
        }
    }
}

static void DrawRaceStart(Race* race) {
    if (!race->ctx.isInterrupted) {
        PaintStartLine(race, 31);
        DrawRace(race);
        sleep(1);
    }
    if (!race->ctx.isInterrupted) {
        PaintStartLine(race, 33);
        DrawRace(race);
        sleep(1);
    }
    if (!race->ctx.isInterrupted) {
        PaintStartLine(race, 32);
        DrawRace(race);
    }
}

typedef struct {
    Car* car;
    RaceContext* ctx;
} CarThreadArgs;

static void* RunCar(void* arg) {
    CarThreadArgs* args = arg;
    RaceContext* ctx = args->ctx;
    Car* car = args->car;

    int baseSleepTime = 1000 * 80;
    unsigned int seed = 0;

    for (int i = 0; car->driverName[i] != '\0'; ++i) {
        seed = seed * 37 + car->driverName[i];
    }

    bool (*TryMoveFunc)(Car*);

    while (!ctx->isOver) {
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

void TerminateRace(Race* race) {
    race->ctx.isInterrupted = true;
}

void RunRace(Race* race) {
    ResetStats(race);
    struct timespec startTime, endTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    DrawRaceStart(race);
    if (race->ctx.isInterrupted) {
        dprintf(1, "\n\x1b[33;1m[!] The race was forcibly interrupted by the user.\x1b[0m\n");
        return;
    }

    CarThreadArgs args[MAX_CARS];
    for (int i = 0; i < race->carCount; ++i) {
        args[i].car = &race->cars[i];
        args[i].ctx = &race->ctx;
        if (pthread_create(&race->cars[i].thread, NULL, RunCar, &args[i])) {
            perror("create car thread failed");
            exit(2);
        }
    }

    while (!IsRaceOver(&race->judge) && !race->ctx.isInterrupted) {
        DrawRace(race);
        usleep(race->settings->delayMs * 1000);
    }

    pthread_mutex_lock(&race->ctx.mutex);
    race->ctx.isOver = true;
    pthread_cond_broadcast(&race->ctx.cond);
    pthread_mutex_unlock(&race->ctx.mutex);

    for (int i = 0; i < race->carCount; ++i) {
        if (pthread_join(race->cars[i].thread, NULL)) {
            perror("join pthread failed");
        }
    }

    DrawRace(race);
    if (IsRaceOver(&race->judge)) {
        AnnounceWinner(&race->judge, 1);
    } else {
        dprintf(1, "\n\x1b[33;1m[!] The race was forcibly interrupted by the user.\x1b[0m\n");
    }

    clock_gettime(CLOCK_MONOTONIC, &endTime);
    race->stats.duration =
        (double) (endTime.tv_sec - startTime.tv_sec) + (double) (endTime.tv_nsec - startTime.tv_nsec) / 1e9;
}

void SaveRaceLog(Race* race) {
    time_t now = time(NULL);
    struct tm* localTime = localtime(&now);

    int fd;
    if (strlen(race->settings->logFile) == 0) {
        char fileName[250];
        strftime(fileName, sizeof(fileName), "../logs/race_%d-%m-%Y_%H-%M-%S.log", localTime);
        fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dprintf(1, "Race report log successfully saved to: %s\n", fileName);
    } else {
        fd = open(race->settings->logFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dprintf(1, "Race report log successfully saved to: %s\n", race->settings->logFile);
    }

    if (fd < 0) {
        perror("failed to open log file");
        return;
    }

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%d-%m-%Y %H:%M:%S", localTime);

    const char* status = race->ctx.isInterrupted ? "Interrupted by user (SIGINT)" : "Completed successfully";

    dprintf(fd, "==================================================\n");
    dprintf(fd, "              RACE SIMULATION REPORT              \n");
    dprintf(fd, "==================================================\n");
    dprintf(fd, "Timestamp          : %s\n", timestamp);
    dprintf(fd, "Status             : %s\n", status);
    dprintf(fd, "Winner             : %s\n", race->judge.leaderBoard[0]->driverName);
    dprintf(fd, "Total Duration     : %.2f s\n", race->stats.duration);
    dprintf(fd, "Participants       : %d\n", race->carCount);
    dprintf(fd, "\n");

    dprintf(fd, "=============================LEADER BOARD==============================\n");
    dprintf(fd, " %-5s | %-12s | %-12s | %-5s | %-5s | %-5s  | %-5s\n", "Place", "Team", "Driver", "Laps", "Lane",
            "Dist", "State");

    for (int i = 0; i < race->judge.carCount; ++i) {
        Car* car = race->judge.leaderBoard[i];

        pthread_mutex_lock(&car->mutex);
        dprintf(fd, "   %-3d | %-12s | %-12s | %-5d | %-5d |  %-5d | %-5s\n", i + 1, car->team.name, car->driverName,
                car->laps, car->cell->road.lane, car->cell->road.distToFinish, car->isOut ? "OUT" : "IN RACE");
        pthread_mutex_unlock(&car->mutex);
    }
    close(fd);
}
