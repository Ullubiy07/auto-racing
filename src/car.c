#include <stdlib.h>

#include "car.h"
#include "track.h"

void InitCar(Car* car, int id, Team team, const Grid* grid, int slot) {	
	car->id = id;
	car->team = team;
	car->pos = grid->pos;
	car->dir = grid->dir;
	car->lane = grid->laneCount;
	
	int offsetX = grid->dir.dx * (slot - 1);
	int offsetY = grid->dir.dy * (slot - 1);
	
	car->pos.x -= offsetX;
	car->pos.y -= offsetY;
	
	car->is_finished = 0;
	car->is_out = 0;
}

static Point GetNextPosition(const Car* car, MoveType type) {	
	switch (type) {
		case MOVE_FORWARD:
			return (Point) {car->pos.x + car->dir.dx, car->pos.y + car->dir.dy};
		case MOVE_LEFT:
			return (Point) {car->pos.x + car->dir.dy, car->pos.y - car->dir.dx};
		case MOVE_RIGHT:
			return (Point) {car->pos.x - car->dir.dy, car->pos.y + car->dir.dx};
		default:
			return car->pos;
	}
}

static bool CanMove(Car* car, const Track* track, MoveType type) {
	Point pos = GetNextPosition(car, type);
	if (pos.x < 0 || pos.x >= track->width || pos.y < 0 || pos.y >= track->height) {
		return false;
	}
	
	bool currentIsTurn = IsCellTurn(track->map[car->pos.y][car->pos.x].type);
	if ((type == MOVE_LEFT || type == MOVE_RIGHT) && currentIsTurn) {
		return false;
	}
	return track->map[pos.y][pos.x].clear;
}

static bool MoveByType(Car* car, Track* track, MoveType type) {
	if (!CanMove(car, track, type)) {
		return false;
	}
	
	Point next = GetNextPosition(car, type);
	Cell* cell = &track->map[next.y][next.x];
	
	track->map[car->pos.y][car->pos.x].clear = true;
	cell->clear = false;
	car->pos = next;
	
	if (type == MOVE_LEFT) {
		car->lane += (track->grid.isClockwise ? -1 : 1);
	} else if (type == MOVE_RIGHT) {
		car->lane += (track->grid.isClockwise ? 1 : -1);
	}
	
	if (IsCellTurn(cell->type)) {
		car->dir = GetCellDirection(cell->type);
	}
	return true;
}

static size_t GetValidMoves(Car* car, const Track* track, MoveType* moves) {
	size_t count = 0;
	
	if (CanMove(car, track, MOVE_FORWARD)) {
		moves[count++] = MOVE_FORWARD;
	}
	if (CanMove(car, track, MOVE_LEFT)) {
		moves[count++] = MOVE_LEFT;
	}
	if (CanMove(car, track, MOVE_RIGHT)) {
		moves[count++] = MOVE_RIGHT;
	}
	return count;
}

bool MakeRandomMove(Car* car, Track* track) {
	MoveType moves[3];
	size_t count = GetValidMoves(car, track, moves);
	if (count == 0) {
		return false;
	}
	int num = rand() % count;
	return MoveByType(car, track, moves[num]);
}

bool MoveCar(Car* car, Track* track) {
	return MakeRandomMove(car, track);
}
