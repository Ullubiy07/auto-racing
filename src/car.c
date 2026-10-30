#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "car.h"

void InitCar(Car* car, char* driverName, Team team, const Cell* cell) {
    car->driverName = driverName;
    car->team = team;
    car->cell = cell;

    car->prevPos = (Point){-1, -1};

    car->laps = 0;

    car->hasPassedStart = false;
    car->isOut = false;

    pthread_mutex_init(&car->mutex, NULL);
}

static const Cell* GetNextCell(const Cell* cell, MoveType type) {
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
    const Cell* newCell = GetNextCell(car->cell, type);
    if (!newCell || (newCell->x == car->prevPos.x && newCell->y == car->prevPos.y)) {
        return false;
    }

    bool currentIsTurn = IsCellTurn(car->cell->type);
    if ((type == MOVE_LEFT || type == MOVE_RIGHT) && currentIsTurn) {
        return false;
    }
    return true;
}

static void MoveCarToCell(Car* car, const Cell* newCell) {
    car->prevPos = (Point){car->cell->x, car->cell->y};
    car->cell = newCell;
}

static bool TryMove(Car* car, MoveType type) {
    if (!CanMove(car, type)) {
        return false;
    }

    const Cell* newCell = GetNextCell(car->cell, type);
    Entity e = {.data = car, .type = ENTITY_CAR};
    if (!TryTakeCell((Cell*) newCell, e)) {
        return false;
    }

    // Подсчет кругов
    if (IsCellStart(newCell->type) && !IsCellStart(car->cell->type)) {
        if (car->hasPassedStart) {
            ++car->laps;
        }
        car->hasPassedStart = true;
    }

    LeaveCell((Cell*) car->cell);
    MoveCarToCell(car, newCell);
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

    pthread_mutex_lock(&car->mutex);
    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(car, moves[i])) {
            pthread_mutex_unlock(&car->mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&car->mutex);
    return false;
}

typedef struct {
    Car* car;
    MoveType type;
} Move;

static int CompareMoves(const void* lhs, const void* rhs) {
    Move* moveA = (Move*) lhs;
    Move* moveB = (Move*) rhs;
    const Cell* cellA = GetNextCell(moveA->car->cell, moveA->type);
    const Cell* cellB = GetNextCell(moveB->car->cell, moveB->type);
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

    pthread_mutex_lock(&car->mutex);
    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(car, moves[i].type)) {
            pthread_mutex_unlock(&car->mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&car->mutex);
    return false;
}
