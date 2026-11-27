#ifndef VIEWER_H
#define VIEWER_H

#include <netinet/in.h>
#include <signal.h>

#include "viewer_protocol.h"

// Адрес сервера из командной строки.
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
} ViewerClientSettings;

// Локальные копии карты и последнего снимка гонки.
typedef struct {
    int socket;
    ViewerInitMessage init;               // Карта и настройки, полученные при подключении
    RaceSnapshot snapshot;                // Последний снимок от сервера
    Judge judge;                          // Локально отсортированная таблица
    bool isTerminal;                      // stdout — терминал; разрешены цвета и перерисовка
    volatile sig_atomic_t isInterrupted;  // Флаг обработчика SIGINT/SIGTERM
} ViewerClient;

// Подключиться, зарегистрироваться и получить карту; 0 — успех.
int InitViewerClient(ViewerClient* client, const ViewerClientSettings* settings);

// Получать и отображать снимки до конца гонки или SIGINT; 0 — успех.
int RunViewerClient(ViewerClient* client);

// Закрыть соединение и восстановить курсор терминала.
void DestroyViewerClient(ViewerClient* client);

#endif
