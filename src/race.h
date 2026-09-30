#ifndef RACE_H
#define RACE_H

#include "car.h"
#include "track.h"

enum {
	MAX_CARS = 8
};

// Структура, описывающая гонку
typedef struct {
	Track track;  // Гоночная трасса (статическая)
	Car cars[MAX_CARS];  // Машины в порядке лидирования
} Race;

// Вывод текущего состояния гонки на дисплей
void DrawRace(Race* race); 

void StartRace(Race* race);

#endif
