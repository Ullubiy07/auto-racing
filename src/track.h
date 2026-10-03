#ifndef TRACK_H
#define TRACK_H

#include <stdbool.h>

enum {
	MAX_MAP_HEIGHT = 30,
	MAX_MAP_WIDTH  = 50
};

// Перечисление, описывающее тип клеток схемы трассы
typedef enum {
	LAYOUT_EMPTY,           // Фон, пустота, исключительно для отрисовки
	LAYOUT_UNKNOWN,         // Неизвестная клетка

	// Стены
	LAYOUT_WALL_HOR,        // Вертикальная
	LAYOUT_WALL_VERT,       // Горизонтальная
	LAYOUT_WALL_TOP_LEFT,   // Верхний левый уголок
	LAYOUT_WALL_TOP_RIGHT,  // Верхний правый уголок
	LAYOUT_WALL_BOT_LEFT,   // Нижний левый уголок
	LAYOUT_WALL_BOT_RIGHT,  // Нижний правый уголок
	
	// Дороги
	LAYOUT_ROAD,            // Дорога
	LAYOUT_START_RIGHT,     // Старт вправо
	LAYOUT_START_LEFT,      // Старт влево
	LAYOUT_START_UP,        // Старт вверх
	LAYOUT_START_DOWN,      // Старт вниз
	LAYOUT_TURN_RIGHT,      // Поворот вправо
	LAYOUT_TURN_LEFT,       // Поворот влево
	LAYOUT_TURN_UP,         // Поворот вверх
	LAYOUT_TURN_DOWN        // Поворот вниз
} LayoutType;

bool IsCellTurn(LayoutType type);

bool IsCellStart(LayoutType type);

// Структура, описывающая информацию о клетке
typedef struct {
	LayoutType type;  // Тип клетки 
	char inputSymbol;  // Символ, обрабатываемый на входе
	const char* outputSymbol;  // Символ для отображения
} CellInfo;

// Получить информацию о клетке
const CellInfo* GetCellInfo(LayoutType type);

typedef struct {
	int x;
	int y;
} Point;

// Получить направление клетки
Point CellDirection(LayoutType type);

// Структура, описывающая стартовую решетку
typedef struct {
	Point pos;      // Координаты точки старта (x, y)
	Point dir;      // Направление движения (dx, dy)
	int laneCount;  // Количество стартовых полос (дорожек)
} Grid;

typedef struct {
	LayoutType type;
	bool clear;
} Cell;

// Структура, описывающая гоночную трассу
typedef struct {
	Cell map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];  // Карта
	Grid grid;   // Стартовая решетка
	int height;  // Высота карты
	int width;   // Ширина карты
} Track;

// Загрузить трассу из файла
void LoadTrack(const char* fileName, Track* track);

#endif
