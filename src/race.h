#ifndef RACE_H
#define RACE_H

#include "car.h"
#include "track.h"
#include "judje.h"

enum {
	MAX_CARS = 26,
	MAX_TEAMS = 6,
};

// Структура, описывающая правила гонки (входные параметры)
typedef struct {
	const char* startOrder;  // Порядок старта: массив символов машин, выстроенных вдоль внешней дорожки
	int teamCount;           // Количество команд
	int carsInTeam;          // Количество машин в команде
	int maxLaps;             // Число кругов
	int maxRounds;           // Предельное число раундов
	int bonus;               // Бонус
} Rules;

// Структура, описывающая гонку
typedef struct {
	Track* track;  // Гоночная трасса
	const Rules* rules;  // Правила гонки
	const Judje* judje;
	int carCount;        // Количество машин
	Car cars[MAX_CARS];  // Машины в порядке лидирования
} Race;

// Инициализировать гонку
void InitRace(Race* race, Track* track, const Rules* rules, const Judje* judje);

void StartRace(Race* race);

// Вывести текущее состояние гонки на дисплей
void DrawRace(const Race* race);

#endif
