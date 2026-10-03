#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "track.h"

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

void LoadTrack(const char* fileName, Track* track) {
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
			track->map[track->height][x].type = type;
			SetCellEntity(&track->map[track->height][x], ENTITY_NONE, NULL);
			++x;
		}
	}
	track->grid.isClockwise = IsMovementClockwiseAt(track, track->grid.pos, track->grid.dir);
}
