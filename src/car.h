#ifndef CAR_H
#define CAR_H

#include "track.h"

// Команда, в которой состоят машины
typedef struct Team {
	int id;     // Идентификатор команды (1...)
	int color;  // Цвет команды
} Team;

// Возможные ходы машины
typedef enum {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_FORWARD
} MoveType;

// Машина, участвующая в гонке
typedef struct Car {
	int id;               // Идентификатор машины
	Team team;            // Команда, в которой состоит машина
	
	Point pos;            // Местоположение (x, y)
	Direction dir;        // Направление движения (dx, dy)
	int lane;             // Номер дорожки

	int distToFinish;     // Расстояние до финиша
	int laps;             // Число пройденных кругов
	Point prevPos;        // Предыдущее местоположение
	
	bool hasPassedStart;  // Прошла ли машина линию старта
	bool isOut;           // Сошла ли машина с дистанции
} Car;

// Инициализация машины на стартовой решетке
void InitCar(Car* car, int id, Team team, const Track* track, int slot);

// Движение машины в свободную клетку
bool MoveCar(Car* car, Track* track);

#endif
