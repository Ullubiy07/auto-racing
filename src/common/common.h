#ifndef COMMON_H
#define COMMON_H

#include "cell.h"
#include "constants.h"

#include <pthread.h>
#include <stddef.h>

// Для блокирующих сокетов: 0 — весь буфер передан, -1 — ошибка с причиной в errno.
int SendAll(int socket, const void* data, size_t size);

// 0 — весь буфер получен, -1 — ошибка; EOF до конца буфера даёт errno = ECONNRESET.
int RecvAll(int socket, void* data, size_t size);

// Название и цвет команды.
typedef struct Team {
    char name[MAX_TEAM_NAME_SIZE];
    int color;  // ANSI-код цвета, 31–37
} Team;

// Машина, участвующая в гонке
typedef struct Car {
    int id;  // Назначается сервером
    char driverName[MAX_DRIVER_NAME_SIZE];
    Team team;            // Команда, в которой состоит машина
    Point pos;            // Местоположение (x, y)
    int laps;             // Число пройденных кругов
    Point prevPos;        // Предыдущая клетка; {-1, -1} до первого хода
    bool hasPassedStart;  // Первое пересечение старта уже было; следующие дают круг
    bool isOut;           // Сошла ли машина с дистанции
    bool isPaused;        // Участие приостановлено командой control
} Car;

// Заполнить данные машины; стартовая позиция задаётся отдельно.
int InitCar(Car* car, int id, const char* driverName, Team team);

// Направление хода относительно текущей клетки.
typedef enum { MOVE_LEFT, MOVE_RIGHT, MOVE_FORWARD, MOVE_NONE } MoveType;

// Выбрать соседнюю позицию; MOVE_NONE оставляет текущую.
Point GetNextPosition(Point current, const Road* road, MoveType type);

// Проверить правила перемещения без проверки занятости клетки.
bool IsMoveAllowed(const Car* car, CellType currentType, const Road* road, Point next);

// Данные клетки для обмена без серверного мьютекса.
typedef struct {
    int x;
    int y;
    int color;  // ANSI-код цвета клетки
    CellType type;
    Entity entity;
    Road road;
} CellData;

// Копирует состояние, самостоятельно захватывая мьютекс клетки.
CellData MakeCellData(Cell* cell);

// Первое сообщение после connect: роль клиента; затем регистрация выбранной роли.
typedef enum { CLIENT_CAR, CLIENT_VIEWER, CLIENT_CONTROL } ClientType;

// Соединение клиента и его поток обработки на сервере.
typedef struct {
    int socket;  // Сокет принятого клиента
    pthread_t thread;
    ClientType type;
} ClientConnection;

#endif
