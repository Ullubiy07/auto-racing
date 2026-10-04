#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "track.h"

typedef struct {
	Point data[MAX_MAP_HEIGHT * MAX_MAP_WIDTH];
	int head;
	int tail;
} Queue;

static void QueuePush(Queue* queue, Point pos) {
	queue->data[queue->tail++] = pos;
}

static Point QueuePop(Queue* queue) {
	return queue->data[queue->head++];
}

static bool IsQueueEmpty(const Queue* queue) {
	return queue->head >= queue->tail;
}

static bool IsInsideTrack(const Track* track, Point pos) {
	return pos.x >= 0 && pos.x < track->width && pos.y >= 0 && pos.y < track->height;
}

static void CalcDistances(Track* track) {
	Queue queue = { .head = 0, .tail = 0 };
	
	for (int y = 0; y < track->height; ++y) {
		for (int x = 0; x < track->width; ++x) {
			track->map[y][x].distToFinish = -1;
		}
	}
	
	for (int y = 0; y < track->height; ++y) {
		for (int x = 0; x < track->width; ++x) {
			if (IsCellStart(track->map[y][x].type)) {
				track->map[y][x].distToFinish = 0;

				Direction startDir = track->grid.dir;
				Direction backDir = { -startDir.dx, -startDir.dy };
				Point backPos = { x + backDir.dx, y + backDir.dy };

				if (IsInsideTrack(track, backPos) &&
					IsCellDriveable(track->map[backPos.y][backPos.x].type) &&
					track->map[backPos.y][backPos.x].distToFinish == -1) 
				{
					track->map[backPos.y][backPos.x].distToFinish = 1;
					QueuePush(&queue, backPos);
				}
			}
		}
	}

	static const Direction dirs[] = {
		{ -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }
	};
	
	while (!IsQueueEmpty(&queue)) {
		Point pos = QueuePop(&queue);
		int dist = track->map[pos.y][pos.x].distToFinish;
		
		for (int i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
			Point newPos = { pos.x + dirs[i].dx, pos.y + dirs[i].dy };
			
			if (IsInsideTrack(track, newPos)) {
				Cell* cell = &track->map[newPos.y][newPos.x];
				
				if (IsCellDriveable(cell->type) && cell->distToFinish == -1) {
					cell->distToFinish = dist + 1;
					QueuePush(&queue, newPos);
				}
			}
		}
	}
}

static bool IsMovementClockwiseAt(const Track* track, Point pos, Direction dir) {
	while (pos.y >= 0 && pos.y < track->height &&
		pos.x >= 0 && pos.x < track->width) 
	{
		CellType type = track->map[pos.y][pos.x].type;
		if (!IsCellDriveable(type)) {
			dprintf(2, "No turn found (hit non-driveable cell)\n");
			exit(2);
		}
		if (IsCellTurn(type)) {
			Direction next = GetCellDirection(type);
			int cross = dir.dx * next.dy - dir.dy * next.dx;
			return cross > 0;
		}
		pos.x += dir.dx;
		pos.y += dir.dy;
	}
	return false;
}

static size_t LoadFile(const char* fileName, char* buf, size_t bufSize) {
	int fd = open(fileName, O_RDONLY);
	if (fd == -1) {
		perror("Open file error");
		exit(2);
	}
	int count = read(fd, buf, bufSize);
	if (count == -1) {
		perror("Read file error");
		close(fd);
		exit(2);
	}
	close(fd);
	return count;
}

void LoadTrack(Track* track, const char* fileName) {
	const size_t bufSize = MAX_MAP_HEIGHT * MAX_MAP_WIDTH;
	char buf[bufSize];
	int count = LoadFile(fileName, buf, bufSize);
	
	track->width = 0;
	track->height = 0;
	track->grid.laneCount = 0;
	
	for (int i = 0, x = 0; i < count; ++i) {
		if (buf[i] == '\n') {
			if (track->height == 0) {
				track->width = x;
			}
			++track->height;
			x = 0;
		} else {
			CellType type = GetCellType(buf[i]);
			if (type == LAYOUT_UNKNOWN) {
				dprintf(2, "Unknown cell type: '%c'\n", buf[i]);
				exit(2);
			}
			if (IsCellStart(type)) {
				++track->grid.laneCount;
				if (isupper(buf[i])) {
					track->grid.pos = (Point) { x, track->height };
					track->grid.dir = GetCellDirection(type);
				}
			}
			if (track->height >= MAX_MAP_HEIGHT || x >= MAX_MAP_WIDTH) {
				dprintf(2, "Track map exceeds size limits (max %dx%d)\n", MAX_MAP_WIDTH, MAX_MAP_HEIGHT);
				exit(2);
			}
			
			track->map[track->height][x].type = type;
			SetCellEntity(&track->map[track->height][x], ENTITY_NONE, NULL);
			++x;
		}
	}
	
	track->grid.isClockwise = IsMovementClockwiseAt(track, track->grid.pos, track->grid.dir);
	CalcDistances(track);
}
