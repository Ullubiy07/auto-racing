#include <errno.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "car.h"

int InitCar(Car* car, char* driverName, Team team, Cell* cell) {
    car->driverName = driverName;
    car->team = team;
    car->cell = cell;
    car->prevPos = (Point){-1, -1};
    car->laps = 0;
    car->hasPassedStart = false;
    car->isOut = false;

    if (sem_init(&car->sema, 1, 1)) {
        perror("Initialize car semaphore failed");
        return 1;
    }
    return 0;
}

void DestroyCar(Car* car) {
    if (sem_destroy(&car->sema)) {
        perror("Destroy car semaphore failed");
    }
}

static Cell* GetNextCell(Cell* cell, MoveType type) {
    switch (type) {
    case MOVE_FORWARD:
        return cell->road.forward;
    case MOVE_LEFT:
        return cell->road.left;
    case MOVE_RIGHT:
        return cell->road.right;
    case MOVE_NONE:
        return cell;
    }
    return NULL;
}

static bool CanMove(Car* car, MoveType type) {
    Cell* newCell = GetNextCell(car->cell, type);
    if (!newCell || (newCell->x == car->prevPos.x && newCell->y == car->prevPos.y)) {
        return false;
    }

    bool currentIsTurn = IsCellTurn(car->cell->type);
    if ((type == MOVE_LEFT || type == MOVE_RIGHT) && currentIsTurn) {
        return false;
    }
    return true;
}

static bool TryMove(Car* car, MoveType type) {
    if (!CanMove(car, type)) {
        return false;
    }

    Cell* newCell = GetNextCell(car->cell, type);
    Entity e = {.data = car, .type = ENTITY_CAR};
    if (!TryTakeCell(newCell, e)) {
        return false;
    }

    // Подсчет кругов
    if (IsCellStart(newCell->type) && !IsCellStart(car->cell->type)) {
        if (car->hasPassedStart) {
            ++car->laps;
        }
        car->hasPassedStart = true;
    }

    LeaveCell(car->cell);
    car->prevPos = (Point){car->cell->x, car->cell->y};
    car->cell = newCell;
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

bool TryRandomMove(Car* car) {
    MoveType moves[3] = {MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT};
    ShuffleMoves(moves, 3);

    while (sem_wait(&car->sema) && errno == EINTR);
    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(car, moves[i])) {
            sem_post(&car->sema);
            return true;
        }
    }
    sem_post(&car->sema);
    return false;
}

typedef struct {
    Car* car;
    MoveType type;
} Move;

static int CompareMoves(const void* lhs, const void* rhs) {
    Move* moveA = (Move*) lhs;
    Move* moveB = (Move*) rhs;
    Cell* cellA = GetNextCell(moveA->car->cell, moveA->type);
    Cell* cellB = GetNextCell(moveB->car->cell, moveB->type);
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

bool TryBestMove(Car* car) {
    Move moves[3] = {(Move){.car = car, .type = MOVE_FORWARD}, (Move){.car = car, .type = MOVE_RIGHT},
                     (Move){.car = car, .type = MOVE_LEFT}};

    qsort(moves, 3, sizeof(moves[0]), CompareMoves);

    while (sem_wait(&car->sema) && errno == EINTR);
    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(car, moves[i].type)) {
            sem_post(&car->sema);
            return true;
        }
    }
    sem_post(&car->sema);
    return false;
}
