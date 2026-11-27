#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "cell.h"

#include "track.h"

typedef struct {
    Point pos;
    Direction dir;  // Направление движения при обходе трассы
} Node;

typedef struct {
    Node data[MAX_MAP_HEIGHT * MAX_MAP_WIDTH];
    int head;  // Индекс следующего извлекаемого узла
    int tail;  // Индекс для добавления узла
} Queue;

static void QueuePush(Queue* queue, Node node) {
    queue->data[queue->tail++] = node;
}

static Node QueuePop(Queue* queue) {
    return queue->data[queue->head++];
}

static bool IsQueueEmpty(const Queue* queue) {
    return queue->head >= queue->tail;
}

static bool IsInsideTrack(const Track* track, Point pos) {
    return pos.x >= 0 && pos.x < track->width && pos.y >= 0 && pos.y < track->height;
}

static void CalcDistances(Track* track) {
    Queue queue = {.head = 0, .tail = 0};

    for (int y = 0; y < track->height; ++y) {
        for (int x = 0; x < track->width; ++x) {
            track->map[y][x].road.distToFinish = -1;
        }
    }

    for (int y = 0; y < track->height; ++y) {
        for (int x = 0; x < track->width; ++x) {
            if (IsCellStart(track->map[y][x].type)) {
                Direction startDir = track->grid.dir;
                Direction backDir = {-startDir.dx, -startDir.dy};
                Point backPos = {x + backDir.dx, y + backDir.dy};

                if (IsInsideTrack(track, backPos) && IsCellDriveable(track->map[backPos.y][backPos.x].type) &&
                    track->map[backPos.y][backPos.x].road.distToFinish == -1) {
                    track->map[backPos.y][backPos.x].road.distToFinish = 1;
                    QueuePush(&queue, (Node){.pos = backPos});
                }
            }
        }
    }

    static const Direction dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    while (!IsQueueEmpty(&queue)) {
        Point pos = QueuePop(&queue).pos;
        int dist = track->map[pos.y][pos.x].road.distToFinish;

        for (int i = 0; i < 4; ++i) {
            Point newPos = {pos.x + dirs[i].dx, pos.y + dirs[i].dy};

            if (IsInsideTrack(track, newPos)) {
                Cell* cell = &track->map[newPos.y][newPos.x];

                if (IsCellDriveable(cell->type) && cell->road.distToFinish == -1 &&
                    (!IsCellStart(cell->type) || dist > 1)) {
                    cell->road.distToFinish = dist + 1;
                    QueuePush(&queue, (Node){.pos = newPos});
                }
            }
        }
    }
}

static int SetMovementClockwise(Track* track) {
    Point pos = track->grid.pos;
    Direction dir = track->grid.dir;

    while (pos.y >= 0 && pos.y < track->height && pos.x >= 0 && pos.x < track->width) {
        CellType type = track->map[pos.y][pos.x].type;
        if (!IsCellDriveable(type)) {
            dprintf(2, "No turn found (hit non-driveable cell)\n");
            return 1;
        }
        if (IsCellTurn(type)) {
            Direction next = GetCellDirection(type);
            int cross = dir.dx * next.dy - dir.dy * next.dx;
            track->grid.isClockwise = cross > 0;
            return 0;
        }
        pos.x += dir.dx;
        pos.y += dir.dy;
    }
    dprintf(2, "No turn found before track boundary\n");
    return 1;
}

static void BuildRoadGraph(Track* track) {
    for (int y = 0; y < track->height; ++y) {
        for (int x = 0; x < track->width; ++x) {
            track->map[y][x].road = (Road){.lane = 0,
                                           .distToFinish = track->map[y][x].road.distToFinish,
                                           .forward = {-1, -1},
                                           .left = {-1, -1},
                                           .right = {-1, -1},
                                           .back = {-1, -1}};
        }
    }

    Queue queue = {.head = 0, .tail = 0};
    Point start = track->grid.pos;
    Road* road = &track->map[start.y][start.x].road;
    road->lane = track->grid.laneCount;

    QueuePush(&queue, (Node){start, track->grid.dir});

    bool visited[MAX_MAP_HEIGHT][MAX_MAP_WIDTH] = {false};
    visited[start.y][start.x] = true;

    while (!IsQueueEmpty(&queue)) {
        Node node = QueuePop(&queue);
        Cell* cell = &track->map[node.pos.y][node.pos.x];

        if (!IsCellDriveable(cell->type)) {
            continue;
        }

        Direction dir = node.dir;
        if (IsCellTurn(cell->type) || IsCellStart(cell->type)) {
            dir = GetCellDirection(cell->type);
        }

        // Определение передней для текущей и задней для передней
        Point forward = {node.pos.x + dir.dx, node.pos.y + dir.dy};
        if (IsInsideTrack(track, forward)) {
            Cell* newCell = &track->map[forward.y][forward.x];

            if (IsCellDriveable(newCell->type)) {
                cell->road.forward = (Point){newCell->x, newCell->y};
                newCell->road.back = (Point){cell->x, cell->y};

                if (!visited[forward.y][forward.x]) {
                    visited[forward.y][forward.x] = true;

                    Cell* newCell = &track->map[forward.y][forward.x];
                    newCell->road.lane = cell->road.lane;

                    QueuePush(&queue, (Node){forward, dir});
                }
            }
        }

        // Определение левой клетки
        Point left = {node.pos.x + dir.dy, node.pos.y - dir.dx};
        if (IsInsideTrack(track, left)) {
            Cell* newCell = &track->map[left.y][left.x];
            if (IsCellDriveable(newCell->type)) {
                cell->road.left = (Point){newCell->x, newCell->y};

                if (!visited[left.y][left.x]) {
                    visited[left.y][left.x] = true;

                    Cell* newCell = &track->map[left.y][left.x];
                    newCell->road.lane = cell->road.lane + (track->grid.isClockwise ? 1 : -1);

                    QueuePush(&queue, (Node){left, dir});
                }
            }
        }

        // Определение правой клетку
        Point right = {node.pos.x - dir.dy, node.pos.y + dir.dx};
        if (IsInsideTrack(track, right)) {
            Cell* newCell = &track->map[right.y][right.x];
            if (IsCellDriveable(newCell->type)) {
                cell->road.right = (Point){newCell->x, newCell->y};

                if (!visited[right.y][right.x]) {
                    visited[right.y][right.x] = true;

                    Cell* newCell = &track->map[right.y][right.x];
                    newCell->road.lane = cell->road.lane + (track->grid.isClockwise ? -1 : 1);

                    QueuePush(&queue, (Node){right, dir});
                }
            }
        }
    }
}

static int LoadFile(const char* fileName, char* buf, size_t bufSize) {
    int fd = open(fileName, O_RDONLY);
    if (fd == -1) {
        perror("Open file error");
        return -1;
    }
    size_t total = 0;
    while (total < bufSize) {
        ssize_t count = read(fd, buf + total, bufSize - total);
        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("Read file error");
            close(fd);
            return -1;
        }
        if (count == 0) {
            break;
        }
        total += (size_t) count;
    }
    if (close(fd) == -1) {
        perror("Close fd error");
    }
    return (int) total;
}

int LoadTrack(Track* track, const char* fileName) {
    track->width = 0;
    track->height = 0;
    track->initCellCount = 0;
    track->grid = (Grid){.pos = {-1, -1}};

    // Один байт сверх максимума позволяет обнаружить слишком большой файл.
    const size_t maxFileSize = MAX_MAP_HEIGHT * (MAX_MAP_WIDTH + 1);
    char buf[maxFileSize + 1];
    int count = LoadFile(fileName, buf, sizeof(buf));
    if (count == -1) {
        return 1;
    }
    if (count == 0) {
        dprintf(2, "Track map is empty\n");
        return 1;
    }
    if ((size_t) count > maxFileSize) {
        dprintf(2, "Track map file exceeds size limit\n");
        return 1;
    }

    int width = 0, col = 0, row = 0;
    int cellCount = 0;

    // Проверка прямоугольной карты и удаление переводов строк.
    for (int i = 0; i <= count; ++i) {
        if (i == count || buf[i] == '\n') {
            if (i == count && col == 0) {
                break;
            }
            if (col == 0) {
                dprintf(2, "Track map contains an empty row\n");
                return 1;
            }
            if (width == 0) {
                width = col;
            } else if (col != width) {
                dprintf(2, "Track map rows have different widths\n");
                return 1;
            }
            ++row;
            col = 0;
        } else {
            CellType type = GetCellType(buf[i]);
            if (buf[i] == '\0' || type == LAYOUT_UNKNOWN) {
                dprintf(2, "Unknown cell type: '%c'\n", buf[i]);
                return 1;
            }

            if (row >= MAX_MAP_HEIGHT || col >= MAX_MAP_WIDTH) {
                dprintf(2, "Track map exceeds size limits (max %dx%d)\n", MAX_MAP_WIDTH, MAX_MAP_HEIGHT);
                return 1;
            }
            buf[cellCount++] = buf[i];
            ++col;
        }
    }

    // Заполнение структуры трассы на основе обработанных данных
    track->width = width;
    track->height = row;
    int startCount = 0;

    for (int y = 0, i = 0; y < track->height; ++y) {
        for (int x = 0; x < track->width; ++x, ++i) {
            CellType type = GetCellType(buf[i]);

            if (IsCellStart(type)) {
                ++track->grid.laneCount;
                if (isupper((unsigned char) buf[i])) {
                    ++startCount;
                    track->grid.pos = (Point){x, y};
                    track->grid.dir = GetCellDirection(type);
                }
            }
            if (InitCell(&track->map[y][x], x, y, type)) {
                goto error;
            }
            ++track->initCellCount;
        }
    }

    if (startCount != 1) {
        dprintf(2, "Track map must contain exactly one uppercase start marker\n");
        goto error;
    }
    if (SetMovementClockwise(track)) {
        goto error;
    }
    CalcDistances(track);
    BuildRoadGraph(track);
    return 0;

error:
    DestroyTrack(track);
    return 1;
}

void DestroyTrack(Track* track) {
    for (int i = 0; i < track->initCellCount; ++i) {
        DestroyCell(&track->map[i / track->width][i % track->width]);
    }
    track->initCellCount = 0;
}
