#ifndef TRACK_H
#define TRACK_H

#include "cell.h"
#include "constants.h"

// Стартовая решётка и направление движения.
typedef struct {
    Point pos;         // Координаты точки старта (x, y)
    Direction dir;     // Направление движения (dx, dy)
    int laneCount;     // Количество стартовых полос (дорожек)
    bool isClockwise;  // Движение по часовой стрелке
} Grid;

// Карта трассы и состояние инициализации клеток.
typedef struct {
    Cell map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];  // Карта
    Grid grid;                                // Стартовая решетка
    int height;                               // Высота карты
    int width;                                // Ширина карты
    int initCellCount;                        // Количество клеток с инициализированным мьютексом
} Track;

// Загрузить трассу; 0 — успех. Перед повторной загрузкой вызвать DestroyTrack.
int LoadTrack(Track* track, const char* fileName);

// Уничтожить мьютексы клеток и обнулить их счётчик.
void DestroyTrack(Track* track);

#endif
