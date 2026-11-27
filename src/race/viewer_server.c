#include "viewer_server.h"

#include <errno.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "race.h"
#include "viewer_protocol.h"

// Живёт до pthread_join; socket закрывает основной поток после завершения обработчика.
typedef struct ViewerConnection {
    Race* race;
    int socket;
    pthread_t thread;               // Поток отправки этому наблюдателю
    atomic_bool isConnected;        // false: обработчик выходит, можно вызвать join
    struct ViewerConnection* next;  // Следующее соединение в списке
} ViewerConnection;

int InitViewerServer(ViewerServer* viewers) {
    viewers->connections = NULL;

    int error = pthread_cond_init(&viewers->condition, NULL);
    if (error != 0) {
        dprintf(2, "Initialize viewer condition: %s\n", strerror(error));
        return 1;
    }

    return 0;
}

// Вызывается под race->mutex, как и изменение машин обработчиками.
static void MakeRaceSnapshot(const Race* race, RaceSnapshot* snapshot) {
    *snapshot = (RaceSnapshot){.state = race->state, .carCount = race->connectedCarsCount, .winnerId = race->winnerId};

    for (int i = 0; i < snapshot->carCount; ++i) {
        const Car* car = &race->cars[i];
        CarStanding* standing = &snapshot->standings[i];
        standing->car = *car;
        standing->lane = -1;
        standing->distToFinish = -1;

        if (car->pos.x >= 0 && car->pos.x < race->track.width && car->pos.y >= 0 && car->pos.y < race->track.height) {
            const Road* road = &race->track.map[car->pos.y][car->pos.x].road;
            standing->lane = road->lane;
            standing->distToFinish = road->distToFinish;
        }
    }
}

static int SendViewerInitialization(ViewerConnection* viewer) {
    Race* race = viewer->race;
    ViewerInitMessage message = {0};

    pthread_mutex_lock(&race->mutex);
    message.mapWidth = race->track.width;
    message.mapHeight = race->track.height;
    message.expectedCarCount = race->settings->carCount;
    message.rules = race->settings->rules;

    for (int y = 0; y < message.mapHeight; ++y) {
        for (int x = 0; x < message.mapWidth; ++x) {
            message.map[y][x] = MakeCellData(&race->track.map[y][x]);
        }
    }

    pthread_mutex_unlock(&race->mutex);

    return SendAll(viewer->socket, &message, sizeof(message));
}

static struct timespec AddMilliseconds(struct timespec time, int milliseconds) {
    time.tv_sec += milliseconds / 1000;
    time.tv_nsec += (milliseconds % 1000) * 1000000L;

    if (time.tv_nsec >= 1000000000L) {
        ++time.tv_sec;
        time.tv_nsec -= 1000000000L;
    }

    return time;
}

static bool TimeReached(struct timespec now, struct timespec deadline) {
    return now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec);
}

static bool WaitForNextSnapshot(ViewerConnection* viewer) {
    Race* race = viewer->race;
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    struct timespec deadline = AddMilliseconds(now, race->settings->drawDelayMs);

    while (true) {
        // После регистрации viewer ничего не отправляет: POLLIN означает EOF или ошибку протокола.
        struct pollfd connection = {.fd = viewer->socket, .events = POLLIN};

        int ready = poll(&connection, 1, 0);
        if (ready > 0 || (ready == -1 && errno != EINTR)) {
            return false;
        }

        clock_gettime(CLOCK_REALTIME, &now);
        if (TimeReached(now, deadline)) {
            return true;
        }

        // Проверяем отключение раз в 100 мс даже при большом интервале кадров.
        struct timespec checkDeadline = AddMilliseconds(now, 100);
        if (TimeReached(checkDeadline, deadline)) {
            checkDeadline = deadline;
        }

        pthread_mutex_lock(&race->mutex);
        if (race->state >= RACE_FINISHED) {
            pthread_mutex_unlock(&race->mutex);
            return true;
        }

        int error = pthread_cond_timedwait(&race->viewers.condition, &race->mutex, &checkDeadline);
        pthread_mutex_unlock(&race->mutex);

        if (error == 0) {
            return true;
        }

        if (error != ETIMEDOUT) {
            dprintf(2, "Wait for viewer snapshot: %s\n", strerror(error));
            return false;
        }
    }
}

static void* ViewerHandler(void* arg) {
    ViewerConnection* viewer = arg;
    Race* race = viewer->race;

    if (SendViewerInitialization(viewer) == -1) {
        goto finish;
    }

    while (true) {
        RaceSnapshot snapshot;

        pthread_mutex_lock(&race->mutex);
        MakeRaceSnapshot(race, &snapshot);
        pthread_mutex_unlock(&race->mutex);

        // Сетевой ввод-вывод всегда за пределами мьютекса гонки.
        if (SendAll(viewer->socket, &snapshot, sizeof(snapshot)) == -1) {
            break;
        }

        if (snapshot.state >= RACE_FINISHED) {
            break;
        }

        if (!WaitForNextSnapshot(viewer)) {
            break;
        }
    }

finish:
    shutdown(viewer->socket, SHUT_RDWR);
    viewer->isConnected = false;
    dprintf(1, "Viewer disconnected\n");
    return NULL;
}

static void CloseViewerConnection(ViewerConnection* viewer) {
    int error = pthread_join(viewer->thread, NULL);
    if (error != 0) {
        dprintf(2, "Join viewer handler: %s\n", strerror(error));
    }

    close(viewer->socket);
    free(viewer);
}

void CleanupViewerConnections(ViewerServer* viewers) {
    ViewerConnection** current = &viewers->connections;

    while (*current != NULL) {
        ViewerConnection* viewer = *current;
        if (viewer->isConnected) {
            current = &viewer->next;
            continue;
        }

        *current = viewer->next;
        CloseViewerConnection(viewer);
    }
}

void RegisterViewer(Race* race, int socket) {
    ViewerConnection* viewer = NULL;

    if (!race->isInterrupted && !race->isOver) {
        viewer = malloc(sizeof(*viewer));
        if (viewer == NULL) {
            perror("allocate viewer connection");
        }
    }

    ViewerRegistrationResponse response = {.accepted = viewer != NULL};

    if (SendAll(socket, &response, sizeof(response)) == -1) {
        free(viewer);
        close(socket);
        return;
    }

    if (!response.accepted) {
        close(socket);
        return;
    }

    viewer->race = race;
    viewer->socket = socket;
    viewer->isConnected = true;
    viewer->next = race->viewers.connections;

    int error = pthread_create(&viewer->thread, NULL, ViewerHandler, viewer);
    if (error != 0) {
        dprintf(2, "Create viewer handler: %s\n", strerror(error));
        close(socket);
        free(viewer);
        return;
    }

    race->viewers.connections = viewer;
    dprintf(1, "Viewer connected\n");
}

void StopViewerServer(Race* race) {
    pthread_mutex_lock(&race->mutex);
    pthread_cond_broadcast(&race->viewers.condition);
    pthread_mutex_unlock(&race->mutex);

    // SO_SNDTIMEO ограничивает ожидание, даже если viewer перестал читать.
    while (race->viewers.connections != NULL) {
        ViewerConnection* viewer = race->viewers.connections;
        race->viewers.connections = viewer->next;
        CloseViewerConnection(viewer);
    }
}
