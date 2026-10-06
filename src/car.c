#include <stdlib.h>

#include "car.h"

void InitCar(Car* car, int id, Team team, Cell* cell) {	
	car->id = id;
	car->team = team;
	car->cell = cell;
	
	car->prevPos = (Point) {-1, -1};
	
	car->laps = 0;
	
	car->hasPassedStart = false;
	car->isOut = false;
}

static Cell* GetNextPosition(const Car* car, MoveType type) {
	switch (type) {
		case MOVE_FORWARD:
			return car->cell->road.forward;
		case MOVE_LEFT:
			return car->cell->road.left;
		case MOVE_RIGHT:
			return car->cell->road.right;
	}
	return NULL;
}

static bool CanMove(const Car* car, MoveType type) {
	Cell* newCell = GetNextPosition(car, type);
	if (!newCell) {
		return false;
	}
	
	bool currentIsTurn = IsCellTurn(car->cell->type);
	if ((type == MOVE_LEFT || type == MOVE_RIGHT) && currentIsTurn) {
		return false;
	}
	return IsCellFree(newCell);
}

static bool MoveByType(Car* car, MoveType type) {
	if (!CanMove(car, type)) {
		return false;
	}
	
	Cell* newCell = GetNextPosition(car, type);
	
	ClearCell(car->cell);
	SetCellEntity(newCell, ENTITY_CAR, car);
	
	// Подсчет кругов
	if (IsCellStart(newCell->type) && !IsCellStart(car->cell->type)) {
		if (car->hasPassedStart) {
			++car->laps;
		}
		car->hasPassedStart = true;
	}
	
	car->prevPos = (Point) {car->cell->x, car->cell->y};
	car->cell = newCell;
	
	return true;
}

size_t GetValidMoves(Car* car, MoveType* moves) {
	MoveType types[] = { MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT };
	size_t count = 0;
	
	for (int i = 0; i < 3; ++i) {
		Cell* nextPos = GetNextPosition(car, types[i]);
		if (nextPos && !(nextPos->x == car->prevPos.x && nextPos->y == car->prevPos.y) && 
			CanMove(car, types[i])) 
		{
			moves[count++] = types[i];
		}
	}
	return count;
}

bool MakeRandomMove(Car* car) {
	MoveType moves[3];
	size_t count = GetValidMoves(car, moves);
	if (count == 0) {
		return false;
	}
	int num = rand() % count;
	return MoveByType(car, moves[num]);
}

bool MoveCar(Car* car) {
	return MakeRandomMove(car);
}
