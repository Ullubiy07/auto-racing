#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "cell.h"

#include "track.h"

typedef struct {
    Point pos;
    Direction dir;
} Node;

typedef struct {
    Node data[MAX_MAP_HEIGHT * MAX_MAP_WIDTH];
    int head;
    int tail;
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
    track->grid.isClockwise = false;
    return 0;
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
    int count = read(fd, buf, bufSize);
    if (count == -1) {
        perror("Read file error");
        close(fd);
        return -1;
    }
    if (close(fd) == -1) {
        perror("Close fd error");
    }
    return count;
}

static Track* NewTrack(int width, int height) {
    char buf[20];
    snprintf(buf, sizeof(buf), "track-shm-%d", getpid());

    int fd = shm_open(buf, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("Shared memory open failed");
        return NULL;
    }

    size_t size = sizeof(Track) + height * sizeof(Cell*) + width * height * sizeof(Cell);
    if (ftruncate(fd, size) == -1) {
        perror("Truncate file failed");
        if (close(fd) == -1) {
            perror("Close fd error");
        }
        if (shm_unlink(buf) == -1) {
            perror("Unlink shared memory failed");
        }
        return NULL;
    }

    Track* track = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (track == MAP_FAILED) {
        perror("Memory map failed");
        if (close(fd) == -1) {
            perror("Close fd error");
        }
        if (shm_unlink(buf) == -1) {
            perror("Unlink shared memory failed");
        }
        return NULL;
    }
    if (close(fd) == -1) {
        perror("Close fd error");
    }

    track->map = (Cell**) (track + 1);
    track->width = width;
    track->height = height;

    Cell* cells = (Cell*) (track->map + height);
    for (int i = 0; i < height; ++i) {
        track->map[i] = cells + i * width;
    }
    return track;
}

void DestroyTrack(Track* track) {
    size_t size = sizeof(Track) + track->height * sizeof(Cell*) + track->width * track->height * sizeof(Cell);
    if (munmap(track, size) == -1) {
        perror("Unmap memory failed");
    }

    char buf[20];
    snprintf(buf, sizeof(buf), "track-shm-%d", getpid());
    if (shm_unlink(buf) == -1) {
        perror("Unlink shared memory failed");
    }
}

Track* LoadTrack(const char* fileName) {
    Track* track;
    const size_t bufSize = MAX_MAP_HEIGHT * MAX_MAP_WIDTH;
    char buf[bufSize];
    int count = LoadFile(fileName, buf, bufSize);
    if (count == -1) {
        return NULL;
    }

    int width = 0, col = 0, row = 0;
    int newlineCount = 0;

    // Обработка и валидация входных параметров, очистка буфера от '\n'
    for (int i = 0; i < count; ++i) {
        if (buf[i] == '\n') {
            if (row == 0) {
                width = col;
            }
            ++row;
            ++newlineCount;
            col = 0;
        } else {
            buf[i - newlineCount] = buf[i];

            CellType type = GetCellType(buf[i]);
            if (type == LAYOUT_UNKNOWN) {
                dprintf(2, "Unknown cell type: '%c'\n", buf[i]);
                return NULL;
            }

            if (row >= MAX_MAP_HEIGHT || col >= MAX_MAP_WIDTH) {
                dprintf(2, "Track map exceeds size limits (max %dx%d)\n", MAX_MAP_WIDTH, MAX_MAP_HEIGHT);
                return NULL;
            }
            ++col;
        }
    }

    // Заполнение структуры трассы на основе обработанных данных
    track = NewTrack(width, row);
    if (!track) {
        return NULL;
    }
    track->grid.laneCount = 0;

    for (int y = 0, i = 0; y < track->height; ++y) {
        for (int x = 0; x < track->width; ++x, ++i) {
            CellType type = GetCellType(buf[i]);

            if (IsCellStart(type)) {
                ++track->grid.laneCount;
                if (isupper(buf[i])) {
                    track->grid.pos = (Point){x, y};
                    track->grid.dir = GetCellDirection(type);
                }
            }
            if (InitCell(&track->map[y][x], x, y, type)) {
                DestroyTrack(track);
                return NULL;
            }
        }
    }

    if (SetMovementClockwise(track)) {
        DestroyTrack(track);
        return NULL;
    }
    CalcDistances(track);
    BuildRoadGraph(track);
    return track;
}
