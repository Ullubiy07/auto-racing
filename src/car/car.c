#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#include "car.h"

int InitCarClient(CarClient* client, const CarClientSettings* settings) {
    client->socket = -1;
    client->cell = NULL;
    client->isOver = false;
    client->failed = false;
    client->winnerId = -1;
    client->isInterrupted = 0;

    InitCar(&client->car, -1, settings->driverName, settings->team);

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        perror("create tcp socket");
        return 1;
    }

    struct sockaddr_in server = {.sin_family = AF_INET, .sin_port = htons(settings->port)};

    if (inet_pton(AF_INET, settings->ip, &server.sin_addr) != 1) {
        dprintf(2, "Invalid server IPv4 address: %s\n", settings->ip);
        goto error;
    }

    if (connect(clientSocket, (const struct sockaddr*) &server, sizeof(server))) {
        perror("connect to the server");
        goto error;
    }

    CarRegistrationMessage registration = {.team = settings->team};

    strcpy(registration.driverName, settings->driverName);

    ClientType type = CLIENT_CAR;
    if (SendAll(clientSocket, &type, sizeof(type)) == -1) {
        perror("send car client type");
        goto error;
    }

    if (SendAll(clientSocket, &registration, sizeof(registration)) == -1) {
        perror("register car");
        goto error;
    }

    CarRegistrationResponse response;
    if (RecvAll(clientSocket, &response, sizeof(response)) == -1) {
        perror("register car");
        goto error;
    }

    if (!response.accepted) {
        dprintf(2, "Server rejected registration of %s\n", settings->driverName);
        goto error;
    }

    if (response.id < 0 || response.id >= MAX_CARS) {
        dprintf(2, "Server assigned an invalid car ID\n");
        goto error;
    }

    client->car.id = response.id;

    dprintf(1, "Car %d (%s) registered; waiting for the track\n", client->car.id, settings->driverName);

    CarInitMessage msg;
    if (RecvAll(clientSocket, &msg, sizeof(msg)) == -1) {
        if (errno == ECONNRESET) {
            dprintf(2, "Server disconnected before sending the complete car initialization msg\n");
        } else {
            perror("receive car initialization msg");
        }

        goto error;
    }

    if (msg.mapWidth <= 0 || msg.mapWidth > MAX_MAP_WIDTH) {
        dprintf(2, "Invalid map dimensions in car initialization msg\n");
        goto error;
    }

    if (msg.mapHeight <= 0 || msg.mapHeight > MAX_MAP_HEIGHT) {
        dprintf(2, "Invalid map dimensions in car initialization msg\n");
        goto error;
    }

    if (msg.pos.x < 0 || msg.pos.x >= msg.mapWidth) {
        dprintf(2, "Invalid starting position in car initialization msg\n");
        goto error;
    }

    if (msg.pos.y < 0 || msg.pos.y >= msg.mapHeight) {
        dprintf(2, "Invalid starting position in car initialization msg\n");
        goto error;
    }

    bool validDelay = msg.settings.minSleepTimeMs >= 0 && msg.settings.maxSleepTimeMs >= msg.settings.minSleepTimeMs;

    if (!validDelay || msg.settings.maxLaps <= 0) {
        dprintf(2, "Invalid settings in car initialization msg\n");
        goto error;
    }

    client->mapWidth = msg.mapWidth;
    client->mapHeight = msg.mapHeight;
    memcpy(client->map, msg.map, sizeof(client->map));
    client->settings = msg.settings;

    client->car.pos = msg.pos;
    client->cell = &client->map[msg.pos.y][msg.pos.x];
    client->socket = clientSocket;

    dprintf(1, "Car %d initialized at (%d, %d), track %dx%d\n", client->car.id, msg.pos.x, msg.pos.y, msg.mapWidth,
            msg.mapHeight);
    return 0;

error:
    close(clientSocket);
    return 1;
}

void DestroyCarClient(CarClient* client) {
    if (client->socket >= 0) {
        if (close(client->socket) == -1) {
            perror("close car socket");
        }

        client->socket = -1;
    }
}

static CellData* GetNextCell(CarClient* client, MoveType type) {
    Point pos = GetNextPosition(client->car.pos, &client->cell->road, type);
    if (pos.x < 0 || pos.x >= client->mapWidth || pos.y < 0 || pos.y >= client->mapHeight) {
        return NULL;
    }

    return &client->map[pos.y][pos.x];
}

static void ConnectionFailed(CarClient* client, const char* operation) {
    if (!client->isInterrupted) {
        perror(operation);
        client->failed = true;
    }

    client->isOver = true;
}

static bool ReceiveCarState(CarClient* client, const CarMoveRequest* request) {
    CarMoveResponse response;
    if (RecvAll(client->socket, &response, sizeof(response)) == -1) {
        ConnectionFailed(client, "receive car move response");
        return false;
    }

    bool validId = response.car.id == client->car.id;
    bool validX = response.car.pos.x >= 0 && response.car.pos.x < client->mapWidth;
    bool validY = response.car.pos.y >= 0 && response.car.pos.y < client->mapHeight;

    if (!validId || !validX || !validY) {
        dprintf(2, "Invalid car state in move response\n");
        client->failed = true;
        client->isOver = true;
        return false;
    }

    client->car = response.car;
    client->cell = &client->map[client->car.pos.y][client->car.pos.x];

    client->isOver = response.raceOver;
    client->winnerId = response.winnerId;

    if (response.allowed) {
        dprintf(1, "%s: move to (%d, %d) accepted, laps %d\n", client->car.driverName, client->car.pos.x,
                client->car.pos.y, client->car.laps);
    } else if (request && !response.raceOver) {
        dprintf(1, "%s: move to (%d, %d) rejected\n", client->car.driverName, request->pos.x, request->pos.y);
    }

    return response.allowed;
}

static bool TryMove(CarClient* client, MoveType type) {
    if (client->isOver || client->isInterrupted) {
        return false;
    }

    CellData* next = GetNextCell(client, type);
    if (!next) {
        return false;
    }

    Point nextPosition = {.x = next->x, .y = next->y};

    if (!IsMoveAllowed(&client->car, client->cell->type, &client->cell->road, nextPosition)) {
        return false;
    }

    CarMoveRequest request = {.pos = nextPosition};

    if (SendAll(client->socket, &request, sizeof(request)) == -1) {
        ConnectionFailed(client, "send car move request");
        return false;
    }

    return ReceiveCarState(client, &request);
}

static bool TryRandomMove(CarClient* client) {
    MoveType moves[] = {MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT};

    for (size_t i = 0; i < 3; ++i) {
        int j = rand() % (i + 1);
        MoveType temp = moves[i];
        moves[i] = moves[j];
        moves[j] = temp;
    }

    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(client, moves[i])) {
            return true;
        }
    }

    return false;
}

typedef struct {
    CarClient* client;
    MoveType type;
} Move;

static int CompareMoves(const void* lhs, const void* rhs) {
    const Move* moveA = lhs;
    const Move* moveB = rhs;

    CellData* cellA = GetNextCell(moveA->client, moveA->type);
    CellData* cellB = GetNextCell(moveB->client, moveB->type);

    if (!cellA) {
        return cellB ? 1 : 0;
    }

    if (!cellB) {
        return -1;
    }

    return cellA->road.distToFinish - cellB->road.distToFinish;
}

static bool TryBestMove(CarClient* client) {
    Move moves[] = {{.client = client, .type = MOVE_FORWARD},
                    {.client = client, .type = MOVE_RIGHT},
                    {.client = client, .type = MOVE_LEFT}};

    qsort(moves, 3, sizeof(moves[0]), CompareMoves);
    for (size_t i = 0; i < 3; ++i) {
        if (TryMove(client, moves[i].type)) {
            return true;
        }
    }

    return false;
}

static int WaitForStart(CarClient* client) {
    RaceCommand command;
    if (RecvAll(client->socket, &command, sizeof(command)) == -1) {
        ConnectionFailed(client, "wait for race start");
        return 1;
    }

    if (command != RACE_START) {
        dprintf(2, "Server stopped the race before its start\n");
        client->isOver = true;
        return 1;
    }

    dprintf(1, "%s: race started\n", client->car.driverName);
    return 0;
}

int RunCarClient(CarClient* client) {
    if (WaitForStart(client)) {
        return client->failed ? 1 : 0;
    }

    unsigned int seed = 0;
    for (int i = 0; client->car.driverName[i] != '\0'; ++i) {
        seed = seed * 37 + (unsigned char) client->car.driverName[i];
    }

    while (!client->isOver && !client->isInterrupted && !client->car.isOut) {
        if (rand_r(&seed) % 2) {
            TryRandomMove(client);
        } else {
            TryBestMove(client);
        }

        if (client->isOver || client->isInterrupted) {
            break;
        }

        int minDelayMs = client->settings.minSleepTimeMs;
        int maxDelayMs = client->settings.maxSleepTimeMs;
        unsigned int delayRange = (unsigned int) (maxDelayMs - minDelayMs) + 1U;
        int delayMs = (int) (rand_r(&seed) % delayRange) + minDelayMs;

        struct pollfd socket = {.fd = client->socket, .events = POLLIN};

        int ready = poll(&socket, 1, delayMs);
        if (ready == -1 && errno != EINTR) {
            ConnectionFailed(client, "wait between car moves");
        } else if (ready > 0 && !client->isInterrupted) {
            ReceiveCarState(client, NULL);
        }
    }

    if (client->isInterrupted) {
        dprintf(1, "%s: leaving the race\n", client->car.driverName);
    } else if (client->isOver && !client->failed) {
        dprintf(1, "%s: race finished, winner ID %d\n", client->car.driverName, client->winnerId);
    }

    return client->failed ? 1 : 0;
}
