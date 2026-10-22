#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "car.h"

void InitCar(Car* car, char* driverName, Team team, const Cell* cell) {
    car->driverName = driverName;
    car->team = team;
    car->cell = cell;

    car->prevPos = (Point){-1, -1};

    car->laps = 0;

    car->hasPassedStart = false;
    car->isStopped = false;
    car->isOut = false;
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

static bool TryRandomMove(Car* car) {
    MoveType moves[3] = {MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT};
    ShuffleMoves(moves, 3);

    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(car, moves[i])) {
            return true;
        }
    }
    return false;
}

static int CompareCells(const void* lhs, const void* rhs) {
    Cell* cellA = (Cell*) lhs;
    Cell* cellB = (Cell*) rhs;
    return cellA->road.distToFinish - cellB->road.distToFinish;
}

static bool TryBestMove(Car* car) {
    MoveType moves[3] = {MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT};
    const Cell* cells[3] = {GetNextCell(car->cell, MOVE_FORWARD), GetNextCell(car->cell, MOVE_LEFT),
                            GetNextCell(car->cell, MOVE_RIGHT)};

    qsort(cells, 3, sizeof(cells[0]), CompareCells);

    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(car, moves[i])) {
            return true;
        }
    }
    return false;
}

void StopCar(Car* car) {
    car->isStopped = true;
}

void* RunCar(void* arg) {
    Car* car = arg;

    int baseSleepTime = 1000 * 80;
    unsigned int seed = 0;

    for (int i = 0; car->driverName[i] != '\0'; ++i) {
        seed = seed * 37 + car->driverName[i];
    }

    bool (*TryMoveFunc)(Car*);

    while (!car->isStopped) {
        TryMoveFunc = TryBestMove;
        if (rand() % 2) {
            TryMoveFunc = TryRandomMove;
        }

        int sleepTime = baseSleepTime + (rand_r(&seed) % 100) * 1000;
        if (!TryMoveFunc(car)) {
            sleepTime += (rand_r(&seed) % 50) * 1000;
        }
        usleep(sleepTime);
    }
    return NULL;
}
