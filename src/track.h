#ifndef TRACK_H
#define TRACK_H

#include "entity.h"

enum {
	MAX_MAP_HEIGHT = 30,
	MAX_MAP_WIDTH  = 50
};

// Структура, описывающая гоночную трассу
typedef struct {
	Entity map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH]; // Карта трассы
	int height; // Высота карты
	int width;  // Ширина карты
} RaceTrack;

#endif
