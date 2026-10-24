#ifndef JUDJE_H
#define JUDJE_H

#include <stdbool.h>

#include "car.h"

enum {
    MAX_CARS = 26,
    MAX_TEAMS = 7,
};

// Правила, которыми руководствуется судья
typedef struct {
    int maxRounds;  // Предельное число раундов
    int maxLaps;    // Всего кругов
} JudgeRules;

// Судья, отслеживающий ход гонки и таблицу лидеров
typedef struct {
    const JudgeRules* rules;

    Car* leaderBoard[MAX_CARS];  // Таблица лидеров
    int carCount;                // Общее число машин
    int activeCarCount;          // Количество активных машин (не сошедших)
    int carsInCurrentRound;      // Количество активных машин на начало текущего раунда
} Judge;

// Инициализировать судью
void InitJudge(Judge* judge, Car* cars, int carCount, const JudgeRules* rules);

// Проверить, закончилась ли гонка
bool IsRaceOver(const Judge* judge);

// Вывести таблицу лидеров на экран
void DrawLeaderBoard(Judge* judge, int fd);

// Объявить победителя гонки
void AnnounceWinner(const Judge* judge, int fd);

#endif
