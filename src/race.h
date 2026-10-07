#ifndef RACE_H
#define RACE_H

#include "car.h"
#include "track.h"
#include "judge.h"

enum {
	MAX_TEAM_NAME_SIZE = 32,
	MAX_DRIVER_NAME_SIZE = 32,
	MAX_CARS_IN_TEAM = 4
};

typedef struct {
	char name[MAX_TEAM_NAME_SIZE];
	char drivers[MAX_CARS_IN_TEAM][MAX_DRIVER_NAME_SIZE];
	int driverCount;
	int color;
} TeamConfig;

// Настройки гонки
typedef struct {
	TeamConfig teams[MAX_TEAMS];
	int teamCount;           // Количество команд
	int carCount;
	
	char startOrder[MAX_CARS][MAX_DRIVER_NAME_SIZE];  // Порядок старта: массив символов машин вдоль внешней дорожки
	int startOrderSize;
	
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

// Инициализировать настройки гонки по умолчанию
void InitDefaultSettings(RaceSettings* settings);

// Начать гонку
void StartRace(Race* race);

#endif
