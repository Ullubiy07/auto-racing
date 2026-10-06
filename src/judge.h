#ifndef JUDJE_H
#define JUDJE_H

#include <stdbool.h>

#include "car.h"

enum {
	MAX_CARS = 26,
	MAX_TEAMS = 6
};

// Правила, которыми руководствуется судья
typedef struct {
	int maxRounds;  // Предельное число раундов
	int lapsTotal;  // Всего кругов
} JudgeRules;

// Судья, отслеживающий ход гонки и таблицу лидеров
typedef struct {
	const JudgeRules* rules;
	int roundsPlayed;  			 // Количество сыгранных раундов
	int turn;          			 // Индекс текущего хода
	
	Car* leaderBoard[MAX_CARS];  // Таблица лидеров
	int carCount;                // Общее число машин
	int activeCarCount;          // Количество активных машин (не сошедших)
	int carsInCurrentRound;      // Количество активных машин на начало текущего раунда
} Judge;

// Инициализировать судью
void InitJudge(Judge* judje, Car* cars, int carCount, const JudgeRules* rules);

// Проверить, закончилась ли гонка
bool IsRaceOver(const Judge* judje);

// Проверить, закончился ли раунд
bool IsRoundOver(const Judge* judje);

// Начать новый раунд
void StartRound(Judge* judje);

// Закончить текущий раунд
void EndRound(Judge* judje);

// Получить машину текущего хода
Car* GetCurrentCar(const Judge* judje);

// Перейти к следующему ходу
void NextTurn(Judge* judje);

// Зарегистрировать нулевой ход (сход машины с дистанции)
void RegisterZeroMove(Judge* judje);

// Вывести таблицу лидеров на экран
void DrawLeaderBoard(const Judge* judje);

// Объявить победителя гонки
void AnnounceWinner(const Judge* judje);

#endif
