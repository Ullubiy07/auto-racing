#ifndef RACE_H
#define RACE_H

#include "track.h"

// Структура, описывающая автогонку
typedef struct {
	RaceTrack track; // Гоночная трасса
	int lapCount;    // Количество кругов до финиша
	int teamCount;   // Количество команд, участвующих в гонке
} AutoRace;

// Загрузить карту гоночной трассы из файла
void LoadTrack(const char* fileName, RaceTrack* track);

// Вывод текущего состояния гонки на дисплей
void DrawRace(RaceTrack* track);

#endif
