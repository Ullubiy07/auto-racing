#ifndef CELL_H
#define CELL_H

#include <pthread.h>
#include <stdbool.h>

// Тип клетки на схеме трассы.
typedef enum {
    LAYOUT_EMPTY,    // Пустая клетка
    LAYOUT_UNKNOWN,  // Неизвестная клетка

    // Стены
    LAYOUT_WALL_HOR,        // Горизонтальная
    LAYOUT_WALL_VERT,       // Вертикальная
    LAYOUT_WALL_TOP_LEFT,   // Верхний левый уголок
    LAYOUT_WALL_TOP_RIGHT,  // Верхний правый уголок
    LAYOUT_WALL_BOT_LEFT,   // Нижний левый уголок
    LAYOUT_WALL_BOT_RIGHT,  // Нижний правый уголок

    // Дороги
    LAYOUT_ROAD,         // Обычная прямая дорога
    LAYOUT_START_RIGHT,  // Старт вправо
    LAYOUT_START_LEFT,   // Старт влево
    LAYOUT_START_UP,     // Старт вверх
    LAYOUT_START_DOWN,   // Старт вниз
    LAYOUT_TURN_RIGHT,   // Поворот вправо
    LAYOUT_TURN_LEFT,    // Поворот влево
    LAYOUT_TURN_UP,      // Поворот вверх
    LAYOUT_TURN_DOWN     // Поворот вниз
} CellType;

// Является ли клетка поворотом.
bool IsCellTurn(CellType type);

// Принадлежит ли клетка стартовой линии.
bool IsCellStart(CellType type);

// Разрешено ли движение по клетке.
bool IsCellDriveable(CellType type);

// Тип по символу карты; LAYOUT_UNKNOWN для неизвестного символа.
CellType GetCellType(char inputSymbol);

// Координаты клетки.
typedef struct {
    int x;
    int y;
} Point;

// Вектор движения.
typedef struct {
    int dx;
    int dy;
} Direction;

// Получить направление клетки
Direction GetCellDirection(CellType type);

// Символы ввода и отображения клетки.
typedef struct {
    CellType type;          // Тип клетки
    const char* inSymbols;  // Допустимые входные символы
    const char* outSymbol;  // Символ для отображения
} CellInfo;

// Получить информацию о клетке
const CellInfo* GetCellInfo(CellType type);

// Тип содержимого клетки.
typedef enum { ENTITY_CAR, ENTITY_NONE } EntityType;

// Содержимое клетки; id = -1 для пустой клетки.
typedef struct {
    EntityType type;
    int id;  // ID машины; -1 для ENTITY_NONE
} Entity;

// Связи дороги; отсутствующий сосед обозначается {-1, -1}.
typedef struct {
    int lane;          // Номер полосы
    int distToFinish;  // Расстояние до финиша в клетках; -1, если нет пути
    Point forward;     // Сосед по ходу движения
    Point back;        // Сосед против хода движения
    Point left;        // Сосед слева по ходу движения
    Point right;       // Сосед справа по ходу движения
} Road;

// Серверная клетка с мьютексом для доступа к содержимому.
typedef struct cell_t {
    int x;
    int y;
    int color;  // ANSI-код цвета клетки
    CellType type;
    Entity entity;
    Road road;
    pthread_mutex_t mutex;  // Защищает entity
} Cell;

// Заполнить клетку и создать мьютекс; 0 — успех.
int InitCell(Cell* cell, int x, int y, CellType type);

// Уничтожить мьютекс клетки.
void DestroyCell(Cell* cell);

// Занять пустую клетку; true при успехе.
bool TryTakeCell(Cell* cell, Entity e);

// Освободить клетку.
void LeaveCell(Cell* cell);

// Получить копию содержимого под мьютексом.
Entity CellEntity(Cell* cell);

#endif
