#include <stdlib.h>

#include "car.h"

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
	
	car->prevPos = (Point) {-1, -1};
	
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

static bool CanMove(const Car* car, const Track* track, MoveType type) {
	Point pos = GetNextPosition(car, type);
	if (pos.x < 0 || pos.x >= track->width || pos.y < 0 || pos.y >= track->height) {
		return false;
	}
	
	Cell newCell = track->map[pos.y][pos.x];
	if (!IsCellDriveable(newCell.type)) {
		return false;
	}
	
	Cell oldCell = track->map[car->pos.y][car->pos.x];
	bool currentIsTurn = IsCellTurn(oldCell.type);
	if ((type == MOVE_LEFT || type == MOVE_RIGHT) && currentIsTurn) {
		return false;
	}
	return IsCellFree(&newCell);
}

static bool MoveByType(Car* car, Track* track, MoveType type) {
	if (!CanMove(car, track, type)) {
		return false;
	}
	
	Point next = GetNextPosition(car, type);
	Cell* oldCell = &track->map[car->pos.y][car->pos.x];
	Cell* newCell = &track->map[next.y][next.x];
	
	ClearCell(oldCell);
	SetCellEntity(newCell, ENTITY_CAR, car);
	
	car->prevPos = car->pos;
	car->pos = next;
	
	if (type == MOVE_LEFT) {
		car->lane += (track->grid.isClockwise ? -1 : 1);
	} else if (type == MOVE_RIGHT) {
		car->lane += (track->grid.isClockwise ? 1 : -1);
	}
	
	if (IsCellTurn(newCell->type)) {
		car->dir = GetCellDirection(newCell->type);
	}
	return true;
}

static size_t GetValidMoves(Car* car, const Track* track, MoveType* moves) {
	MoveType types[] = { MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT };
	size_t count = 0;
	
	for (int i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
		Point nextPos = GetNextPosition(car, types[i]);
		if (!(nextPos.x == car->prevPos.x && nextPos.y == car->prevPos.y) && 
			CanMove(car, track, types[i])) 
		{
			moves[count++] = types[i];
		}
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
