#ifndef CAR_H
#define CAR_H

// Для структуры Point
#include "track.h"

// Структура, описывающая команду, участвующую в гонке
typedef struct Team {
	int id;     // Идентификатор команды (1...)
	int color;  // Цвет команды
} Team;

// Структура, описывающая автомобиль
typedef struct Car {
	Team team;  // Команда, в которой состоит машина
	Point pos;  // Местоположение (x, y)
	Point dir;  // Напарвление движения (dx, dy)
	int id;     // Идентификатор машины (1...)
	int lane;   // Номер дорожки
} Car;

#endif
