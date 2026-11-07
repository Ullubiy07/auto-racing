#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "common.h"
#include "car.h"

static int InitCarProcess(CarProcess* proc, int id, int racePid) {
    proc->racePid = racePid;
    proc->id = id;
    proc->cell = NULL;

    char raceName[20];
    snprintf(raceName, sizeof(raceName), "race-shm-%d", racePid);
    int raceFd = shm_open(raceName, O_RDWR, 0);
    if (raceFd == -1) {
        perror("Race shared memory open failed");
        return 1;
    }

    struct stat info;
    if (fstat(raceFd, &info)) {
        perror("Get race size failed");
        if (close(raceFd) == -1) {
            perror("Close race fd error");
        }
        return 1;
    }

    void* resources = mmap(NULL, info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, raceFd, 0);

    if (resources == MAP_FAILED) {
        perror("Memory map failed");
        if (close(raceFd) == -1) {
            perror("Close race fd error");
        }
        return 1;
    }
    if (close(raceFd) == -1) {
        perror("Close race fd error");
    }

    proc->resources = resources;
    proc->resourcesSize = info.st_size;
    
    if (id < 0 || id >= proc->resources->carCount) {
        dprintf(2, "Invalid car id\n");
        goto cleanup_resources;
    }
    
    pthread_mutex_lock(&proc->resources->state.mutex);
    bool occupied = proc->resources->carRegistered[id];
    if (!occupied) {
        proc->resources->carRegistered[id] = true;
    }
    pthread_mutex_unlock(&proc->resources->state.mutex);
    
    if (occupied) {
        dprintf(2, "Car id is already taken");
        goto cleanup_resources;
    }
    
    proc->car = (Car*) ((char*) resources + sizeof(RaceResources) + id * sizeof(Car));

    char trackName[20];
    snprintf(trackName, sizeof(trackName), "track-shm-%d", racePid);
    int trackFd = shm_open(trackName, O_RDWR, 0);
    if (trackFd == -1) {
        perror("Track shared memory open failed");
        goto cleanup_id_register;
    }

    if (fstat(trackFd, &info)) {
        perror("Get track size failed");
        if (close(trackFd) == -1) {
            perror("Close track fd error");
        }
        goto cleanup_id_register;
    }

    Track* track = mmap(NULL, info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, trackFd, 0);
    if (track == MAP_FAILED) {
        perror("Map track failed");
        if (close(trackFd) == -1) {
            perror("Close track fd error");
        }
        goto cleanup_id_register;
    }
    if (close(trackFd) == -1) {
        perror("Close track fd error");
    }
    proc->track = track;
    proc->trackSize = info.st_size;

    Cell* cells = (Cell*) ((char*) track + sizeof(Track) + track->height * sizeof(Cell*));
    for (int y = 0; y < track->height; ++y) {
        proc->map[y] = cells + y * track->width;
    }

    char name[50];
    snprintf(name, sizeof(name), "race-%d-car-%d", racePid, id);
    proc->sema = sem_open(name, 0);
    if (proc->sema == SEM_FAILED) {
        perror("Open car semaphore failed");
        goto cleanup_track;
    }

    if (InitCellSemaphores(&proc->semas, racePid, false)) {
        goto cleanup_sema;
    }
    return 0;

cleanup_sema:
    if (sem_close(proc->sema)) {
        perror("Close car semaphore failed");
    }
cleanup_track:
    if (munmap(proc->track, proc->trackSize)) {
        perror("Unmap track shared memory failed");
    }
cleanup_id_register:
    pthread_mutex_lock(&proc->resources->state.mutex);
    proc->resources->carRegistered[id] = false;
    pthread_mutex_unlock(&proc->resources->state.mutex);
cleanup_resources:
    if (munmap(proc->resources, proc->resourcesSize)) {
        perror("Unmap race shared memory failed");
    }
    return 1;
}

int InitCar(Car* car, int id, char* driverName, Team team) {
    car->id = id;
    strcpy(car->driverName, driverName);
    car->team = team;
    car->prevPos = (Point){-1, -1};
    car->laps = 0;
    car->hasPassedStart = false;
    car->isOut = false;
    return 0;
}

void DestroyCarProcess(CarProcess* proc) {
    if (sem_close(proc->sema)) {
        perror("Close car semaphore failed");
    }
    if (munmap(proc->track, proc->trackSize)) {
        perror("Unmap track shared memory failed");
    }
    if (munmap(proc->resources, proc->resourcesSize)) {
        perror("Unmap race shared memory failed");
    }
    CloseCellSemaphores(&proc->semas);
}

static Cell* GetNextCell(CarProcess* proc, MoveType type) {
    Point pos;

    switch (type) {
    case MOVE_FORWARD:
        pos = proc->cell->road.forward;
        break;
    case MOVE_LEFT:
        pos = proc->cell->road.left;
        break;
    case MOVE_RIGHT:
        pos = proc->cell->road.right;
        break;
    case MOVE_NONE:
        return proc->cell;
    default:
        return NULL;
    }

    if (pos.x < 0 || pos.x >= proc->track->width || pos.y < 0 || pos.y >= proc->track->height) {
        return NULL;
    }
    return &proc->map[pos.y][pos.x];
}

static bool CanMove(CarProcess* proc, MoveType type) {
    Cell* newCell = GetNextCell(proc, type);
    if (!newCell || (newCell->x == proc->car->prevPos.x && newCell->y == proc->car->prevPos.y)) {
        return false;
    }

    bool currentIsTurn = IsCellTurn(proc->cell->type);
    if ((type == MOVE_LEFT || type == MOVE_RIGHT) && currentIsTurn) {
        return false;
    }
    return true;
}

static bool TryMove(CarProcess* proc, MoveType type) {
    if (!CanMove(proc, type)) {
        return false;
    }

    Cell* newCell = GetNextCell(proc, type);
    Car* car = proc->car;
    Entity e = {.id = car->id, .type = ENTITY_CAR};
    if (!TryTakeCell(newCell, e, proc->semas.cells[newCell->y][newCell->x])) {
        return false;
    }

    // Подсчет кругов
    if (IsCellStart(newCell->type) && !IsCellStart(proc->cell->type)) {
        if (car->hasPassedStart) {
            ++car->laps;
        }
        car->hasPassedStart = true;
    }

    LeaveCell(proc->cell, proc->semas.cells[proc->cell->y][proc->cell->x]);
    car->prevPos = car->pos;
    proc->cell = newCell;
    car->pos = (Point){newCell->x, newCell->y};
    return true;
}

static void ShuffleMoves(MoveType* moves, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        int j = rand() % (i + 1);
        int temp = moves[i];
        moves[i] = moves[j];
        moves[j] = temp;
    }
}

bool TryRandomMove(CarProcess* proc) {
    MoveType moves[3] = {MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT};
    ShuffleMoves(moves, 3);

    while (sem_wait(proc->sema) && errno == EINTR);
    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(proc, moves[i])) {
            sem_post(proc->sema);
            return true;
        }
    }
    sem_post(proc->sema);
    return false;
}

typedef struct {
    CarProcess* proc;
    MoveType type;
} Move;

static int CompareMoves(const void* lhs, const void* rhs) {
    Move* moveA = (Move*) lhs;
    Move* moveB = (Move*) rhs;
    Cell* cellA = GetNextCell(moveA->proc, moveA->type);
    Cell* cellB = GetNextCell(moveB->proc, moveB->type);
    if (!cellA && !cellB) {
        return 0;
    }
    if (!cellA) {
        return 1;
    }
    if (!cellB) {
        return -1;
    }
    return cellA->road.distToFinish - cellB->road.distToFinish;
}

bool TryBestMove(CarProcess* proc) {
    Move moves[3] = {(Move){.proc = proc, .type = MOVE_FORWARD}, (Move){.proc = proc, .type = MOVE_RIGHT},
                     (Move){.proc = proc, .type = MOVE_LEFT}};

    qsort(moves, 3, sizeof(moves[0]), CompareMoves);

    while (sem_wait(proc->sema) && errno == EINTR);
    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(proc, moves[i].type)) {
            sem_post(proc->sema);
            return true;
        }
    }
    sem_post(proc->sema);
    return false;
}

static int SignalReady(int racePid) {
    char name[30];
    snprintf(name, sizeof(name), "race-cars-ready-%d", racePid);
    sem_t* sema = sem_open(name, 0);
    if (sema == SEM_FAILED) {
        perror("Open cars ready semaphore failed");
        return 1;
    }
    sem_post(sema);
    if (sem_close(sema)) {
        perror("Close cars ready semaphore failed");
        return 1;
    }
    return 0;
}

static int SignalStopped(int racePid) {
    char name[30];
    snprintf(name, sizeof(name), "race-cars-stopped-%d", racePid);
    sem_t* sema = sem_open(name, 0);
    if (sema == SEM_FAILED) {
        perror("Open cars stopped semaphore failed");
        return 1;
    }
    sem_post(sema);
    if (sem_close(sema)) {
        perror("Close cars stopped semaphore failed");
        return 1;
    }
    return 0;
}

static void* RunCar(CarProcess* proc) {
    CarSettings* settings = &proc->resources->carSettings;
    RaceState* state = &proc->resources->state;
    Car* car = proc->car;
    unsigned int seed = 0;

    for (int i = 0; car->driverName[i] != '\0'; ++i) {
        seed = seed * 37 + car->driverName[i];
    }

    bool (*TryMoveFunc)(CarProcess*);

    SignalReady(proc->racePid);
    pthread_mutex_lock(&state->mutex);
    while (!state->isStarted && !state->isOver) {
        pthread_cond_wait(&state->cond, &state->mutex);
    }
    pthread_mutex_unlock(&state->mutex);

    proc->cell = &proc->map[car->pos.y][car->pos.x];

    while (!state->isOver) {
        TryMoveFunc = TryBestMove;
        if (rand() % 2) {
            TryMoveFunc = TryRandomMove;
        }

        int sleepTimeMs =
            rand_r(&seed) % (settings->maxSleepTimeMs - settings->minSleepTimeMs + 1) + settings->minSleepTimeMs;
        TryMoveFunc(proc);

        while (sem_wait(proc->sema) && errno == EINTR);
        bool finished = car->laps >= settings->maxLaps;
        sem_post(proc->sema);
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

int main(int argc, char* argv[]) {
    int id = atoi(argv[1]);
    char* driverName = argv[2];
    Team team = {.color = atoi(argv[4])};
    strcpy(team.name, argv[3]);

    int racePid = atoi(argv[5]);

    CarProcess proc;

    if (InitCarProcess(&proc, id, racePid)) {
        return 1;
    }
    InitCar(proc.car, id, driverName, team);

    RunCar(&proc);
    DestroyCarProcess(&proc);
    SignalStopped(proc.racePid);
    return 0;
}
