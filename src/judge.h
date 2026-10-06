#ifndef JUDJE_H
#define JUDJE_H

#include <stdbool.h>

#include "car.h"

enum {
	MAX_CARS = 26,
	MAX_TEAMS = 6
};

// Структура, описывающая правила, которыми руководствуется судья
typedef struct {
	int maxRounds;  // Предельное число раундов
	int lapsTotal;  // Всего кругов
} JudgeRules;

// Структура, описывающая судью, который следит за ходом гонки
typedef struct {
	const JudgeRules* rules;
	int roundsPlayed;  // Количество сыгранных раундов
	int turn;          // Индекс текущего хода
	
	Car* leaderBoard[MAX_CARS];  // Доска лидеров
	int carCount;                // Общее число машин
	int activeCarCount;          // Количество активных машин (не сошедших)
	int carsInCurrentRound;      // Количество активных машин на начало текущего раунда
} Judge;

// Инициализировать судью
void InitJudge(Judge* judje, Car* cars, int carCount, const JudgeRules* rules);

// Закончилась ли гонка
bool IsRaceOver(const Judge* judje);

// Закончился ли раунд
bool IsRoundOver(const Judge* judje);

// Начать раунд
void StartRound(Judge* judje);

// Закончить раунд
void EndRound(Judge* judje);

// Получить машину текущего хода
Car* GetCurrentCar(const Judge* judje);

// Зафиксировать следующий ход
void NextTurn(Judge* judje);

// Зарегистрировать нулевой ход
void RegisterZeroMove(Judge* judje);

// Вывести доску лидеров на дисплей
void DrawLeaderBoard(const Judge* judje);

// Объявить победителя гонки, выведя на дисплей
void AnnounceWinner(const Judge* judje);

#endif
