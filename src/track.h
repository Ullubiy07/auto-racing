#ifndef TRACK_H
#define TRACK_H

#include <stdbool.h>

enum {
	MAX_MAP_HEIGHT = 30,
	MAX_MAP_WIDTH  = 50
};

// Перечисление, описывающее тип клеток карты
typedef enum {
	CELL_EMPTY,           // Фон, пустота, исключительно для отрисовки
	CELL_UNKNOWN,         // Неизвестная клетка

	// Стены
	CELL_WALL_HOR,        // Вертикальная
	CELL_WALL_VERT,       // Горизонтальная
	CELL_WALL_TOP_LEFT,   // Верхний левый уголок
	CELL_WALL_TOP_RIGHT,  // Верхний правый уголок
	CELL_WALL_BOT_LEFT,   // Нижний левый уголок
	CELL_WALL_BOT_RIGHT,  // Нижний правый уголок
	
	// Дороги
	CELL_ROAD,            // Дорога
	CELL_START_RIGHT,     // Старт вправо
	CELL_START_LEFT,      // Старт влево
	CELL_START_UP,        // Старт вверх
	CELL_START_DOWN,      // Старт вниз
	CELL_TURN_RIGHT,      // Поворот вправо
	CELL_TURN_LEFT,       // Поворот влево
	CELL_TURN_UP,         // Поворот вверх
	CELL_TURN_DOWN        // Поворот вниз
} CellType;

// Является ли клетка поворотом
bool IsCellTurn(CellType type);

// Является ли клетка стартовой
bool IsCellStart(CellType type);

// Можно ли по клетке ездить
bool IsCellDriveable(CellType type);

// Структура, описывающая информацию о клетке
typedef struct {
	CellType type;  // Тип клетки 
	char inputSymbol;  // Символ, обрабатываемый на входе
	const char* outputSymbol;  // Символ для отображения
} CellInfo;

// Получить информацию о клетке
const CellInfo* GetCellInfo(CellType type);

typedef struct {
	int x;
	int y;
} Point;

// Получить направление клетки
Point CellDirection(CellType type);

// Структура, описывающая стартовую решетку
typedef struct {
	Point pos;      // Координаты точки старта (x, y)
	Point dir;      // Направление движения (dx, dy)
	int laneCount;  // Количество стартовых полос (дорожек)
} Grid;

// Структура, описывающая гоночную трассу (входные данные)
typedef struct {
	CellType map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];  // Карта
	Grid grid;   // Стартовая решетка
	int height;  // Высота карты
	int width;   // Ширина карты
} Track;

// Загрузить трассу из файла
void LoadTrack(const char* fileName, Track* track);

#endif
