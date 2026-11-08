#ifndef JUDJE_H
#define JUDJE_H

#include <stdbool.h>

#include "car.h"

// Правила, которыми руководствуется судья
typedef struct {
    int maxLaps;  // Всего кругов
} JudgeRules;

// Судья, отслеживающий ход гонки и таблицу лидеров
typedef struct {
    const JudgeRules* rules;

    Car* leaderBoard[MAX_CARS];            // Таблица лидеров
    sem_t** carSemaphores;
    Cell** map;
    int carCount;                // Общее число машин
} Judge;

// Инициализировать судью
void InitJudge(Judge* judge, Car* cars, sem_t** carSemaphores, int carCount, Cell** map, const JudgeRules* rules);

// Проверить, закончилась ли гонка
bool IsRaceOver(const Judge* judge);

// Вывести таблицу лидеров на экран
void DrawLeaderBoard(Judge* judge, int fd);

int CountCarsInRace(const Judge* judge);

#endif
