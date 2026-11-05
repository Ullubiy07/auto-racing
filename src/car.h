#ifndef CAR_H
#define CAR_H

#include "cell.h"

#include <semaphore.h>

// Команда, в которой состоят машины
typedef struct Team {
    char* name;
    int color;  // Цвет команды
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
    Team team;   // Команда, в которой состоит машина
    Cell* cell;  // Местоположение (x, y)

    int laps;       // Число пройденных кругов
    Point prevPos;  // Предыдущее местоположение

    bool hasPassedStart;  // Прошла ли машина линию старта
    bool isOut;           // Сошла ли машина с дистанции

    sem_t* sema;
} Car;

// Инициализация машины на стартовой решетке
int InitCar(Car* car, char* driverName, Team team, Cell* cell);

void DestroyCar(Car* car);

bool TryRandomMove(Car* car);

bool TryBestMove(Car* car);

#endif
