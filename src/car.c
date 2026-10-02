#include "car.h"
#include "track.h"

void InitCar(Car* car, char id, Team team, const Grid* grid, int slot) {	
	car->id = id;
	car->team = team;
	car->pos = grid->pos;
	car->dir = grid->dir;
	car->lane = grid->laneCount;
	
	int offsetX = grid->dir.x * (slot - 1);
	int offsetY = grid->dir.y * (slot - 1);
	
	car->pos.x -= offsetX;
	car->pos.y -= offsetY;
	
	car->is_finished = 0;
	car->is_out = 0;
}

static void Turn(Car* car, CellType type) {
	switch (type) {
		case CELL_TURN_LEFT: car->dir = (Point){-1, 0}; return;
		case CELL_TURN_RIGHT: car->dir = (Point){1, 0}; return;
		case CELL_TURN_DOWN: car->dir = (Point){0, 1}; return;
		case CELL_TURN_UP: car->dir = (Point){0, -1}; return;
		default: return;
	}
}

void Move(Car* car, Point pos, CellType type) {
	if (IsCellTurn(type)) {
		Turn(car, type);
	}
	car->pos = pos;
}
