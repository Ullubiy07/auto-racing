#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "track.h"

static const CellInfo cells[] = {
	{ LAYOUT_WALL_HOR,       "-",  "──" },
	{ LAYOUT_WALL_VERT,      "|",  "│ " },
	{ LAYOUT_WALL_TOP_LEFT,  "1",  "┌─" },
	{ LAYOUT_WALL_TOP_RIGHT, "2",  "┐ " },
	{ LAYOUT_WALL_BOT_LEFT,  "3",  "└─" },
	{ LAYOUT_WALL_BOT_RIGHT, "4",  "┘ " },
	{ LAYOUT_ROAD,           ".",  ". " },
	{ LAYOUT_EMPTY,          " ",  "  " },
	{ LAYOUT_TURN_RIGHT,     ">",  ". " },
	{ LAYOUT_TURN_LEFT,      "<",  ". " },
	{ LAYOUT_TURN_UP,        "^",  ". " },
	{ LAYOUT_TURN_DOWN,      "v",  ". " },
	{ LAYOUT_START_LEFT,     "Ll", "▓ " },
	{ LAYOUT_START_RIGHT,    "Rr", "▓ " },
	{ LAYOUT_START_DOWN,     "Dd", "▓ " },
	{ LAYOUT_START_UP,       "Uu", "▓ " },
	{ LAYOUT_UNKNOWN,        "?",  "? " }
};

static const size_t CELL_MAP_SIZE = sizeof(cells) / sizeof(cells[0]);

static CellType GetCellType(char inputSymbol) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (strchr(cells[i].inSymbols, inputSymbol)) {
			return cells[i].type;
		}
	}
	return LAYOUT_UNKNOWN;
}

bool IsCellTurn(CellType type) {
	return LAYOUT_TURN_RIGHT <= type && type <= LAYOUT_TURN_DOWN;
}

bool IsCellStart(CellType type) {
	return LAYOUT_START_RIGHT <= type && type <= LAYOUT_START_DOWN;
}

static bool IsCellDriveable(CellType type) {
	return type == LAYOUT_ROAD || IsCellTurn(type) || IsCellStart(type);
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

const CellInfo* GetCellInfo(CellType type) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (cells[i].type == type) {
			return &cells[i];
		}
	}
	return 0;
}

Direction GetCellDirection(CellType type) {
	switch (type) {
		case LAYOUT_START_LEFT:
		case LAYOUT_TURN_LEFT: 
			return (Direction) { -1, 0 };
		case LAYOUT_START_RIGHT:
		case LAYOUT_TURN_RIGHT: 
			return (Direction) { 1, 0 };
		case LAYOUT_START_DOWN:
		case LAYOUT_TURN_DOWN: 
			return (Direction) { 0, 1 };
		case LAYOUT_START_UP:
		case LAYOUT_TURN_UP: 
			return (Direction) { 0, -1 };
		default:
			break;
	}
	return (Direction) { 0, 0 };
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
			track->map[track->height][x].clear = IsCellDriveable(type);
			++x;
		}
	}
	track->grid.isClockwise = IsMovementClockwiseAt(track, track->grid.pos, track->grid.dir);
}
