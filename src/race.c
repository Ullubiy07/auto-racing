#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

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
    RaceResources* res = race->resources;
    Point pos = race->track->grid.pos;
    Cell* start = &race->track->map[pos.y][pos.x];

    for (int i = 0; i < race->carCount; ++i) {
        const char* driver = race->settings->startOrder[i];
        const TeamConfig* teamCfg = FindTeamByDriver(race->settings, driver);

        if (!teamCfg) {
            dprintf(2, "Driver %s has no team\n", driver);
            exit(1);
        }

        Team team = (Team){.color = teamCfg->color, .name = (char*) teamCfg->name};
        Entity car = {.data = &res->cars[i], .type = ENTITY_CAR};

        start = start->road.back;
        if (!start || !TryTakeCell(start, car)) {
            dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, race->carCount);
            exit(1);
        }

        InitCar(&res->cars[i], (char*) driver, team, start);
    }
}

static void ResetStats(Race* race) {
    race->stats.duration = 0.0;
}

static RaceResources* NewRaceResources(const RaceSettings* settings) {
    int fd = shm_open("race-shared-memory", O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("race: shared memory open failed");
        exit(1);
    }

    size_t size = sizeof(RaceResources) + settings->carCount * sizeof(Car);
    if (ftruncate(fd, size) == -1) {
        perror("Truncate file failed");
        if (close(fd) == -1) {
            perror("Close fd error");
        }
        if (shm_unlink("race-shared-memory") == -1) {
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
        if (shm_unlink("race-shared-memory") == -1) {
            perror("Unlink shared memory failed");
        }
        exit(1);
    }

    if (close(fd) == -1) {
        perror("Close fd error");
    }
    return res;
}

void InitRace(Race* race, const RaceSettings* settings) {
    race->resources = NewRaceResources(settings);
    race->track = LoadTrack(settings->mapFile);

    race->settings = settings;
    race->carCount = settings->carCount;
    race->resources->cars = (Car*) (race->resources + 1);
    race->resources->state.isOver = 0;
    race->resources->state.isInterrupted = false;

    pthread_condattr_t condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&race->resources->state.cond, &condAttr);
    pthread_condattr_destroy(&condAttr);

    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&race->resources->state.mutex, &mutexAttr);
    pthread_mutexattr_destroy(&mutexAttr);

    ResetStats(race);
    InitCars(race);
    InitJudge(&race->judge, race->resources->cars, race->carCount, &race->settings->rules);
}

static void DestroyRace(Race* race) {
    for (int i = 0; i < race->carCount; ++i) {
        DestroyCar(&race->resources->cars[i]);
    }
    DestroyTrack(race->track);

    int size = sizeof(RaceResources) + race->settings->carCount * sizeof(Car);
    if (munmap(race->resources, size) == -1) {
        perror("Unmap memory failed");
    }
    if (shm_unlink("race-shared-memory") == -1) {
        perror("Unlink shared memory failed");
    }
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
    DrawLeaderBoard(&race->judge, 1);
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

static void* RunCar(Car* car, RaceState* state) {
    int baseSleepTime = 1000 * 80;
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

void TerminateRace(Race* race) {
    race->resources->state.isInterrupted = true;
}

void RunRace(Race* race) {
    RaceResources* res = race->resources;
    RaceState* state = &res->state;

    ResetStats(race);
    struct timespec startTime, endTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    DrawRaceStart(race);
    if (state->isInterrupted) {
        dprintf(1, "\n\x1b[33;1m[!] The race was forcibly interrupted by the user.\x1b[0m\n");
        return;
    }

    for (int i = 0; i < race->carCount; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork car failed");
            exit(1);
        } else if (pid == 0) {
            RunCar(&res->cars[i], state);
            exit(0);
        }
    }

    while (!IsRaceOver(&race->judge) && !state->isInterrupted) {
        DrawRace(race);
        usleep(race->settings->delayMs * 1000);
    }

    pthread_mutex_lock(&state->mutex);
    state->isOver = true;
    pthread_cond_broadcast(&state->cond);
    pthread_mutex_unlock(&state->mutex);

    DrawRace(race);
    if (IsRaceOver(&race->judge)) {
        AnnounceWinner(&race->judge, 1);
    } else {
        dprintf(1, "\n\x1b[33;1m[!] The race was forcibly interrupted by the user.\x1b[0m\n");
    }

    clock_gettime(CLOCK_MONOTONIC, &endTime);
    race->stats.duration =
        (double) (endTime.tv_sec - startTime.tv_sec) + (double) (endTime.tv_nsec - startTime.tv_nsec) / 1e9;

    SaveRaceLog(race);
    DestroyRace(race);
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

    const char* status =
        race->resources->state.isInterrupted ? "Interrupted by user (SIGINT)" : "Completed successfully";
    const char* winner = race->resources->state.isInterrupted ? "None" : race->judge.leaderBoard[0]->driverName;

    dprintf(fd, "==================================================\n");
    dprintf(fd, "              RACE SIMULATION REPORT              \n");
    dprintf(fd, "==================================================\n");
    dprintf(fd, "Timestamp          : %s\n", timestamp);
    dprintf(fd, "Status             : %s\n", status);
    dprintf(fd, "Winner             : %s\n", winner);
    dprintf(fd, "Duration           : %.2f s\n", race->stats.duration);
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
