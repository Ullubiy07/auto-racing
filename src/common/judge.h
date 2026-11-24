#ifndef JUDGE_H
#define JUDGE_H

#include <stdbool.h>

#include "common.h"

// Правила, которыми руководствуется судья
typedef struct {
    int maxLaps;  // Кругов для победы
} JudgeRules;

// Снимок машины и её положения на трассе, без привязки к карте.
typedef struct {
    Car car;           // Копия состояния машины
    int lane;          // Полоса; -1 до расстановки
    int distToFinish;  // Расстояние до финиша в клетках; -1, если неизвестно
} CarStanding;

// Таблица лидеров по последнему переданному снимку гонки.
typedef struct {
    const JudgeRules* rules;  // Заимствованный указатель на правила

    CarStanding leaderBoard[MAX_CARS];  // Порядок мест; индекс уже не равен ID
    int carCount;                       // Число записей в таблице
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
