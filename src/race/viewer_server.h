#ifndef VIEWER_SERVER_H
#define VIEWER_SERVER_H

#include <pthread.h>

struct Race;
struct ViewerConnection;

// Список наблюдателей; изменяется только основным потоком сервера.
typedef struct {
    struct ViewerConnection* connections;  // Голова списка соединений
    pthread_cond_t condition;              // Будит отправителей при смене состояния гонки
} ViewerServer;

// Создать условную переменную; 0 — успех.
int InitViewerServer(ViewerServer* viewers);

// Принять или отклонить наблюдателя; принимает владение сокетом.
void RegisterViewer(struct Race* race, int socket);

// Собрать завершившиеся потоки и освободить их соединения.
void CleanupViewerConnections(ViewerServer* viewers);

// Дождаться отправки итоговых снимков и освободить все соединения.
void StopViewerServer(struct Race* race);

#endif
