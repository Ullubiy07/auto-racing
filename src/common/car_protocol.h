#ifndef CAR_PROTOCOL_H
#define CAR_PROTOCOL_H

#include "common.h"

// Задержки между ходами (мс) и лимит кругов.
typedef struct {
    int minSleepTimeMs;
    int maxSleepTimeMs;
    int maxLaps;  // Кругов для победы
} CarSettings;

// Данные машины для регистрации на сервере.
typedef struct {
    char driverName[MAX_DRIVER_NAME_SIZE];
    Team team;
    int resumeId;  // ID сохранённой машины; -1 для новой
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
    Car car;  // Полное состояние, включая позицию и пройденные круги
} CarInitMessage;

// Запрос хода к соседней клетке.
typedef struct {
    Point pos;  // Желаемая следующая клетка
} CarMoveRequest;

// Результат хода или уведомление об окончании гонки.
typedef struct {
    bool allowed;  // Запрошенный ход выполнен
    Car car;       // Состояние машины после обработки запроса сервером

    bool raceOver;  // Гонка завершена для всех машин
    int winnerId;   // -1, если победитель пока не определён
} CarMoveResponse;

// Заголовок сообщения сервера машине; после него идут данные указанного типа.
typedef enum {
    RACE_START,
    RACE_STOP,
    RACE_REGISTRATION,  // CarRegistrationResponse
    RACE_INIT,          // CarInitMessage
    RACE_UPDATE,        // CarMoveResponse
    RACE_DISCONNECT     // Временное отключение с сохранением состояния
} RaceCommand;

// Отправить заголовок и данные; size = 0 для команд без данных.
// Сообщения одному клиенту отправляет только один поток одновременно.
int SendRaceMessage(int socket, RaceCommand command, const void* data, size_t size);

#endif
