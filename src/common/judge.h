#ifndef JUDGE_H
#define JUDGE_H

#include <stdbool.h>

#include "common.h"

// Правила, которыми руководствуется судья
typedef struct {
    int maxLaps;  // Всего кругов
} JudgeRules;

// Снимок машины и её положения на трассе, без привязки к карте.
typedef struct {
    Car car;
    int lane;
    int distToFinish;
} CarStanding;

// Таблица лидеров по последнему переданному снимку гонки.
typedef struct {
    const JudgeRules* rules;

    CarStanding leaderBoard[MAX_CARS];
    int carCount;  // Общее число машин
} Judge;

// Создать пустую таблицу; правила должны жить дольше судьи.
void InitJudge(Judge* judge, const JudgeRules* rules);

// Скопировать и отсортировать carCount снимков (от 0 до MAX_CARS).
void UpdateLeaderBoard(Judge* judge, const CarStanding* standings, int carCount);

// Проверить финиш машины или сход всех участников.
bool IsRaceOver(const Judge* judge);

// Вывести текущую таблицу лидеров в дескриптор fd.
void DrawLeaderBoard(const Judge* judge, int fd);

// Число машин, не сошедших с дистанции.
int CountCarsInRace(const Judge* judge);

#endif
