#ifndef RACE_H
#define RACE_H

#include "car.h"
#include "track.h"

enum {
	MAX_CARS = 15
};

// Структура, описывающая правила гонки (входные параметры)
typedef struct {
	int startOrder[MAX_CARS];  // Порядок старта: массив ID (1...N) команд, выстроенных вдоль внешней дорожки
	int teamCount;             // Количество команд
	int carsInTeam;            // Количество машин в команде
	int maxLaps;               // Число кругов
	int maxRounds;             // Предельное число раундов
	int bonus;
} Rules;

// Структура, описывающая гонку
typedef struct {
	const Track* track;  // Гоночная трасса
	const Rules* rules;  // Правила гонки
	int carCount;        // Количество машин
	Car cars[MAX_CARS];  // Машины в порядке лидирования
} Race;

// Создать гонку
void CreateRace(Race* r, const Track* track, const Rules* rules);

void StartRace(Race* r);

// Вывести текущее состояние гонки на дисплей
void DrawRace(Race* r);

#endif
