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

void Move(Car* car, Point pos, CellType type) {
	if (IsCellTurn(type)) {
		car->dir = CellDirection(type);
	}
	car->pos = pos;
}
