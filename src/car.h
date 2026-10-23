#ifndef CAR_H
#define CAR_H

#include "cell.h"

#include <pthread.h>
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
    bool isOut;          // Сошла ли машина с дистанции
    
    pthread_mutex_t mutex;
    pthread_t thread;
} Car;

// Инициализация машины на стартовой решетке
void InitCar(Car* car, char* driverName, Team team, const Cell* cell);

bool TryRandomMove(Car* car);

bool TryBestMove(Car* car);

#endif
