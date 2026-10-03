#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "track.h"

static const CellInfo cells[] = {
	{ CELL_WALL_HOR,       '-', "─" },
	{ CELL_WALL_VERT,      '|', "│" },
	{ CELL_WALL_TOP_LEFT,  '1', "┌" },
	{ CELL_WALL_TOP_RIGHT, '2', "┐" },
	{ CELL_WALL_BOT_LEFT,  '3', "└" },
	{ CELL_WALL_BOT_RIGHT, '4', "┘" },
	{ CELL_ROAD,           '.', "." },
	{ CELL_EMPTY,          ' ', " " },
	{ CELL_TURN_RIGHT,     '>', "." },
	{ CELL_TURN_LEFT,      '<', "." },
	{ CELL_TURN_UP,        '^', "." },
	{ CELL_TURN_DOWN,      'v', "." },
	{ CELL_START_LEFT,     'L', "▓" },
	{ CELL_START_RIGHT,    'R', "▓" },
	{ CELL_START_DOWN,     'D', "▓" },
	{ CELL_START_UP,       'U', "▓" },
	{ CELL_UNKNOWN,        '?', "?" }
};

static const size_t CELL_MAP_SIZE = sizeof(cells) / sizeof(cells[0]);

static CellType GetCellType(char inputSymbol) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (cells[i].inputSymbol == inputSymbol) {
			return cells[i].type;
		}
	}
	return CELL_UNKNOWN;
}

static void SetStart(Track* track) {
	for (int i = 0; i < track->height; ++i) {
		for (int j = 0; j < track->width; ++j) {
			CellType type = track->map[i][j];
			if (IsCellStart(type)) {
				++track->grid.laneCount;
				if (i == 1 || j == 1 || i == track->height - 2 || j == track->width - 2) {
					track->grid.pos = (Point) { j, i };
					track->grid.dir = CellDirection(type);
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

bool IsCellTurn(CellType type) {
	return CELL_TURN_RIGHT <= type && type <= CELL_TURN_DOWN;
}

bool IsCellStart(CellType type) {
	return CELL_START_RIGHT <= type && type <= CELL_START_DOWN;
}

bool IsCellDriveable(CellType type) {
	return type == CELL_ROAD || IsCellTurn(type) || IsCellStart(type);
}

const CellInfo* GetCellInfo(CellType type) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (cells[i].type == type) {
			return &cells[i];
		}
	}
	return 0;
}

Point CellDirection(CellType type) {
	switch (type) {
		case CELL_START_LEFT:
		case CELL_TURN_LEFT: 
			return (Point) { -1, 0 };
		case CELL_START_RIGHT:
		case CELL_TURN_RIGHT: 
			return (Point) { 1, 0 };
		case CELL_START_DOWN:
		case CELL_TURN_DOWN: 
			return (Point) { 0, 1 };
		case CELL_START_UP:
		case CELL_TURN_UP: 
			return (Point) { 0, -1 };
		default:
			break;
	}
	return (Point) { 0, 0 };
}

void LoadTrack(const char* fileName, Track* track) {
	const size_t bufSize = MAX_MAP_HEIGHT * MAX_MAP_WIDTH;
	char buf[bufSize];
	int count = LoadFile(fileName, buf, bufSize);
	
	track->width = 0;
	track->height = 0;
	
	for (int i = 0, x = 0; i < count; ++i) {
		if (buf[i] == '\n') {
			if (track->height == 0) {
				track->width = x;
			}
			++track->height;
			x = 0;
		} else {
			CellType type = GetCellType(buf[i]);
			if (type == CELL_UNKNOWN) {
				dprintf(2, "Unknown cell type: '%c'\n", buf[i]);
				exit(2);
			}
			track->map[track->height][x++] = type;
		}
	}
	
	SetStart(track);
}
