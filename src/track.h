#ifndef TRACK_H
#define TRACK_H

enum {
	MAX_MAP_HEIGHT = 30,
	MAX_MAP_WIDTH  = 50
};

// Перечисление, описывающее тип клеток карты
typedef enum {
	CELL_WALL,         // Стена
	CELL_ROAD,         // Дорога
	CELL_START_RIGHT,  // Старт вправо
	CELL_START_LEFT,   // Старт влево
	CELL_START_UP,     // Старт вверх
	CELL_START_DOWN,   // Старт вниз
	CELL_TURN_RIGHT,   // Поворот вправо
	CELL_TURN_LEFT,    // Поворот влево
	CELL_TURN_UP,      // Поворот вверх
	CELL_TURN_DOWN,    // Поворот вниз
	CELL_UNKNOWN       // Неизвестно
} CellType;

typedef struct {
	int x;
	int y;
} Point;

// Структура, описывающая стартовую дорожку (внешнюю по правилам)
typedef struct {
	Point pos;  // Координаты точки старта (x, y)
	Point dir;  // Направление движения (dx, dy)
} StartLane;

// Структура, описывающая гоночную трассу
typedef struct {
	CellType map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];  // Карта
	StartLane start;  // Стартовая дорожка
	int height;  // Высота карты
	int width;   // Ширина карты
} Track;

// Загрузить трассу из файла
void LoadTrack(const char* fileName, Track* track);

// Получить тип клетки по ее символу
CellType GetCellType(char symbol);

// Получить символ клетки по ее типу
char GetCellSymbol(CellType type);

#endif
