#ifndef CAR_H
#define CAR_H

// Структура, описывающая команду, участвующую в гонке
typedef struct Team {
	int id;    // Идентификатор команды
	int color; // Цвет команды
} Team;

// Структура, описывающая автомобиль
typedef struct Car {
	Team team; // Команда, в которой состоит машина
	int dx;    // Смещение по x 
	int dy;    // Смещение по y
} Car;

#endif
