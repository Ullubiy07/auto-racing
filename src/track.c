#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "track.h"

static CellType GetCellType(char symbol);
static void FindStart(Track* track);

void LoadTrack(const char* fileName, Track* track) {
	int fd = open(fileName, O_RDONLY);
	if (fd == -1) {
		perror("open track file");
		exit(2);
	}
	
	const size_t bufSize = MAX_MAP_HEIGHT * (MAX_MAP_WIDTH + 1);
	char buf[bufSize + 1];
	int count = read(fd, buf, bufSize);
	if (count == -1) {
		perror("read track file");
		close(fd);
		exit(2);
	}
	close(fd);
	buf[count] = '\0';
	
	CellType type;
	track->width = 0;
	int x = 0, y = 0;
	
	for (int i = 0; i < count; ++i) {
		if (buf[i] == '\n') {
			if (y == 0) {
				track->width = x;
			}
			++y;
			x = 0;
		} else {
			type = GetCellType(buf[i]);
			if (type == CELL_UNKNOWN) {
				dprintf(2, "unknown cell type: '%c'\n", buf[i]);
				exit(2);
			}
			track->map[y][x++] = type;
		}
	}
	track->height = y;
	FindStart(track);
}

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

const CellInfo* GetCellInfo(CellType type) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (cells[i].type == type) {
			return &cells[i];
		}
	}
	return 0;
}

static CellType GetCellType(char inputSymbol) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (cells[i].inputSymbol == inputSymbol) {
			return cells[i].type;
		}
	}
	return CELL_UNKNOWN;
}

static void FindStart(Track* track) {
	for (int i = 0; i < track->height; ++i) {
		for (int j = 0; j < track->width; ++j) {
			CellType type = track->map[i][j];
			if (type == CELL_START_LEFT || type == CELL_START_RIGHT ||
				type == CELL_START_DOWN || type == CELL_START_UP) {
				++track->start.lane;
				if (i == 1 || j == 1 || i == track->height - 2 || j == track->width - 2) {
					track->start.pos = (Point){j, i};
					switch (type) {
						case CELL_START_LEFT: track->start.dir = (Point){-1, 0}; return;
						case CELL_START_RIGHT: track->start.dir = (Point){1, 0}; return;
						case CELL_START_DOWN: track->start.dir = (Point){0, 1}; return;
						case CELL_START_UP: track->start.dir = (Point){0, -1}; return;
						default: return;
					}
				}
			}
		}
	}
}
