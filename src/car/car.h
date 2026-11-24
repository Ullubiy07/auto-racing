#ifndef CAR_H
#define CAR_H

#include "common.h"

#include <netinet/in.h>
#include <signal.h>

// Параметры подключения и данные водителя из CLI.
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
    char driverName[MAX_DRIVER_NAME_SIZE];
    Team team;
} CarClientSettings;

// Соединение и локальное состояние машины.
typedef struct {
    int socket;
    Car car;
    CellData* cell;  // Указатель на текущую клетку в map

    int mapWidth;
    int mapHeight;
    CellData map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];  // Копия карты; занятость клеток не обновляется
    CarSettings settings;

    bool isOver;                          // Цикл клиента завершён, в том числе при ошибке
    bool failed;                          // Ошибка обмена или некорректный ответ сервера
    int winnerId;                         // -1, пока победитель неизвестен
    volatile sig_atomic_t isInterrupted;  // Флаг обработчика SIGINT
} CarClient;

// Подключиться, зарегистрироваться и получить карту; 0 — успех.
int InitCarClient(CarClient* client, const CarClientSettings* settings);

// Закрыть соединение с сервером.
void DestroyCarClient(CarClient* client);

// Дождаться старта и обмениваться ходами; 0 — завершение без ошибки.
int RunCarClient(CarClient* client);

#endif
