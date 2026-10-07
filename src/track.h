#ifndef TRACK_H
#define TRACK_H

#include "cell.h"

enum {
    MAX_MAP_HEIGHT = 40,
    MAX_MAP_WIDTH = 50,
};

// Структура, описывающая стартовую решетку
typedef struct {
    Point pos;     // Координаты точки старта (x, y)
    Direction dir; // Направление движения (dx, dy)
    int laneCount; // Количество стартовых полос (дорожек)
} Grid;

// Структура, описывающая гоночную трассу
typedef struct {
    Cell map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH]; // Карта
    Grid grid;                               // Стартовая решетка
    int height;                              // Высота карты
    int width;                               // Ширина карты
} Track;

// Загрузить трассу из файла
void LoadTrack(Track* track, const char* fileName);

#endif
