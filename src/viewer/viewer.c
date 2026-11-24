#include "viewer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static bool ValidInitialization(const ViewerInitMessage* message) {
    if (message->mapWidth <= 0 || message->mapWidth > MAX_MAP_WIDTH ||
        message->mapHeight <= 0 || message->mapHeight > MAX_MAP_HEIGHT) {
        return false;
    }

    if (message->expectedCarCount <= 0 || message->expectedCarCount > MAX_CARS || message->rules.maxLaps <= 0) {
        return false;
    }

    for (int y = 0; y < message->mapHeight; ++y) {
        for (int x = 0; x < message->mapWidth; ++x) {
            if (GetCellInfo(message->map[y][x].type) == NULL) {
                return false;
            }
        }
    }

    return true;
}

int InitViewerClient(ViewerClient* client, const ViewerClientSettings* settings) {
    client->isTerminal = isatty(STDOUT_FILENO);
    client->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket == -1) {
        perror("create viewer socket");
        return 1;
    }

    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(settings->port)
    };

    if (inet_pton(AF_INET, settings->ip, &server.sin_addr) != 1) {
        dprintf(2, "Invalid server IPv4 address: %s\n", settings->ip);
        return 1;
    }

    if (connect(client->socket, (const struct sockaddr*) &server, sizeof(server)) == -1) {
        if (!client->isInterrupted) {
            perror("connect viewer to server");
        }

        return 1;
    }

    struct timeval timeout = {.tv_sec = 5};
    if (setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        perror("set viewer registration timeout");
        return 1;
    }

    ClientType type = CLIENT_VIEWER;
    if (SendAll(client->socket, &type, sizeof(type)) == -1) {
        perror("send viewer client type");
        return 1;
    }

    ViewerRegistrationResponse response;
    if (RecvAll(client->socket, &response, sizeof(response)) == -1) {
        perror("receive viewer registration response");
        return 1;
    }

    if (!response.accepted) {
        dprintf(2, "Server rejected viewer registration\n");
        return 1;
    }

    if (RecvAll(client->socket, &client->init, sizeof(client->init)) == -1) {
        perror("receive viewer initialization");
        return 1;
    }

    if (!ValidInitialization(&client->init)) {
        dprintf(2, "Invalid viewer initialization from server\n");
        return 1;
    }

    timeout.tv_sec = 0;
    if (setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        perror("clear viewer registration timeout");
        return 1;
    }

    InitJudge(&client->judge, &client->init.rules);
    return 0;
}

static bool ValidSnapshot(const ViewerClient* client) {
    const RaceSnapshot* snapshot = &client->snapshot;

    if (snapshot->state < RACE_WAITING || snapshot->state > RACE_FAILED ||
        snapshot->carCount < 0 || snapshot->carCount > client->init.expectedCarCount ||
        snapshot->winnerId < -1 || snapshot->winnerId >= snapshot->carCount) {
        return false;
    }

    for (int i = 0; i < snapshot->carCount; ++i) {
        const Car* car = &snapshot->standings[i].car;

        if (car->id != i || car->laps < 0 || car->team.color < 31 || car->team.color > 37 ||
            !memchr(car->driverName, '\0', sizeof(car->driverName)) ||
            !memchr(car->team.name, '\0', sizeof(car->team.name))) {
            return false;
        }

        bool validPosition = car->pos.x >= 0 && car->pos.x < client->init.mapWidth &&
                             car->pos.y >= 0 && car->pos.y < client->init.mapHeight;
        bool notPlaced = car->pos.x == -1 && car->pos.y == -1;

        if (!validPosition && !notPlaced) {
            return false;
        }

        if (notPlaced && (snapshot->state == RACE_RUNNING || snapshot->state == RACE_FINISHED)) {
            return false;
        }
    }

    return true;
}

static void DrawTrack(const ViewerClient* client) {
    int occupants[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];

    for (int y = 0; y < client->init.mapHeight; ++y) {
        for (int x = 0; x < client->init.mapWidth; ++x) {
            occupants[y][x] = -1;
        }
    }

    for (int i = 0; i < client->snapshot.carCount; ++i) {
        const Car* car = &client->snapshot.standings[i].car;
        if (!car->isOut && car->pos.x >= 0 && car->pos.y >= 0) {
            occupants[car->pos.y][car->pos.x] = i;
        }
    }

    for (int y = 0; y < client->init.mapHeight; ++y) {
        for (int x = 0; x < client->init.mapWidth; ++x) {
            const CellData* cell = &client->init.map[y][x];
            const char* symbol = GetCellInfo(cell->type)->outSymbol;
            int color = cell->color;
            int id = occupants[y][x];

            if (id >= 0) {
                symbol = "🏎 ";
                color = client->snapshot.standings[id].car.team.color;
            }

            if (client->isTerminal) {
                dprintf(1, "\x1b[%dm%s\x1b[0m", color, symbol);
            } else {
                dprintf(1, "%s", symbol);
            }
        }

        dprintf(1, "\n");
    }
}

static void DrawViewer(ViewerClient* client) {
    if (client->isTerminal) {
        dprintf(1, "\x1b[H");
    }

    DrawTrack(client);

    UpdateLeaderBoard(&client->judge, client->snapshot.standings, client->snapshot.carCount);
    DrawLeaderBoard(&client->judge, STDOUT_FILENO);

    if (client->snapshot.winnerId >= 0) {
        const Car* winner = &client->snapshot.standings[client->snapshot.winnerId].car;
        dprintf(1, "Winner: %s, team %s (car %d)\n", winner->driverName, winner->team.name, winner->id);
    } else if (client->snapshot.state >= RACE_FINISHED) {
        dprintf(1, "Race ended without a winner\n");
    }

    if (client->isTerminal) {
        dprintf(1, "\x1b[J");
    }

    dprintf(1, "\n");
}

int RunViewerClient(ViewerClient* client) {
    if (client->isTerminal) {
        dprintf(1, "\x1b[2J\x1b[H\x1b[?25l");
    }

    while (!client->isInterrupted) {
        if (RecvAll(client->socket, &client->snapshot, sizeof(client->snapshot)) == -1) {
            if (client->isInterrupted) {
                break;
            }

            dprintf(2, "Server disconnected before the final race snapshot\n");
            return 1;
        }

        if (!ValidSnapshot(client)) {
            dprintf(2, "Invalid race snapshot from server\n");
            return 1;
        }

        DrawViewer(client);

        if (client->snapshot.state >= RACE_FINISHED) {
            return client->snapshot.state == RACE_FAILED ? 1 : 0;
        }
    }

    return 0;
}

void DestroyViewerClient(ViewerClient* client) {
    if (client->isTerminal) {
        dprintf(1, "\x1b[0m\x1b[?25h");
    }

    if (client->socket >= 0) {
        close(client->socket);
        client->socket = -1;
    }
}
