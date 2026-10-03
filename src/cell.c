#include <string.h>

#include "cell.h"

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

CellType GetCellType(char inputSymbol) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (strchr(cells[i].inSymbols, inputSymbol)) {
			return cells[i].type;
		}
	}
	return LAYOUT_UNKNOWN;
}

const CellInfo* GetCellInfo(CellType type) {
	for (size_t i = 0; i < CELL_MAP_SIZE; ++i) {
		if (cells[i].type == type) {
			return &cells[i];
		}
	}
	return NULL;
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

bool IsCellTurn(CellType type) {
	return LAYOUT_TURN_RIGHT <= type && type <= LAYOUT_TURN_DOWN;
}

bool IsCellStart(CellType type) {
	return LAYOUT_START_RIGHT <= type && type <= LAYOUT_START_DOWN;
}

bool IsCellDriveable(CellType type) {
	return type == LAYOUT_ROAD || IsCellTurn(type) || IsCellStart(type);
}

bool IsCellFree(const Cell* cell) {
	return cell->entity.type == ENTITY_NONE;
}

void ClearCell(Cell* cell) {
	cell->entity.data = NULL;
	cell->entity.type = ENTITY_NONE;
}

void SetCellEntity(Cell* cell, EntityType type, void* data) {
	cell->entity.data = data;
	cell->entity.type = type;
}
