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
    CellData* cell;  // Текущая клетка машины

    int mapWidth;
    int mapHeight;
    CellData map[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];  // Карта клеток
    CarSettings settings;

    bool isOver;
    bool failed;
    int winnerId;  // -1, пока победитель неизвестен
    volatile sig_atomic_t isInterrupted;
} CarClient;

// Подключиться, зарегистрироваться и получить карту; 0 — успех.
int InitCarClient(CarClient* client, const CarClientSettings* settings);

// Закрыть соединение с сервером.
void DestroyCarClient(CarClient* client);

// Дождаться старта и обмениваться ходами; 0 — завершение без ошибки.
int RunCarClient(CarClient* client);

#endif
