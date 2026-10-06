#ifndef RACE_H
#define RACE_H

#include "car.h"
#include "track.h"
#include "judge.h"

// Настройки гонки
typedef struct {
	const char* startOrder;  // Порядок старта: массив символов машин вдоль внешней дорожки
	int teamCount;           // Количество команд
	int carsInTeam;          // Количество машин в команде
	JudgeRules rules;        // Правила гонки
} RaceSettings;

// Состояние гонки
typedef struct {
	Track* track;                  // Гоночная трасса
	const RaceSettings* settings;  // Настройки гонки
	Judge judge;                   // Судья, контролирующий гонку
	Car cars[MAX_CARS];            // Машины, участвующие в гонке
	int carCount;                  // Количество машин
} Race;

// Инициализировать гонку
void InitRace(Race* race, Track* track, const RaceSettings* settings);

// Начать гонку
void StartRace(Race* race);

#endif
