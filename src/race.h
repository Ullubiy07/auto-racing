#ifndef RACE_H
#define RACE_H

#include "car.h"
#include "common.h"
#include "track.h"
#include "judge.h"

#include <pthread.h>
#include <stdatomic.h>

enum {
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
    int teamCount;  // Количество команд
    int carCount;

    char startOrder[MAX_CARS][MAX_DRIVER_NAME_SIZE];  // Порядок старта: массив символов машин вдоль внешней дорожки
    int startOrderSize;

    JudgeRules rules;  // Правила гонки

    int drawDelayMs;
    int carMinDelayMs;
    int carMaxDelayMs;
    char mapFile[250];
    char logFile[250];
    bool mapOnly;
} RaceSettings;

typedef struct {
    double duration;
    char* status;
    char* winner;
} RaceStats;

// Состояние гонки
typedef struct {
    const RaceSettings* settings;  // Настройки для инитиализации гонки
    Track* track;                  // Гоночная трасса
    CellSemaphores cellSemaphores;
    sem_t* carSemaphores[MAX_CARS];
    sem_t* carsReady;
    sem_t* carsStopped;
    int carsReadyCount;
    
    Car* cars;
    Judge judge;                   // Судья, контролирующий гонку

    RaceStats stats;           // Статистика для сохранения лога
    RaceResources* resources;  // Ресурсы в разделяемой памяти
} Race;

// Инициализировать гонку
int InitRace(Race* race, const RaceSettings* settings);

// Запустить гонку
int RunRace(Race* race);

void DestroyRace(Race* race);

void TerminateRace(Race* race);

void SaveRaceLog(Race* race);

#endif
