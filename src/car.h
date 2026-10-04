#ifndef CAR_H
#define CAR_H

#include "track.h"

// Структура, описывающая команду, участвующую в гонке
typedef struct Team {
	int id;     // Идентификатор команды (1...)
	int color;  // Цвет команды
} Team;

typedef enum {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_FORWARD
} MoveType;

// Структура, описывающая автомобиль
typedef struct Car {
	int id;     // Идентификатор машины
	Team team;  // Команда, в которой состоит машина
	
	Point pos;      // Местоположение (x, y)
	Direction dir;  // Направление движения (dx, dy)
	int lane;       // Номер дорожки

	int laps;         // Число проеханных кругов
	Point prevPos;
	int is_finished;  // Завершила ли машина гонку
	int is_out;       // Сошла ли машина с дистанции
} Car;

// Инициализация машины на стартовой решетке
void InitCar(Car* car, int id, Team team, const Grid* grid, int slot);

// Движение машины в свободную клетку
bool MoveCar(Car* car, Track* track);

#endif
