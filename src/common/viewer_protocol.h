#ifndef VIEWER_PROTOCOL_H
#define VIEWER_PROTOCOL_H

#include "common.h"
#include "judge.h"

// Этап гонки, отображаемый наблюдателем.
typedef enum { RACE_WAITING, RACE_RUNNING, RACE_FINISHED, RACE_INTERRUPTED, RACE_FAILED } RaceState;

// Результат регистрации наблюдателя.
typedef struct {
    bool accepted;
} ViewerRegistrationResponse;

// Неизменяемая карта и настройки передаются один раз после регистрации.
typedef struct {
    int mapWidth;
    int mapHeight;
    CellData map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];  // Статическая карта; машины берутся из снимков
    int expectedCarCount;                         // Сколько машин сервер ждёт перед стартом
    JudgeRules rules;
} ViewerInitMessage;

// Цельный снимок гонки; standings идут в порядке ID, сортирует наблюдатель.
typedef struct {
    RaceState state;
    int carCount;                     // Зарегистрированные машины, включая сошедшие
    CarStanding standings[MAX_CARS];  // Индекс совпадает с ID машины
    int winnerId;                     // -1, если победителя нет
} RaceSnapshot;

#endif
