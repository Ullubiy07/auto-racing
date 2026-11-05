#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>

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

static void DestroyRaceResources(Race* race) {
    pthread_cond_destroy(&race->resources->state.cond);
    pthread_mutex_destroy(&race->resources->state.mutex);

    int size = sizeof(RaceResources) + race->settings->carCount * sizeof(Car);
    if (munmap(race->resources, size) == -1) {
        perror("Unmap memory failed");
    }

    char name[20];
    snprintf(name, sizeof(name), "race-shm-%d", getpid());
    if (shm_unlink(name) == -1) {
        perror("Unlink shared memory failed");
    }
}

static int InitCars(Race* race) {
    RaceResources* res = race->resources;
    Point pos = race->track->grid.pos;
    Cell* start = &race->track->map[pos.y][pos.x];
    race->initCarCount = 0;

    for (int i = 0; i < race->carCount; ++i) {
        const char* driver = race->settings->startOrder[i];
        const TeamConfig* teamCfg = FindTeamByDriver(race->settings, driver);

        if (!teamCfg) {
            dprintf(2, "Driver %s has no team\n", driver);
            return 1;
        }

        Team team = (Team){.color = teamCfg->color, .name = (char*) teamCfg->name};
        Entity car = {.data = &res->cars[i], .type = ENTITY_CAR};

        start = start->road.back;
        if (!start || !TryTakeCell(start, car)) {
            dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, race->carCount);
            return 1;
        }

        InitCar(&res->cars[i], (char*) driver, team, start);
        ++race->initCarCount;
    }
    return 0;
}

static void ResetStats(Race* race) {
    race->stats.duration = 0.0;
    race->stats.status = "Completed successfully";
    race->stats.winner = NULL;
}

static RaceResources* NewRaceResources(Race* race) {
    char buf[20];
    snprintf(buf, sizeof(buf), "race-shm-%d", getpid());

    int fd = shm_open(buf, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("Shared memory open failed");
        exit(1);
    }

    size_t size = sizeof(RaceResources) + race->settings->carCount * sizeof(Car);
    if (ftruncate(fd, size) == -1) {
        perror("Truncate file failed");
        if (close(fd) == -1) {
            perror("Close fd error");
        }
        if (shm_unlink(buf) == -1) {
            perror("Unlink shared memory failed");
        }
        exit(1);
    }

    RaceResources* res = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (res == MAP_FAILED) {
        perror("Memory map failed");
        if (close(fd) == -1) {
            perror("Close fd error");
        }
        if (shm_unlink(buf) == -1) {
            perror("Unlink shared memory failed");
        }
        exit(1);
    }

    if (close(fd) == -1) {
        perror("Close fd error");
    }

    pthread_condattr_t condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&res->state.cond, &condAttr);
    pthread_condattr_destroy(&condAttr);

    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&res->state.mutex, &mutexAttr);
    pthread_mutexattr_destroy(&mutexAttr);
    return res;
}

int InitRace(Race* race, const RaceSettings* settings) {
    race->settings = settings;
    race->carCount = settings->carCount;
    ResetStats(race);

    race->resources = NewRaceResources(race);
    race->resources->cars = (Car*) (race->resources + 1);
    race->resources->state.isOver = 0;
    race->resources->state.isInterrupted = false;

    race->track = LoadTrack(settings->mapFile);
    if (!race->track) {
        DestroyRaceResources(race);
        return 1;
    }

    if (InitCars(race)) {
        DestroyRace(race);
        return 1;
    }
    InitJudge(&race->judge, race->resources->cars, race->carCount, &race->settings->rules);
    return 0;
}

void DestroyRace(Race* race) {
    for (int i = 0; i < race->initCarCount; ++i) {
        DestroyCar(&race->resources->cars[i]);
    }
    DestroyTrack(race->track);
    DestroyRaceResources(race);
}

static void ClearScreen() {
    dprintf(1, "\x1b[2J");    // Очистить весь экран
    dprintf(1, "\x1b[0;0f");  // Переместить курсор в левый верхний угол
}

static void DrawRace(Race* race) {
    Track* track = race->track;
    ClearScreen();

    for (int i = 0; i < track->height; ++i) {
        for (int j = 0; j < track->width; ++j) {
            Cell* cell = &track->map[i][j];
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

    if (!race->settings->mapOnly) {
        DrawLeaderBoard(&race->judge, 1);
    }
}

static void PaintStartLine(Race* race, int color) {
    Track* track = race->track;

    for (int i = 0; i < track->height; ++i) {
        for (int j = 0; j < track->width; ++j) {
            Cell* cell = &track->map[i][j];

            if (IsCellStart(cell->type)) {
                cell->color = color;
            }
        }
    }
}

static void DrawRaceStart(Race* race) {
    RaceState* state = &race->resources->state;

    if (!state->isInterrupted) {
        PaintStartLine(race, 31);
        DrawRace(race);
        sleep(1);
    }
    if (!state->isInterrupted) {
        PaintStartLine(race, 33);
        DrawRace(race);
        sleep(1);
    }
    if (!state->isInterrupted) {
        PaintStartLine(race, 32);
        DrawRace(race);
    }
}

typedef struct {
    int minSleepTimeMs;
    int maxSleepTimeMs;
    int maxLaps;
    RaceState* state;
} CarArgs;

static void* RunCar(Car* car, CarArgs args) {
    RaceState* state = args.state;
    unsigned int seed = 0;

    for (int i = 0; car->driverName[i] != '\0'; ++i) {
        seed = seed * 37 + car->driverName[i];
    }

    bool (*TryMoveFunc)(Car*);

    while (!state->isOver) {
        TryMoveFunc = TryBestMove;
        if (rand() % 2) {
            TryMoveFunc = TryRandomMove;
        }

        int sleepTimeMs = rand_r(&seed) % (args.maxSleepTimeMs - args.minSleepTimeMs + 1) + args.minSleepTimeMs;
        TryMoveFunc(car);

        while (sem_wait(&car->sema) && errno == EINTR);
        bool finished = car->laps >= args.maxLaps;
        sem_post(&car->sema);
        if (finished) {
            pthread_mutex_lock(&state->mutex);
            state->isOver = true;
            pthread_cond_broadcast(&state->cond);
            pthread_mutex_unlock(&state->mutex);
            break;
        }

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_sec += sleepTimeMs / 1000;
        sleepTimeMs %= 1000;
        ts.tv_nsec += sleepTimeMs * 1000000;
        if (ts.tv_nsec >= 1000000000L) {
            ++ts.tv_sec;
            ts.tv_nsec -= 1000000000L;
        }

        pthread_mutex_lock(&state->mutex);
        while (!state->isOver) {
            if (pthread_cond_timedwait(&state->cond, &state->mutex, &ts) == ETIMEDOUT) {
                break;
            }
        }
        pthread_mutex_unlock(&state->mutex);
    }
    return NULL;
}

static bool StartRace(Race* race) {
    RaceResources* res = race->resources;
    CarArgs args = {.minSleepTimeMs = race->settings->carMinDelayMs,
                    .maxSleepTimeMs = race->settings->carMaxDelayMs,
                    .maxLaps = race->settings->rules.maxLaps,
                    .state = &race->resources->state};

    for (int i = 0; i < race->carCount; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Fork car failed");
            return false;
        } else if (pid == 0) {
            RunCar(&res->cars[i], args);
            exit(0);
        }
    }
    return true;
}

static void StopRace(Race* race) {
    RaceState* state = &race->resources->state;
    pthread_mutex_lock(&state->mutex);
    state->isOver = true;
    pthread_cond_broadcast(&state->cond);
    pthread_mutex_unlock(&state->mutex);

    for (int i = 0; i < race->carCount; ++i) {
        pid_t pid;
        do {
            pid = wait(NULL);
        } while (pid == -1 && errno == EINTR);

        if (pid == -1) {
            if (errno == ECHILD) {
                break;
            }
            perror("Wait child process failed");
        }
    }
}

void TerminateRace(Race* race) {
    race->resources->state.isInterrupted = true;
}

int RunRace(Race* race) {
    int rc = 0;
    RaceResources* res = race->resources;
    RaceState* state = &res->state;

    DrawRaceStart(race);
    if (state->isInterrupted) {
        dprintf(1, "\n\x1b[33;1m[!] The race was forcibly interrupted by the user.\x1b[0m\n");
        race->stats.status = "Interrupted by user";
        goto finish;
    }

    ResetStats(race);
    struct timespec startTime, endTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    if (!StartRace(race)) {
        StopRace(race);
        race->stats.status = "Failed to start";
        rc = 1;
        goto finish;
    }

    while (!IsRaceOver(&race->judge) && !state->isInterrupted) {
        DrawRace(race);
        usleep(race->settings->drawDelayMs * 1000);
    }

    StopRace(race);
    DrawRace(race);

    if (IsRaceOver(&race->judge)) {
        Car* winner = race->judge.leaderBoard[0];
        dprintf(1, "\n\x1b[%d;1mWinner: %s, 🏎\x1b[0m\n", winner->team.color, winner->driverName);
        race->stats.winner = winner->driverName;
    } else if (state->isInterrupted) {
        dprintf(1, "\n\x1b[33;1m[!] The race was forcibly interrupted by the user.\x1b[0m\n");
        race->stats.status = "Interrupted by user";
    } else {
        dprintf(2, "Internal error");
        race->stats.status = "Internal error";
        rc = 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &endTime);
    race->stats.duration =
        (double) (endTime.tv_sec - startTime.tv_sec) + (double) (endTime.tv_nsec - startTime.tv_nsec) / 1e9;

finish:
    SaveRaceLog(race);
    return rc;
}

void SaveRaceLog(Race* race) {
    int fd = open(race->settings->logFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Open log file failed");
        return;
    }
    dprintf(1, "Race report log successfully saved to: %s\n", race->settings->logFile);

    dprintf(fd, "==================================================\n");
    dprintf(fd, "              RACE SIMULATION REPORT              \n");
    dprintf(fd, "==================================================\n");
    dprintf(fd, "Status             : %s\n", race->stats.status);
    dprintf(fd, "Winner             : %s\n", race->stats.winner ? race->stats.winner : "None");
    dprintf(fd, "Duration           : %.2f s\n", race->stats.duration);
    dprintf(fd, "Participants       : %d\n", race->carCount);
    dprintf(fd, "\n");

    dprintf(fd, "=============================LEADER BOARD==============================\n");
    dprintf(fd, " %-5s | %-12s | %-12s | %-5s | %-5s | %-5s  | %-5s\n", "Place", "Team", "Driver", "Laps", "Lane",
            "Dist", "State");

    for (int i = 0; i < race->judge.carCount; ++i) {
        Car* car = race->judge.leaderBoard[i];

        while (sem_wait(&car->sema) && errno == EINTR);
        dprintf(fd, "   %-3d | %-12s | %-12s | %-5d | %-5d |  %-5d | %-5s\n", i + 1, car->team.name, car->driverName,
                car->laps, car->cell->road.lane, car->cell->road.distToFinish, car->isOut ? "OUT" : "IN RACE");
        sem_post(&car->sema);
    }

    if (close(fd) == -1) {
        perror("Close fd failed");
    }
}
