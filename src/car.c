#include <stdlib.h>

#include "car.h"

void InitCar(Car* car, char* driverName, Team team, const Cell* cell) {	
	car->driverName = driverName;
	car->team = team;
	car->cell = cell;
	
	car->prevPos = (Point) {-1, -1};
	
	car->laps = 0;
	
	car->hasPassedStart = false;
	car->isBlocked = false;
	car->isOut = false;
}

static const Cell* GetNextCell(const Cell* cell, MoveType type) {
	switch (type) {
		case MOVE_FORWARD:
			return cell->road.forward;
		case MOVE_LEFT:
			return cell->road.left;
		case MOVE_RIGHT:
			return cell->road.right;
		case MOVE_NONE:
			return cell;
	}
	return NULL;
}

int GetMoveCost(const Cell* cell, MoveType type) {
	switch (type) {
		case MOVE_FORWARD:
			return 1;
		case MOVE_LEFT:
			return cell->road.lane < cell->road.left->road.lane ? 3 : 1;
		case MOVE_RIGHT:
			return cell->road.lane < cell->road.right->road.lane ? 3 : 1;
		case MOVE_NONE:
			return 0;
	}
	return 0;
}

static bool CanMove(const Cell* cell, MoveType type) {
	const Cell* newCell = GetNextCell(cell, type);
	if (!newCell) {
		return false;
	}
	
	bool currentIsTurn = IsCellTurn(cell->type);
	if ((type == MOVE_LEFT || type == MOVE_RIGHT) && currentIsTurn) {
		return false;
	}
	return IsCellFree(newCell);
}

static void MoveCarToCell(Car* car, const Cell* newCell) {
	car->prevPos = (Point) { car->cell->x, car->cell->y };
	car->cell = newCell;
}

bool MakeMove(Car* car, MoveType type) {
	if (!CanMove(car->cell, type)) {
		return false;
	}
	
	const Cell* newCell = GetNextCell(car->cell, type);
	
	// Подсчет кругов
	if (IsCellStart(newCell->type) && !IsCellStart(car->cell->type)) {
		if (car->hasPassedStart) {
			++car->laps;
		}
		car->hasPassedStart = true;
	}
	
	ClearCell((Cell*) car->cell);
	MoveCarToCell(car, newCell);
	SetCellEntity((Cell*) newCell, ENTITY_CAR, car);
	return true;
}

static size_t GetValidMoves(const Car* car, MoveType* moves, int budget) {
	MoveType types[] = { MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT };
	size_t count = 0;
	
	for (int i = 0; i < 3; ++i) {
		const Cell* nextCell = GetNextCell(car->cell, types[i]);
		if (nextCell && !(nextCell->x == car->prevPos.x && nextCell->y == car->prevPos.y) && 
			CanMove(car->cell, types[i]) && GetMoveCost(car->cell, types[i]) <= budget) 
		{
			moves[count++] = types[i];
		}
	}
	return count;
}

static MoveType GetRandomMove(const Car* car, int budget) {
	MoveType moves[3];
	size_t count = GetValidMoves(car, moves, budget);
	if (count == 0) {
		return MOVE_NONE;
	}
	return moves[rand() % count];
}

size_t BuildRoute(const Car* car, MoveType* moves, int movesLimit, int maxBudget) {
	// int budget = rand() % maxBudget + 1;
	int budget = maxBudget;
	Car dummy = *car;
	int movesDone = 0;
	dummy.prevPos = (Point) {dummy.cell->x, dummy.cell->y};
	
	while (movesDone < movesLimit && budget > 0) {
		MoveType move = GetRandomMove(&dummy, budget);
		if (move == MOVE_NONE) {
			break;
		}
		
		budget -= GetMoveCost(dummy.cell, move);
		MoveCarToCell(&dummy, GetNextCell(dummy.cell, move));
		moves[movesDone++] = move;
	}
	return movesDone;
}
