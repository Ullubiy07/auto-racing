#ifndef CAR_H
#define CAR_H

#include "cell.h"
#include "common.h"
#include "track.h"

#include <semaphore.h>

// Команда, в которой состоят машины
typedef struct Team {
    char name[MAX_TEAM_NAME_SIZE];
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
    int id;
    char driverName[MAX_DRIVER_NAME_SIZE];
    Team team;            // Команда, в которой состоит машина
    Point pos;            // Местоположение (x, y)
    int laps;             // Число пройденных кругов
    Point prevPos;        // Предыдущее местоположение
    bool hasPassedStart;  // Прошла ли машина линию старта
    bool isOut;           // Сошла ли машина с дистанции
} Car;

typedef struct {
    Car* car;
    sem_t* sema;
    Cell* cell;  // Текущая клетка машины
    int id;

    Cell* map[MAX_MAP_HEIGHT];  // Карта клеток
    CellSemaphores semas;       // Именованные семафоры клеток
    RaceResources* resources;   // Разделяемая память гонки
    int racePid;                // pid процесса гонки для подключения к разделяемой памяти
    
    Track* track;
    int trackSize;
    int resourcesSize;
} CarProcess;

// Инициализация машины на стартовой решетке
int InitCar(Car* car, int id, char* driverName, Team team);

bool TryRandomMove(CarProcess* proc);

bool TryBestMove(CarProcess* proc);

#endif
