#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "race.h"
#include "car_protocol.h"
#include "control_protocol.h"

static void StopRace(Race* race);

static void ResetStats(Race* race) {
    race->stats.duration = 0.0;
    race->stats.status = "Completed successfully";
    race->stats.winner = NULL;
}

int InitRace(Race* race, const RaceSettings* settings) {
    race->socket = -1;
    race->startedHandlersCount = 0;
    race->connectedCarsCount = 0;
    race->settings = settings;

    race->isInterrupted = false;
    race->isOver = false;
    race->activeHandlersCount = 0;
    race->isStarted = false;
    race->state = RACE_WAITING;
    race->winnerId = -1;

    InitJudge(&race->judge, &settings->rules);
    ResetStats(race);

    memset(race->cars, 0, sizeof(race->cars));
    memset(race->carDisabled, 0, sizeof(race->carDisabled));
    memset(race->waitingForReconnect, 0, sizeof(race->waitingForReconnect));

    for (int i = 0; i < MAX_CARS; ++i) {
        race->clients[i] = (ClientConnection){.socket = -1, .type = CLIENT_CAR};
    }

    if (settings->carCount <= 0 || settings->carCount > MAX_CARS || settings->rules.maxLaps <= 0) {
        dprintf(2, "Invalid car count or lap limit\n");
        return 1;
    }

    int error = pthread_mutex_init(&race->mutex, NULL);
    if (error != 0) {
        dprintf(2, "Initialize race mutex: %s\n", strerror(error));
        return 1;
    }

    error = pthread_cond_init(&race->startCondition, NULL);
    if (error != 0) {
        dprintf(2, "Initialize race start condition: %s\n", strerror(error));
        pthread_mutex_destroy(&race->mutex);
        return 1;
    }

    if (InitViewerServer(&race->viewers)) {
        pthread_cond_destroy(&race->startCondition);
        pthread_mutex_destroy(&race->mutex);
        return 1;
    }

    if (LoadTrack(&race->track, settings->mapFile)) {
        pthread_cond_destroy(&race->viewers.condition);
        pthread_cond_destroy(&race->startCondition);
        pthread_mutex_destroy(&race->mutex);
        return 1;
    }

    return 0;
}

void DestroyRace(Race* race) {
    if (race->startedHandlersCount > 0) {
        StopRace(race);
    }

    StopViewerServer(race);

    for (int i = 0; i < race->settings->carCount; ++i) {
        if (race->clients[i].socket >= 0) {
            close(race->clients[i].socket);
            race->clients[i].socket = -1;
        }
    }

    if (race->socket >= 0) {
        close(race->socket);
        race->socket = -1;
    }

    DestroyTrack(&race->track);
    pthread_cond_destroy(&race->viewers.condition);
    pthread_cond_destroy(&race->startCondition);
    pthread_mutex_destroy(&race->mutex);
}

static void ClearScreen() {
    dprintf(1, "\x1b[2J");    // Очистить весь экран
    dprintf(1, "\x1b[0;0f");  // Переместить курсор в левый верхний угол
}

static void UpdateRaceLeaderBoard(Race* race) {
    CarStanding standings[MAX_CARS];

    for (int i = 0; i < race->connectedCarsCount; ++i) {
        const Car* car = &race->cars[i];
        const Road* road = &race->track.map[car->pos.y][car->pos.x].road;

        standings[i] = (CarStanding){.car = *car, .lane = road->lane, .distToFinish = road->distToFinish};
    }

    UpdateLeaderBoard(&race->judge, standings, race->connectedCarsCount);
}

static void DrawRace(Race* race) {
    Track* track = &race->track;
    ClearScreen();

    for (int i = 0; i < track->height; ++i) {
        for (int j = 0; j < track->width; ++j) {
            Cell* cell = &track->map[i][j];
            Entity e = CellEntity(cell);

            if (e.type == ENTITY_CAR) {
                dprintf(1, "\x1b[%d;1m🏎 \x1b[0m", race->cars[e.id].team.color);
            } else {
                dprintf(1, "\x1b[%dm%s\x1b[0m", cell->color, GetCellInfo(cell->type)->outSymbol);
            }
        }

        dprintf(1, "\n");
    }

    if (!race->settings->mapOnly) {
        UpdateRaceLeaderBoard(race);
        DrawLeaderBoard(&race->judge, 1);
    }
}

static void PaintStartLine(Race* race, int color) {
    Track* track = &race->track;

    pthread_mutex_lock(&race->mutex);
    for (int i = 0; i < track->height; ++i) {
        for (int j = 0; j < track->width; ++j) {
            Cell* cell = &track->map[i][j];

            if (IsCellStart(cell->type)) {
                cell->color = color;
            }
        }
    }
    pthread_mutex_unlock(&race->mutex);
}

static void DrawRaceStart(Race* race) {
    if (!race->isInterrupted) {
        PaintStartLine(race, 31);
        DrawRace(race);
        sleep(1);
    }

    if (!race->isInterrupted) {
        PaintStartLine(race, 33);
        DrawRace(race);
        sleep(1);
    }

    if (!race->isInterrupted) {
        PaintStartLine(race, 32);
        DrawRace(race);
    }
}

static int StartRaceServer(Race* race) {
    race->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (race->socket == -1) {
        perror("create TCP socket");
        return 1;
    }

    int reuse = 1;
    if (setsockopt(race->socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        perror("setsockopt SO_REUSEADDR");
        return 1;
    }

    struct sockaddr_in server = {.sin_family = AF_INET, .sin_port = htons(race->settings->port)};

    if (inet_pton(AF_INET, race->settings->ip, &server.sin_addr) != 1) {
        dprintf(2, "Invalid server IPv4 address: %s\n", race->settings->ip);
        return 1;
    }

    if (bind(race->socket, (const struct sockaddr*) &server, sizeof(server)) == -1) {
        perror("start race server");
        return 1;
    }

    if (listen(race->socket, SOMAXCONN) == -1) {
        perror("start race server");
        return 1;
    }

    dprintf(1, "Race server listening on %s:%d; waiting for %d cars\n", race->settings->ip, race->settings->port,
            race->settings->carCount);
    return 0;
}

static bool ValidRegistration(const CarRegistrationMessage* message) {
    if (message->resumeId < -1 || message->resumeId >= MAX_CARS) {
        return false;
    }

    if (!memchr(message->driverName, '\0', sizeof(message->driverName))) {
        return false;
    }

    if (message->driverName[0] == '\0') {
        return false;
    }

    if (!memchr(message->team.name, '\0', sizeof(message->team.name))) {
        return false;
    }

    if (message->team.name[0] == '\0') {
        return false;
    }

    return message->team.color >= 31 && message->team.color <= 37;
}

static int SendCarInitialization(Race* race, int id) {
    CarInitMessage message = {0};

    pthread_mutex_lock(&race->mutex);
    message.mapWidth = race->track.width;
    message.mapHeight = race->track.height;
    message.settings = (CarSettings){.minSleepTimeMs = race->settings->carMinDelayMs,
                                     .maxSleepTimeMs = race->settings->carMaxDelayMs,
                                     .maxLaps = race->settings->rules.maxLaps};
    message.car = race->cars[id];
    message.car.isPaused = false;

    for (int y = 0; y < race->track.height; ++y) {
        for (int x = 0; x < race->track.width; ++x) {
            message.map[y][x] = MakeCellData(&race->track.map[y][x]);
        }
    }
    pthread_mutex_unlock(&race->mutex);

    return SendRaceMessage(race->clients[id].socket, RACE_INIT, &message, sizeof(message));
}

static int RestoreCarConnection(Race* race, int id) {
    int socket = race->clients[id].socket;

    if (SendCarInitialization(race, id) == -1) {
        return 1;
    }

    if (SendRaceMessage(socket, RACE_START, NULL, 0) == -1) {
        return 1;
    }

    pthread_mutex_lock(&race->mutex);
    race->cars[id].isPaused = false;
    Car restoredCar = race->cars[id];
    pthread_cond_broadcast(&race->startCondition);
    pthread_cond_broadcast(&race->viewers.condition);

    // Обработчик должен забрать новый сокет до следующей команды управления.
    while (race->waitingForReconnect[id] && !race->isOver) {
        pthread_cond_wait(&race->startCondition, &race->mutex);
    }
    pthread_mutex_unlock(&race->mutex);

    dprintf(1, "Car %d restored at (%d, %d), laps %d\n", id, restoredCar.pos.x, restoredCar.pos.y, restoredCar.laps);
    return 0;
}

static void RegisterCar(Race* race, int socket) {
    CarRegistrationMessage registration;
    if (RecvAll(socket, &registration, sizeof(registration)) == -1) {
        if (!race->isInterrupted) {
            perror("receive car registration");
        }

        close(socket);
        return;
    }

    bool accepted = ValidRegistration(&registration);
    if (race->isInterrupted || race->isOver) {
        SendRaceMessage(socket, RACE_STOP, NULL, 0);
        shutdown(socket, SHUT_WR);
        close(socket);
        return;
    }

    bool resuming = registration.resumeId >= 0;
    int id = resuming ? registration.resumeId : race->connectedCarsCount;

    pthread_mutex_lock(&race->mutex);
    if (resuming && accepted) {
        if (id >= race->connectedCarsCount || !race->cars[id].isPaused || race->cars[id].isOut) {
            accepted = false;
        } else {
            const Car* car = &race->cars[id];
            accepted = strcmp(car->driverName, registration.driverName) == 0 &&
                       strcmp(car->team.name, registration.team.name) == 0 &&
                       car->team.color == registration.team.color;

            // Старый обработчик сам закрывает сокет перед его заменой.
            while (accepted && !race->waitingForReconnect[id] && !race->isOver && !race->isInterrupted) {
                pthread_cond_wait(&race->startCondition, &race->mutex);
            }

            if (race->clients[id].socket >= 0 || race->isOver) {
                accepted = false;
            }
        }
    } else if (race->connectedCarsCount >= race->settings->carCount) {
        accepted = false;
    }
    pthread_mutex_unlock(&race->mutex);

    if (race->isInterrupted || race->isOver) {
        SendRaceMessage(socket, RACE_STOP, NULL, 0);
        shutdown(socket, SHUT_WR);
        close(socket);
        return;
    }

    CarRegistrationResponse response = {.accepted = accepted, .id = accepted ? id : -1};

    if (SendRaceMessage(socket, RACE_REGISTRATION, &response, sizeof(response)) == -1) {
        close(socket);
        return;
    }

    if (!accepted) {
        dprintf(2, "Car registration rejected\n");
        close(socket);
        return;
    }

    struct timeval timeout = {0};
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        perror("clear registration timeout");
        close(socket);
        return;
    }

    if (resuming) {
        pthread_mutex_lock(&race->mutex);
        race->clients[id].socket = socket;
        bool restore = !race->carDisabled[id];
        pthread_mutex_unlock(&race->mutex);

        dprintf(1, "Car %d reconnected; waiting for restore\n", id);
        if (restore && RestoreCarConnection(race, id)) {
            close(socket);
            race->clients[id].socket = -1;
        }

        return;
    }

    pthread_mutex_lock(&race->mutex);
    InitCar(&race->cars[id], id, registration.driverName, registration.team);
    race->cars[id].pos = (Point){-1, -1};
    race->clients[id].socket = socket;
    ++race->connectedCarsCount;
    pthread_cond_broadcast(&race->viewers.condition);
    pthread_mutex_unlock(&race->mutex);

    dprintf(1, "Car %d registered: %s, team %s (%d/%d)\n", id, registration.driverName, registration.team.name,
            race->connectedCarsCount, race->settings->carCount);
}

static void HandleControlConnection(Race* race, int socket) {
    ControlRequest request;
    if (RecvAll(socket, &request, sizeof(request)) == -1) {
        close(socket);
        return;
    }

    ControlResponse response = {0};
    int id = request.carId;
    bool restore = false;

    pthread_mutex_lock(&race->mutex);
    if (!race->isStarted || race->isOver) {
        snprintf(response.message, sizeof(response.message), "Race is not running");
    } else if (id < 0 || id >= race->connectedCarsCount) {
        snprintf(response.message, sizeof(response.message), "Unknown car ID");
    } else if (race->cars[id].isOut) {
        snprintf(response.message, sizeof(response.message), "Car %d has left the race", id);
    } else if (request.action == CONTROL_DISCONNECT) {
        if (race->cars[id].isPaused) {
            snprintf(response.message, sizeof(response.message), "Car %d is already disconnected", id);
        } else {
            race->carDisabled[id] = true;
            race->cars[id].isPaused = true;
            response.accepted = true;
            snprintf(response.message, sizeof(response.message), "Car %d disconnection requested", id);
        }
    } else if (request.action == CONTROL_RESTORE) {
        if (!race->carDisabled[id]) {
            snprintf(response.message, sizeof(response.message), "Car %d is not disabled", id);
        } else {
            race->carDisabled[id] = false;
            restore = race->waitingForReconnect[id] && race->clients[id].socket >= 0;
            response.accepted = true;
            snprintf(response.message, sizeof(response.message), "Car %d restoration enabled", id);
        }
    } else {
        snprintf(response.message, sizeof(response.message), "Unknown control action");
    }
    pthread_cond_broadcast(&race->viewers.condition);
    pthread_mutex_unlock(&race->mutex);

    if (restore && RestoreCarConnection(race, id)) {
        close(race->clients[id].socket);
        race->clients[id].socket = -1;
        response.accepted = false;
        snprintf(response.message, sizeof(response.message), "Could not restore car %d connection", id);
    }

    SendAll(socket, &response, sizeof(response));
    close(socket);
}

static int AcceptClientConnection(Race* race, int timeoutMs) {
    CleanupViewerConnections(&race->viewers);

    struct pollfd listener = {.fd = race->socket, .events = POLLIN};

    int ready = poll(&listener, 1, timeoutMs);
    if (ready == -1) {
        if (errno == EINTR) {
            return 0;
        }

        perror("wait for client connection");
        return 1;
    }

    if (ready == 0) {
        return 0;
    }

    int socket = accept(race->socket, NULL, NULL);
    if (socket == -1) {
        if (errno == EINTR) {
            return 0;
        }

        perror("accept client connection");
        return 1;
    }

    // Ограничиваем незавершённую регистрацию и отправку медленному клиенту.
    struct timeval receiveTimeout = {.tv_sec = 3};
    struct timeval sendTimeout = {.tv_sec = 1};

    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &receiveTimeout, sizeof(receiveTimeout)) == -1) {
        perror("set registration timeout");
        close(socket);
        return 0;
    }

    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, sizeof(sendTimeout)) == -1) {
        perror("set client send timeout");
        close(socket);
        return 0;
    }

    ClientType type;
    if (RecvAll(socket, &type, sizeof(type)) == -1) {
        close(socket);
        return 0;
    }

    switch (type) {
    case CLIENT_CAR:
        RegisterCar(race, socket);
        break;
    case CLIENT_VIEWER:
        RegisterViewer(race, socket);
        break;
    case CLIENT_CONTROL:
        HandleControlConnection(race, socket);
        break;
    default:
        dprintf(2, "Unknown client type\n");
        close(socket);
        break;
    }

    return 0;
}

static int WaitForCarConnections(Race* race) {
    while (race->connectedCarsCount < race->settings->carCount && !race->isInterrupted) {
        if (AcceptClientConnection(race, 100)) {
            return 1;
        }
    }

    if (race->isInterrupted) {
        return 1;
    }

    return 0;
}

static int PlaceCarsOnGrid(Race* race) {
    pthread_mutex_lock(&race->mutex);
    Point pos = race->track.grid.pos;
    Cell* start = &race->track.map[pos.y][pos.x];
    for (int i = 0; i < race->settings->carCount; ++i) {
        pos = start->road.back;
        if (pos.x < 0 || pos.x >= race->track.width || pos.y < 0 || pos.y >= race->track.height) {
            dprintf(2, "Track capacity exceeded at car %d\n", i);
            pthread_mutex_unlock(&race->mutex);
            return 1;
        }

        start = &race->track.map[pos.y][pos.x];
        Entity entity = {.type = ENTITY_CAR, .id = i};

        if (!TryTakeCell(start, entity)) {
            dprintf(2, "Starting cell is occupied\n");
            pthread_mutex_unlock(&race->mutex);
            return 1;
        }

        race->cars[i].pos = pos;
    }

    UpdateRaceLeaderBoard(race);
    pthread_mutex_unlock(&race->mutex);
    for (int i = 0; i < race->settings->carCount; ++i) {
        if (SendCarInitialization(race, i) == -1) {
            perror("send car initialization");
            return 1;
        }
    }

    return 0;
}

void TerminateRace(Race* race) {
    race->isInterrupted = true;
}

// Вызывается под общим мьютексом: проверка и изменение состояния неделимы.
static bool ApplyCarMove(Race* race, int id, Point pos) {
    Car* car = &race->cars[id];

    if (race->isOver || car->isOut || car->isPaused) {
        return false;
    }

    if (pos.x < 0 || pos.x >= race->track.width) {
        return false;
    }

    if (pos.y < 0 || pos.y >= race->track.height) {
        return false;
    }

    Cell* current = &race->track.map[car->pos.y][car->pos.x];
    if (!IsMoveAllowed(car, current->type, &current->road, pos)) {
        return false;
    }

    Cell* next = &race->track.map[pos.y][pos.x];

    if (!IsCellDriveable(next->type)) {
        return false;
    }

    Entity entity = {.type = ENTITY_CAR, .id = id};

    if (!TryTakeCell(next, entity)) {
        return false;
    }

    if (IsCellStart(next->type) && !IsCellStart(current->type)) {
        if (car->hasPassedStart) {
            ++car->laps;
            dprintf(1, "%s completed lap %d\n", car->driverName, car->laps);
        }

        car->hasPassedStart = true;
    }

    LeaveCell(current);
    car->prevPos = car->pos;
    car->pos = pos;
    if (car->laps >= race->settings->rules.maxLaps) {
        race->winnerId = id;
        race->stats.winner = car->driverName;
        race->isOver = true;
        pthread_cond_broadcast(&race->startCondition);
        dprintf(1, "Winner: %s (car %d)\n", car->driverName, id);
    }

    return true;
}

static CarMoveResponse CarResponse(const Race* race, int id, bool allowed) {
    CarMoveResponse response = {
        .allowed = allowed, .car = race->cars[id], .raceOver = race->isOver, .winnerId = race->winnerId};

    return response;
}

// true — вместо схода обработчик должен ждать восстановления.
static bool MarkCarOut(Race* race, int id) {
    pthread_mutex_lock(&race->mutex);

    if (race->isOver || race->cars[id].isOut) {
        pthread_mutex_unlock(&race->mutex);
        return false;
    }

    if (race->cars[id].isPaused) {
        pthread_mutex_unlock(&race->mutex);
        return true;
    }

    Car* car = &race->cars[id];
    car->isOut = true;

    Cell* cell = &race->track.map[car->pos.y][car->pos.x];
    LeaveCell(cell);

    dprintf(1, "%s disconnected and left the race\n", car->driverName);

    bool allOut = true;

    for (int i = 0; i < race->settings->carCount; ++i) {
        if (!race->cars[i].isOut) {
            allOut = false;
            break;
        }
    }

    if (allOut) {
        race->stats.status = "All cars are out";
        race->isOver = true;
        pthread_cond_broadcast(&race->startCondition);
    }

    pthread_mutex_unlock(&race->mutex);
    return false;
}

static int WaitForCarReconnect(Race* race, int id, int socket) {
    SendRaceMessage(socket, RACE_DISCONNECT, NULL, 0);
    shutdown(socket, SHUT_WR);

    struct pollfd connection = {.fd = socket, .events = POLLIN};
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (poll(&connection, 1, 100) <= 0) {
            continue;
        }

        char discarded[256];
        if (recv(socket, discarded, sizeof(discarded), MSG_DONTWAIT) <= 0) {
            break;
        }
    }

    pthread_mutex_lock(&race->mutex);
    close(socket);
    race->clients[id].socket = -1;
    race->waitingForReconnect[id] = true;
    pthread_cond_broadcast(&race->startCondition);
    dprintf(1, "Car %d disconnected; state saved\n", id);

    while (race->cars[id].isPaused && !race->isOver) {
        pthread_cond_wait(&race->startCondition, &race->mutex);
    }

    int restoredSocket = race->isOver ? -1 : race->clients[id].socket;
    race->waitingForReconnect[id] = false;
    pthread_cond_broadcast(&race->startCondition);
    pthread_mutex_unlock(&race->mutex);
    return restoredSocket;
}

static void* CarHandler(void* arg) {
    CarHandlerArgs* args = arg;
    Race* race = args->race;
    int id = args->carId;
    int socket = race->clients[id].socket;

    // Обработчики ждут, пока организатор запустит гонку.
    pthread_mutex_lock(&race->mutex);
    while (!race->isStarted && !race->isOver) {
        pthread_cond_wait(&race->startCondition, &race->mutex);
    }

    bool started = race->isStarted;
    pthread_mutex_unlock(&race->mutex);

    CarMoveRequest request;
    size_t received = 0;

    while (started && !race->isInterrupted) {
        pthread_mutex_lock(&race->mutex);
        CarMoveResponse response = CarResponse(race, id, false);
        pthread_mutex_unlock(&race->mutex);

        if (response.car.isPaused && !response.raceOver) {
            socket = WaitForCarReconnect(race, id, socket);
            received = 0;
            if (socket < 0) {
                break;
            }

            continue;
        }

        if (response.raceOver) {
            if (!race->isInterrupted) {
                SendRaceMessage(socket, RACE_UPDATE, &response, sizeof(response));
            }
            break;
        }

        struct pollfd connection = {.fd = socket, .events = POLLIN};

        int ready = poll(&connection, 1, 100);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (MarkCarOut(race, id)) {
                continue;
            }
            break;
        }

        if (ready == 0) {
            continue;
        }

        // Частичный запрос не мешает обработчику заметить остановку гонки.
        ssize_t count = recv(socket, (char*) &request + received, sizeof(request) - received, MSG_DONTWAIT);
        if (count == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }

        if (count <= 0) {
            if (MarkCarOut(race, id)) {
                continue;
            }
            break;
        }

        received += (size_t) count;
        if (received < sizeof(request)) {
            continue;
        }

        received = 0;

        // Проверка хода и формирование ответа используют одно состояние гонки.
        pthread_mutex_lock(&race->mutex);

        bool allowed = ApplyCarMove(race, id, request.pos);
        response = CarResponse(race, id, allowed);

        if (!response.raceOver) {
            dprintf(1, "%s: move to (%d, %d) %s\n", race->cars[id].driverName, request.pos.x, request.pos.y,
                    allowed ? "accepted" : "rejected");
        }

        pthread_mutex_unlock(&race->mutex);

        if (race->isInterrupted) {
            break;
        }

        if (SendRaceMessage(socket, RACE_UPDATE, &response, sizeof(response)) == -1) {
            if (MarkCarOut(race, id)) {
                continue;
            }
            break;
        }

        if (response.raceOver) {
            break;
        }
    }

    pthread_mutex_lock(&race->mutex);
    pthread_cond_broadcast(&race->startCondition);
    pthread_mutex_unlock(&race->mutex);
    --race->activeHandlersCount;
    return NULL;
}

static int StartRace(Race* race) {
    if (race->settings->mapOnly) {
        DrawRaceStart(race);
        if (race->isInterrupted) {
            return 1;
        }
    }

    for (int i = 0; i < race->settings->carCount; ++i) {
        race->handlerArgs[i] = (CarHandlerArgs){.race = race, .carId = i};

        ++race->activeHandlersCount;
        int error = pthread_create(&race->clients[i].thread, NULL, CarHandler, &race->handlerArgs[i]);
        if (error != 0) {
            --race->activeHandlersCount;
            dprintf(2, "Create car handler: %s\n", strerror(error));
            return 1;
        }

        ++race->startedHandlersCount;
    }

    for (int i = 0; i < race->settings->carCount; ++i) {
        if (SendRaceMessage(race->clients[i].socket, RACE_START, NULL, 0) == -1) {
            perror("send race start command");
            return 1;
        }
    }

    pthread_mutex_lock(&race->mutex);
    race->isStarted = true;
    race->state = RACE_RUNNING;
    pthread_cond_broadcast(&race->startCondition);
    pthread_cond_broadcast(&race->viewers.condition);
    pthread_mutex_unlock(&race->mutex);

    dprintf(1, "Race started with %d cars\n", race->settings->carCount);
    return 0;
}

static void FinishCarConnections(Race* race) {
    struct pollfd connections[MAX_CARS + 1];
    int pending = 0;
    bool acceptReconnect = false;

    for (int i = 0; i < race->connectedCarsCount; ++i) {
        int socket = race->clients[i].socket;
        connections[i] = (struct pollfd){.fd = socket, .events = POLLIN};

        if (socket < 0) {
            acceptReconnect |= race->cars[i].isPaused;
            continue;
        }

        SendRaceMessage(socket, RACE_STOP, NULL, 0);

        // FIN идёт после уведомления; чтение остаётся доступным для завершения обмена.
        shutdown(socket, SHUT_WR);
        ++pending;
    }

    // Машина могла закрыть старый сокет и ещё не успеть подключиться заново.
    connections[race->connectedCarsCount] =
        (struct pollfd){.fd = acceptReconnect ? race->socket : -1, .events = POLLIN};

    struct timespec begin;
    clock_gettime(CLOCK_MONOTONIC, &begin);

    // Дочитываем оставшиеся запросы до EOF, чтобы close не сбросил TCP-соединение.
    while (pending > 0 || acceptReconnect) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsedMs = (now.tv_sec - begin.tv_sec) * 1000 + (now.tv_nsec - begin.tv_nsec) / 1000000;
        if (elapsedMs >= 1000) {
            break;
        }

        int ready = poll(connections, race->connectedCarsCount + 1, (int) (1000 - elapsedMs));
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            break;
        }

        if (connections[race->connectedCarsCount].revents != 0) {
            AcceptClientConnection(race, 0);
        }

        for (int i = 0; i < race->connectedCarsCount; ++i) {
            if (connections[i].fd < 0 || connections[i].revents == 0) {
                continue;
            }

            char discarded[1024];
            ssize_t count = recv(connections[i].fd, discarded, sizeof(discarded), MSG_DONTWAIT);
            if (count == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }

            if (count <= 0) {
                connections[i].fd = -1;
                --pending;
            }
        }
    }
}

static void StopRace(Race* race) {
    pthread_mutex_lock(&race->mutex);
    race->isOver = true;
    if (race->isInterrupted) {
        race->state = RACE_INTERRUPTED;
    } else if (!race->isStarted) {
        race->state = RACE_FAILED;
    } else {
        race->state = RACE_FINISHED;
    }

    pthread_cond_broadcast(&race->startCondition);
    pthread_cond_broadcast(&race->viewers.condition);
    pthread_mutex_unlock(&race->mutex);

    for (int i = 0; i < race->startedHandlersCount; ++i) {
        int error = pthread_join(race->clients[i].thread, NULL);
        if (error != 0) {
            dprintf(2, "Join car handler: %s\n", strerror(error));
        }
    }

    race->startedHandlersCount = 0;
    FinishCarConnections(race);
}

int RunRace(Race* race) {
    int result = 0;
    bool started = false;
    struct timespec begin = {0};
    struct timespec end;

    if (StartRaceServer(race)) {
        goto startupError;
    }

    if (WaitForCarConnections(race)) {
        goto startupError;
    }

    if (PlaceCarsOnGrid(race)) {
        goto startupError;
    }

    if (StartRace(race)) {
        goto startupError;
    }

    started = true;
    clock_gettime(CLOCK_MONOTONIC, &begin);

    while (race->activeHandlersCount > 0 && !race->isInterrupted) {
        if (AcceptClientConnection(race, 20)) {
            result = 1;
            break;
        }
    }

    goto finish;

startupError:
    if (race->isInterrupted) {
        result = 0;
        race->stats.status = "Interrupted by user";
    } else {
        result = 1;
        race->stats.status = "Failed to start";
    }

finish:
    StopRace(race);
    StopViewerServer(race);
    if (race->isInterrupted) {
        race->stats.status = "Interrupted by user";
        dprintf(1, "Race interrupted by user\n");
    }

    if (started) {
        clock_gettime(CLOCK_MONOTONIC, &end);

        double seconds = (double) (end.tv_sec - begin.tv_sec);
        double nanoseconds = (double) (end.tv_nsec - begin.tv_nsec);
        race->stats.duration = seconds + nanoseconds / 1e9;
    }

    if (race->winnerId < 0) {
        dprintf(1, "Race ended without a winner\n");
    }

    SaveRaceLog(race);
    return result;
}

void SaveRaceLog(Race* race) {
    int fd = open(race->settings->logFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Open log file failed");
        return;
    }
    dprintf(1, "Race report log successfully saved to: %s\n", race->settings->logFile);

    dprintf(fd, "==================================================\n");
    dprintf(fd, "              RACE SIMULATION REPORT              \n");
    dprintf(fd, "==================================================\n");
    dprintf(fd, "Status             : %s\n", race->stats.status);
    dprintf(fd, "Winner             : %s\n", race->stats.winner ? race->stats.winner : "None");
    dprintf(fd, "Duration           : %.2f s\n", race->stats.duration);
    dprintf(fd, "Participants       : %d\n", race->connectedCarsCount);

    if (race->judge.carCount > 0) {
        UpdateRaceLeaderBoard(race);

        dprintf(fd, "\n");
        dprintf(fd, "=============================LEADER BOARD==============================\n");
        dprintf(fd, " %-5s | %-12s | %-12s | %-5s | %-5s | %-5s | %-5s\n", "Place", "Team", "Driver", "Laps", "Lane",
                "Dist", "State");
    }

    for (int i = 0; i < race->judge.carCount; ++i) {
        const CarStanding* standing = &race->judge.leaderBoard[i];
        const Car* car = &standing->car;

        dprintf(fd, "   %-3d | %-12s | %-12s | %-5d | %-5d | %-5d | %-5s\n", i + 1, car->team.name, car->driverName,
                car->laps, standing->lane, standing->distToFinish,
                car->isOut      ? "OUT"
                : car->isPaused ? "PAUSED"
                                : "IN RACE");
    }

    if (close(fd) == -1) {
        perror("Close fd failed");
    }
}
