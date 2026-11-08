#include <asm-generic/errno-base.h>
#include <assert.h>
#include <semaphore.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "cell.h"
#include "common.h"
#include "judge.h"
#include "track.h"

#include "race.h"

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

static void ResetStats(Race* race) {
    race->stats.duration = 0.0;
    race->stats.status = "Completed successfully";
    race->stats.winner = NULL;
}

static int InitRaceResources(Race* race) {
    char buf[20];
    snprintf(buf, sizeof(buf), "race-shm-%d", getpid());

    int fd = shm_open(buf, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (fd == -1) {
        perror("Shared memory open failed");
        return 1;
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
        return 1;
    }

    race->resources = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (race->resources == MAP_FAILED) {
        perror("Memory map failed");
        if (close(fd) == -1) {
            perror("Close fd error");
        }
        if (shm_unlink(buf) == -1) {
            perror("Unlink shared memory failed");
        }
        return 1;
    }
    if (close(fd) == -1) {
        perror("Close fd error");
    }

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

    race->resources->carCount = race->settings->carCount;
    race->resources->state.isOver = 0;
    race->resources->state.isInterrupted = false;
    race->resources->carSettings = (CarSettings){.maxLaps = race->settings->rules.maxLaps,
                                                 .minSleepTimeMs = race->settings->carMinDelayMs,
                                                 .maxSleepTimeMs = race->settings->carMaxDelayMs};
    return 0;
}

int InitCarSemaphores(Race* race) {
    int pid = getpid();
    char carName[30];
    for (int i = 0; i < race->settings->carCount; ++i) {
        snprintf(carName, sizeof(carName), "race-%d-car-%d", pid, i);
        race->carSemaphores[i] = sem_open(carName, O_CREAT | O_EXCL, 0666, 1);
        if (race->carSemaphores[i] == SEM_FAILED) {
            perror("Open car semaphore failed");

            for (int j = 0; j < i; ++j) {
                if (sem_close(race->carSemaphores[j])) {
                    perror("Close car semaphore failed");
                }
                snprintf(carName, sizeof(carName), "race-%d-car-%d", pid, j);
                if (sem_unlink(carName)) {
                    perror("Unlink car semaphore failed");
                }
            }
            return 1;
        }
    }
    return 0;
}

int InitCarsReadySemaphore(Race* race) {
    char name[30];
    snprintf(name, sizeof(name), "race-cars-ready-%d", getpid());
    race->carsReady = sem_open(name, O_CREAT | O_EXCL, 0666, 0);
    if (race->carsReady == SEM_FAILED) {
        perror("Open cars ready semaphore failed");
        return 1;
    }
    return 0;
}

int InitCarsStoppedSemaphore(Race* race) {
    char name[30];
    snprintf(name, sizeof(name), "race-cars-stopped-%d", getpid());
    race->carsStopped = sem_open(name, O_CREAT | O_EXCL, 0666, 0);
    if (race->carsStopped == SEM_FAILED) {
        perror("Open cars stopped semaphore failed");
        return 1;
    }
    return 0;
}

void DestroyCarsReadySemaphore(Race* race) {
    char name[30];
    snprintf(name, sizeof(name), "race-cars-ready-%d", getpid());
    if (sem_close(race->carsReady)) {
        perror("Close cars ready semaphore failed");
    }
    if (sem_unlink(name)) {
        perror("Unlink cars ready semaphore failed");
    }
}

void DestroyCarsStoppedSemaphore(Race* race) {
    char name[30];
    snprintf(name, sizeof(name), "race-cars-stopped-%d", getpid());
    if (sem_close(race->carsStopped)) {
        perror("Close cars stopped semaphore failed");
    }
    if (sem_unlink(name)) {
        perror("Unlink cars stopped semaphore failed");
    }
}

void DestroyCarSemaphores(Race* race) {
    char name[30];
    for (int i = 0; i < race->settings->carCount; ++i) {
        if (sem_close(race->carSemaphores[i])) {
            perror("Close car semaphore failed");
        }
        snprintf(name, sizeof(name), "race-%d-car-%d", getpid(), i);
        if (sem_unlink(name)) {
            perror("Unlink car semaphore failed");
        }
    }
}

int InitRace(Race* race, const RaceSettings* settings) {
    race->settings = settings;
    ResetStats(race);

    int pid = getpid();
    if (InitCellSemaphores(&race->cellSemaphores, pid, true)) {
        return 1;
    }

    race->track = LoadTrack(settings->mapFile);
    if (!race->track) {
        goto cleanup_semaphores;
    }

    if (InitRaceResources(race)) {
        goto cleanup_track;
    }
    race->cars = (Car*) (race->resources + 1);

    if (InitCarsReadySemaphore(race)) {
        goto cleanup_resources;
    }

    if (InitCarsStoppedSemaphore(race)) {
        goto cleanup_cars_ready;
    }

    if (InitCarSemaphores(race)) {
        goto cleanup_cars_stopped;
    }
    return 0;

cleanup_cars_stopped:
    DestroyCarsStoppedSemaphore(race);
cleanup_cars_ready:
    DestroyCarsReadySemaphore(race);
cleanup_resources:
    DestroyRaceResources(race);
cleanup_track:
    DestroyTrack(race->track);
cleanup_semaphores:
    CloseCellSemaphores(&race->cellSemaphores);
    UnlinkCellSemaphores(pid);
    return 1;
}

void DestroyRace(Race* race) {
    CloseCellSemaphores(&race->cellSemaphores);
    UnlinkCellSemaphores(getpid());
    DestroyTrack(race->track);
    DestroyRaceResources(race);
    DestroyCarSemaphores(race);
    DestroyCarsReadySemaphore(race);
    DestroyCarsStoppedSemaphore(race);
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
            Entity e = CellEntity(cell, race->cellSemaphores.cells[cell->y][cell->x]);

            if (e.type == ENTITY_CAR) {
                dprintf(1, "\x1b[%d;1m🏎 \x1b[0m", race->cars[e.id].team.color);
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

static void StartRace(Race* race) {
    RaceState* state = &race->resources->state;
    pthread_mutex_lock(&state->mutex);
    state->isStarted = true;
    pthread_cond_broadcast(&state->cond);
    pthread_mutex_unlock(&state->mutex);
}

static void StopRace(Race* race) {
    RaceState* state = &race->resources->state;
    pthread_mutex_lock(&state->mutex);
    state->isOver = true;
    pthread_cond_broadcast(&state->cond);
    pthread_mutex_unlock(&state->mutex);

    for (int i = 0; i < race->carsReadyCount; ++i) {
        while (sem_wait(race->carsStopped) && errno == EINTR);
    }
}

typedef enum { WAIT_READY, WAIT_INTERRUPTED, WAIT_ERROR } WaitResult;

static WaitResult WaitForCarsReady(Race* race) {
    for (int i = 0; i < race->settings->carCount; ++i) {
        if (sem_wait(race->carsReady)) {
            if (race->resources->state.isInterrupted) {
                return WAIT_INTERRUPTED;
            }
            return WAIT_ERROR;
        }
        ++race->carsReadyCount;
    }
    return WAIT_READY;
}

static int PlaceCarsOnGrid(Race* race) {
    Point pos = race->track->grid.pos;
    Cell* start = &race->track->map[pos.y][pos.x];

    for (int i = 0; i < race->settings->carCount; ++i) {
        Entity e = {.id = i, .type = ENTITY_CAR};
        sem_t* sema = race->carSemaphores[i];
        pos = start->road.back;

        if (pos.x < 0 || pos.x >= race->track->width || pos.y < 0 || pos.y >= race->track->height) {
            dprintf(2, "Track capacity exceeded, max: %d, got: %d\n", i, race->settings->carCount);
            return 1;
        }

        start = &race->track->map[pos.y][pos.x];

        while (sem_wait(sema) && errno == EINTR);
        if (race->cars[i].isOut) {
            sem_post(sema);
            continue;
        }
        if (!TryTakeCell(start, e, race->cellSemaphores.cells[pos.y][pos.x])) {
            sem_post(sema);
            dprintf(2, "Starting cell is occupied\n");
            return 1;
        }
        race->cars[i].pos = pos;
        sem_post(sema);
    }
    return 0;
}

void TerminateRace(Race* race) {
    race->resources->state.isInterrupted = true;
}

int RunRace(Race* race) {
    int rc = 0;
    RaceResources* res = race->resources;
    RaceState* state = &res->state;

    race->judge.carCount = 0;
    WaitResult result = WaitForCarsReady(race);
    if (result == WAIT_INTERRUPTED) {
        StopRace(race);
        race->stats.status = "Interrupted by user";
        goto finish;
    } else if (result == WAIT_ERROR) {
        StopRace(race);
        return 1;
    }

    if (PlaceCarsOnGrid(race)) {
        StopRace(race);
        rc = 1;
        race->stats.status = "Place cars error";
        goto finish;
    }

    InitJudge(&race->judge, race->cars, race->carSemaphores, race->settings->carCount, race->track->map,
              &race->settings->rules);
    DrawRaceStart(race);
    if (state->isInterrupted) {
        StopRace(race);
        dprintf(1, "\n\x1b[33;1m[!] The race was forcibly interrupted by the user.\x1b[0m\n");
        race->stats.status = "Interrupted by user";
        goto finish;
    }

    ResetStats(race);
    struct timespec startTime, endTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);
    StartRace(race);

    while (!IsRaceOver(&race->judge) && !state->isInterrupted) {
        DrawRace(race);
        usleep(race->settings->drawDelayMs * 1000);
    }

    StopRace(race);
    DrawRace(race);

    if (IsRaceOver(&race->judge)) {
        if (CountCarsInRace(&race->judge) > 0) {
            Car* winner = race->judge.leaderBoard[0];
            dprintf(1, "\n\x1b[%d;1mWinner: %s, 🏎\x1b[0m\n", winner->team.color, winner->driverName);
            race->stats.winner = winner->driverName;
        } else {
            dprintf(1, "\n\x1b[33;1m[!] No winner - all cars are out\x1b[0m\n");
            race->stats.status = "All cars are out";
        }
    } else if (state->isInterrupted) {
        dprintf(1, "\n\x1b[33;1m[!] The race was forcibly interrupted by the user.\x1b[0m\n");
        race->stats.status = "Interrupted by user";
    } else {
        return 1;
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
    dprintf(fd, "Participants       : %d\n", race->carsReadyCount);

    if (race->judge.carCount > 0) {
        dprintf(fd, "\n");
        dprintf(fd, "=============================LEADER BOARD==============================\n");
        dprintf(fd, " %-5s | %-12s | %-12s | %-5s | %-5s | %-5s  | %-5s\n", "Place", "Team", "Driver", "Laps", "Lane",
                "Dist", "State");
    }

    for (int i = 0; i < race->judge.carCount; ++i) {
        Car* car = race->judge.leaderBoard[i];
        sem_t* sema = race->carSemaphores[car->id];

        while (sem_wait(sema) && errno == EINTR);
        int lane = -1;
        int distance = -1;

        if (car->pos.x >= 0 && car->pos.x < race->track->width && car->pos.y >= 0 && car->pos.y < race->track->height) {
            Cell* cell = &race->track->map[car->pos.y][car->pos.x];
            lane = cell->road.lane;
            distance = cell->road.distToFinish;
        }

        dprintf(fd, "   %-3d | %-12s | %-12s | %-5d | %-5d | %-5d | %-5s\n", i + 1, car->team.name, car->driverName,
                car->laps, lane, distance, car->isOut ? "OUT" : "IN RACE");
        sem_post(sema);
    }

    if (close(fd) == -1) {
        perror("Close fd failed");
    }
}
