#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "common.h"

int InitCar(Car* car, int id, const char* driverName, Team team) {
    car->id = id;
    strcpy(car->driverName, driverName);
    car->team = team;

    car->prevPos = (Point){-1, -1};
    car->laps = 0;
    car->hasPassedStart = false;
    car->isOut = false;

    return 0;
}

Point GetNextPosition(Point current, const Road* road, MoveType type) {
    switch (type) {
    case MOVE_FORWARD:
        return road->forward;
    case MOVE_LEFT:
        return road->left;
    case MOVE_RIGHT:
        return road->right;
    case MOVE_NONE:
        return current;
    }

    return (Point){-1, -1};
}

static bool SamePosition(Point a, Point b) {
    return a.x == b.x && a.y == b.y;
}

bool IsMoveAllowed(const Car* car, CellType currentType, const Road* road, Point next) {
    if (next.x < 0 || next.y < 0) {
        return false;
    }

    if (SamePosition(next, car->pos)) {
        return false;
    }

    if (SamePosition(next, car->prevPos)) {
        return false;
    }

    if (SamePosition(next, road->forward)) {
        return true;
    }

    if (IsCellTurn(currentType)) {
        return false;
    }

    return SamePosition(next, road->left) || SamePosition(next, road->right);
}

int SendAll(int socket, const void* data, size_t size) {
    const char* bytes = data;
    size_t sent = 0;

    while (sent < size) {
        ssize_t count = send(socket, bytes + sent, size - sent, MSG_NOSIGNAL);

        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (count == 0) {
            errno = EPIPE;
            return -1;
        }

        sent += (size_t) count;
    }

    return 0;
}

int RecvAll(int socket, void* data, size_t size) {
    char* bytes = data;
    size_t received = 0;

    while (received < size) {
        ssize_t count = recv(socket, bytes + received, size - received, 0);

        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (count == 0) {
            errno = ECONNRESET;
            return -1;
        }

        received += (size_t) count;
    }

    return 0;
}

CellData MakeCellData(Cell* cell) {
    pthread_mutex_lock(&cell->mutex);

    CellData data = {.x = cell->x,
                     .y = cell->y,
                     .color = cell->color,
                     .type = cell->type,
                     .entity = cell->entity,
                     .road = cell->road};

    pthread_mutex_unlock(&cell->mutex);

    return data;
}
