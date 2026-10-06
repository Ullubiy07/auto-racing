#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
	Queue queue = { .head = 0, .tail = 0 };
	
	for (int y = 0; y < track->height; ++y) {
		for (int x = 0; x < track->width; ++x) {
			track->map[y][x].road.distToFinish = -1;
		}
	}
	
	for (int y = 0; y < track->height; ++y) {
		for (int x = 0; x < track->width; ++x) {
			if (IsCellStart(track->map[y][x].type)) {

				Direction startDir = track->grid.dir;
				Direction backDir = { -startDir.dx, -startDir.dy };
				Point backPos = { x + backDir.dx, y + backDir.dy };

				if (IsInsideTrack(track, backPos) &&
					IsCellDriveable(track->map[backPos.y][backPos.x].type) &&
					track->map[backPos.y][backPos.x].road.distToFinish == -1) 
				{
					track->map[backPos.y][backPos.x].road.distToFinish = 1;
					QueuePush(&queue, (Node) { .pos = backPos });
				}
			}
		}
	}

	static const Direction dirs[4] = {
		{ -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }
	};
	
	while (!IsQueueEmpty(&queue)) {
		Point pos = QueuePop(&queue).pos;
		int dist = track->map[pos.y][pos.x].road.distToFinish;
		
		for (int i = 0; i < 4; ++i) {
			Point newPos = { pos.x + dirs[i].dx, pos.y + dirs[i].dy };
			
			if (IsInsideTrack(track, newPos)) {
				Cell* cell = &track->map[newPos.y][newPos.x];
				
				if (IsCellDriveable(cell->type) && cell->road.distToFinish == -1 && (!IsCellStart(cell->type) || dist > 1)) {
					cell->road.distToFinish = dist + 1;
					QueuePush(&queue, (Node) { .pos = newPos });
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

static void BuildRoadGraph(Track* track) {
	for (int y = 0; y < track->height; ++y) {
		for (int x = 0; x < track->width; ++x) {
			track->map[y][x].road = (Road){ 
				.lane = 0, .distToFinish = track->map[y][x].road.distToFinish,
				.forward = NULL, .left = NULL, .right = NULL, .back = NULL 
			};
		}
	}
	
	Queue queue = { .head = 0, .tail = 0 };
	Point start = track->grid.pos;
	Road* road = &track->map[start.y][start.x].road;
	road->lane = track->grid.laneCount;
	bool isClockwise = IsMovementClockwiseAt(track, start, track->grid.dir);
	
	QueuePush(&queue, (Node){ start, track->grid.dir });
	
	bool visited[MAX_MAP_HEIGHT][MAX_MAP_WIDTH] = { false };
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
		Point forward = { node.pos.x + dir.dx, node.pos.y + dir.dy };
		if (IsInsideTrack(track, forward)) {
			Cell* newCell = &track->map[forward.y][forward.x];
			
			if (IsCellDriveable(newCell->type)) {
				cell->road.forward = newCell;
				newCell->road.back = cell;
				
				if (!visited[forward.y][forward.x]) {
					visited[forward.y][forward.x] = true;
					
					Cell* newCell = &track->map[forward.y][forward.x];
					newCell->road.lane = cell->road.lane;
					
					QueuePush(&queue, (Node){ forward, dir });
				}
			}
		}
		
		// Определение левой клетки
		Point left = { node.pos.x + dir.dy, node.pos.y - dir.dx };
		if (IsInsideTrack(track, left)) {
			Cell* newCell = &track->map[left.y][left.x];
			if (IsCellDriveable(newCell->type)) {
				cell->road.left = newCell;
				
				if (!visited[left.y][left.x]) {
					visited[left.y][left.x] = true;
					
					Cell* newCell = &track->map[left.y][left.x];
					newCell->road.lane = cell->road.lane + (isClockwise ? 1 : -1);
					
					QueuePush(&queue, (Node){ left, dir });
				}
			}
		}
		
		// Определение правой клетку
		Point right = { node.pos.x - dir.dy, node.pos.y + dir.dx };
		if (IsInsideTrack(track, right)) {
			Cell* newCell = &track->map[right.y][right.x];
			if (IsCellDriveable(newCell->type)) {
				cell->road.right = newCell;
				
				if (!visited[right.y][right.x]) {
					visited[right.y][right.x] = true;
					
					Cell* newCell = &track->map[right.y][right.x];
					newCell->road.lane = cell->road.lane + (isClockwise ? -1 : 1);
					
					QueuePush(&queue, (Node){ right, dir });
				}
			}
		}
	}
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
			
			Cell* cell = &track->map[track->height][x];
			cell->type = type;
			cell->x = x;
			cell->y = track->height;
			SetCellEntity(cell, ENTITY_NONE, NULL);
			++x;
		}
	}
	
	CalcDistances(track);
	BuildRoadGraph(track);
}
