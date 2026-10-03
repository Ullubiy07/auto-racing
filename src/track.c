#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "track.h"

static const CellInfo cells[] = {
	{ LAYOUT_WALL_HOR,       '-', "──" },
	{ LAYOUT_WALL_VERT,      '|', "│ " },
	{ LAYOUT_WALL_TOP_LEFT,  '1', "┌─" },
	{ LAYOUT_WALL_TOP_RIGHT, '2', "┐ " },
	{ LAYOUT_WALL_BOT_LEFT,  '3', "└─" },
	{ LAYOUT_WALL_BOT_RIGHT, '4', "┘ " },
	{ LAYOUT_ROAD,           '.', ". " },
	{ LAYOUT_EMPTY,          ' ', "  " },
	{ LAYOUT_TURN_RIGHT,     '>', ". " },
	{ LAYOUT_TURN_LEFT,      '<', ". " },
	{ LAYOUT_TURN_UP,        '^', ". " },
	{ LAYOUT_TURN_DOWN,      'v', ". " },
	{ LAYOUT_START_LEFT,     'L', "▓ " },
	{ LAYOUT_START_RIGHT,    'R', "▓ " },
	{ LAYOUT_START_DOWN,     'D', "▓ " },
	{ LAYOUT_START_UP,       'U', "▓ " },
	{ LAYOUT_UNKNOWN,        '?', "? " }
};

static const size_t CELL_MAP_SIZE = sizeof(cells) / sizeof(cells[0]);

static LayoutType GetCellType(char inputSymbol) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (cells[i].inputSymbol == inputSymbol) {
			return cells[i].type;
		}
	}
	return LAYOUT_UNKNOWN;
}

static void SetStart(Track* track) {
	for (int i = 0; i < track->height; ++i) {
		for (int j = 0; j < track->width; ++j) {
			LayoutType type = track->map[i][j].type;
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

bool IsCellTurn(LayoutType type) {
	return LAYOUT_TURN_RIGHT <= type && type <= LAYOUT_TURN_DOWN;
}

bool IsCellStart(LayoutType type) {
	return LAYOUT_START_RIGHT <= type && type <= LAYOUT_START_DOWN;
}

static bool IsCellDriveable(LayoutType type) {
	return type == LAYOUT_ROAD || IsCellTurn(type) || IsCellStart(type);
}

const CellInfo* GetCellInfo(LayoutType type) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (cells[i].type == type) {
			return &cells[i];
		}
	}
	return 0;
}

Point CellDirection(LayoutType type) {
	switch (type) {
		case LAYOUT_START_LEFT:
		case LAYOUT_TURN_LEFT: 
			return (Point) { -1, 0 };
		case LAYOUT_START_RIGHT:
		case LAYOUT_TURN_RIGHT: 
			return (Point) { 1, 0 };
		case LAYOUT_START_DOWN:
		case LAYOUT_TURN_DOWN: 
			return (Point) { 0, 1 };
		case LAYOUT_START_UP:
		case LAYOUT_TURN_UP: 
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
			LayoutType type = GetCellType(buf[i]);
			if (type == LAYOUT_UNKNOWN) {
				dprintf(2, "Unknown cell type: '%c'\n", buf[i]);
				exit(2);
			}
			track->map[track->height][x].type = type;
			track->map[track->height][x].clear = IsCellDriveable(type);
			++x;
		}
	}
	
	SetStart(track);
}
