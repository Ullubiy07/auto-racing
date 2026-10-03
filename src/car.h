#ifndef CAR_H
#define CAR_H

#include "track.h"

// Структура, описывающая команду, участвующую в гонке
typedef struct Team {
	int id;     // Идентификатор команды (1...)
	int color;  // Цвет команды
} Team;

// Структура, описывающая автомобиль
typedef struct Car {
	char id;    // Идентификатор машины, а также символ для отображения
	Team team;  // Команда, в которой состоит машина
	
	Point pos;  // Местоположение (x, y)
	Point dir;  // Направление движения (dx, dy)
	int lane;   // Номер дорожки
	
	int is_finished;  // Завершила ли машина гонку
	int is_out;       // Сошла ли машина с дистанции
} Car;

// Инициализация машины на стартовой решетке
void InitCar(Car* car, char id, Team team, const Grid* grid, int slot);

// Движение машины в клетку
void Move(Car* car, Point pos, CellType type);

#endif
