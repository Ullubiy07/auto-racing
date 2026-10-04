#ifndef CELL_H
#define CELL_H

#include <stdbool.h>

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

bool IsCellDriveable(CellType type);

CellType GetCellType(char inputSymbol);

typedef struct {
	int dx;
	int dy;
} Direction;

// Получить направление клетки
Direction GetCellDirection(CellType type);

// Структура, описывающая информацию о клетке
typedef struct {
	CellType type;          // Тип клетки 
	const char* inSymbols;  // Символы, обрабатываемый на входе
	const char* outSymbol;  // Символ для отображения
} CellInfo;

// Получить информацию о клетке
const CellInfo* GetCellInfo(CellType type);

typedef enum {
	ENTITY_CAR,
	ENTITY_NONE
} EntityType;

typedef struct {
	EntityType type;
	void* data;
} Entity;

typedef struct {
	CellType type;
	Entity entity;
	int distToFinish;
} Cell;

void ClearCell(Cell* cell);

void SetCellEntity(Cell* cell, EntityType type, void* data);

bool IsCellFree(const Cell* cell);

#endif
