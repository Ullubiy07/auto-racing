#ifndef CAR_H
#define CAR_H

#include "cell.h"

#include <stddef.h>

// Команда, в которой состоят машины
typedef struct Team {
    char* name;
    int color; // Цвет команды
} Team;

// Возможные ходы машины
typedef enum {
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_FORWARD,
    MOVE_NONE,
} MoveType;

// Машина, участвующая в гонке
typedef struct Car {
    char* driverName;
    Team team;        // Команда, в которой состоит машина
    const Cell* cell; // Местоположение (x, y)

    int laps;      // Число пройденных кругов
    Point prevPos; // Предыдущее местоположение

    bool hasPassedStart; // Прошла ли машина линию старта
    bool isBlocked;
    bool isOut; // Сошла ли машина с дистанции
} Car;

// Инициализация машины на стартовой решетке
void InitCar(Car* car, char* driverName, Team team, const Cell* cell);

size_t BuildRoute(const Car* car, MoveType* moves, int movesLimit, int maxBudget);

int GetMoveCost(const Cell* cell, MoveType type);

bool MakeMove(Car* car, MoveType type);

#endif
