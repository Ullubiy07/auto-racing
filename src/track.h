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
} CellType;

bool IsCellTurn(CellType type);

bool IsCellStart(CellType type);

// Структура, описывающая информацию о клетке
typedef struct {
	CellType type;          // Тип клетки 
	const char* inSymbols;  // Символы, обрабатываемый на входе
	const char* outSymbol;  // Символ для отображения
} CellInfo;

// Получить информацию о клетке
const CellInfo* GetCellInfo(CellType type);

typedef struct {
	int x;
	int y;
} Point;

typedef struct {
	int dx;
	int dy;
} Direction;

// Получить направление клетки
Direction GetCellDirection(CellType type);

// Структура, описывающая стартовую решетку
typedef struct {
	Point pos;         // Координаты точки старта (x, y)
	Direction dir;     // Направление движения (dx, dy)
	int laneCount;     // Количество стартовых полос (дорожек)
	bool isClockwise;  // По часовой ли стрелке движение
} Grid;

typedef struct {
	CellType type;
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
