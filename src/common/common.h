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
    int color;  // Цвет команды
} Team;

// Машина, участвующая в гонке
typedef struct Car {
    int id;  // Назначается сервером
    char driverName[MAX_DRIVER_NAME_SIZE];
    Team team;            // Команда, в которой состоит машина
    Point pos;            // Местоположение (x, y)
    int laps;             // Число пройденных кругов
    Point prevPos;        // Предыдущее местоположение
    bool hasPassedStart;  // Прошла ли машина линию старта
    bool isOut;           // Сошла ли машина с дистанции
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
    int color;
    CellType type;
    Entity entity;
    Road road;
} CellData;

// Копирует состояние, самостоятельно захватывая мьютекс клетки.
CellData MakeCellData(Cell* cell);

// Задержки между ходами (мс) и лимит кругов.
typedef struct {
    int minSleepTimeMs;
    int maxSleepTimeMs;
    int maxLaps;
} CarSettings;

// Данные машины для регистрации на сервере.
typedef struct {
    char driverName[MAX_DRIVER_NAME_SIZE];
    Team team;
} CarRegistrationMessage;

// Результат регистрации и назначенный ID.
typedef struct {
    bool accepted;
    int id;  // Назначается сервером; -1 при отказе
} CarRegistrationResponse;

// Карта, настройки и стартовая позиция от сервера.
typedef struct {
    int mapWidth;
    int mapHeight;
    CellData map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];  // Клетки map[y][x] в пределах height и width
    CarSettings settings;
    Point pos;  // Стартовая позиция машины
} CarInitMessage;

// Запрос хода к соседней клетке.
typedef struct {
    Point pos;
} CarMoveRequest;

// Результат хода или уведомление об окончании гонки.
typedef struct {
    bool allowed;
    Car car;  // Состояние машины после обработки запроса сервером

    bool raceOver;
    int winnerId;  // -1, если победитель пока не определён
} CarMoveResponse;

// Роль подключённого клиента.
typedef enum { CLIENT_CAR, CLIENT_VIEWER } ClientType;

// Соединение клиента и его поток обработки на сервере.
typedef struct {
    int socket;
    pthread_t thread;
    ClientType type;
} ClientConnection;

// Команды управления гонкой.
typedef enum { RACE_START, RACE_STOP } RaceCommand;

#endif
