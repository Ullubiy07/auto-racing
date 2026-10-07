#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "judge.h"
#include "cell.h"

static int CompareCars(const void* a, const void* b) {
    const Car* carA = *(const Car**) a;
    const Car* carB = *(const Car**) b;

    const Road* roadA = &carA->cell->road;
    const Road* roadB = &carB->cell->road;

    if (carA->isOut != carB->isOut) {
        return carA->isOut - carB->isOut;
    }

    if ((!carA->hasPassedStart && carB->hasPassedStart)) {
        return 1;
    }
    if ((!carB->hasPassedStart && carA->hasPassedStart)) {
        return -1;
    }

    if (carA->laps != carB->laps) {
        return carB->laps - carA->laps;
    }
    if (roadA->distToFinish != roadB->distToFinish) {
        return roadA->distToFinish - roadB->distToFinish;
    }
    return roadA->lane - roadB->lane;
}

static void UpdateLeaderBoard(Judge* judge) { qsort(judge->leaderBoard, judge->carCount, sizeof(Car*), CompareCars); }

void InitJudge(Judge* judge, Car* cars, int carCount, const JudgeRules* rules) {
    judge->rules = rules;
    judge->carCount = carCount;
    judge->activeCarCount = carCount;
    judge->carsInCurrentRound = carCount;

    for (int i = 0; i < carCount; ++i) {
        judge->leaderBoard[i] = &cars[i];
    }
    UpdateLeaderBoard(judge);

    judge->turn = 0;
    judge->prevMoveCost = 0;
    judge->roundsPlayed = 0;
}

int GetCurrentCarBudget(const Judge* judge, int prevMoveCost) {
    if (judge->turn == 0) {
        return 4;
    }
    if (judge->turn == 1) {
        return prevMoveCost + 2;
    }
    return prevMoveCost + 1;
}

Car* GetCurrentCar(const Judge* judge) {
    assert(judge->turn >= 0 && judge->turn < judge->carsInCurrentRound);
    assert(!judge->leaderBoard[judge->turn]->isOut);

    return judge->leaderBoard[judge->turn];
}

void NextTurn(Judge* judge) { ++judge->turn; }

void RegisterZeroMove(Judge* judge) {
    Car* car = GetCurrentCar(judge);

    assert(car);
    assert(!car->isOut);
    assert(!car->isBlocked);
    assert(judge->activeCarCount);

    car->isBlocked = true;
    --judge->activeCarCount;
}

void StartRound(Judge* judge) {
    judge->turn = 0;
    judge->prevMoveCost = 0;
    judge->carsInCurrentRound = judge->activeCarCount;
}

void EndRound(Judge* judge) {
    ++judge->roundsPlayed;
    for (int i = 0; i < judge->carsInCurrentRound; ++i) {
        Car* car = judge->leaderBoard[i];
        if (car->isBlocked) {
            car->isOut = true;
            ClearCell((Cell*) car->cell);
        }
    }
    UpdateLeaderBoard(judge);
}

bool IsRoundOver(const Judge* judge) { return judge->turn >= judge->carsInCurrentRound; }

bool IsRaceOver(const Judge* judge) {
    bool hasWinner = false;
    for (int i = 0; i < judge->carCount; ++i) {
        hasWinner |= (judge->leaderBoard[i]->laps >= judge->rules->maxLaps);
    }

    return judge->activeCarCount == 0 || judge->roundsPlayed >= judge->rules->maxRounds || hasWinner;
}

void AnnounceWinner(const Judge* judge) {
    Car* winner = judge->leaderBoard[0];
    dprintf(1, "\n\x1b[%d;1mWinner: %s, 🏎\x1b[0m\n", winner->team.color, winner->driverName);
}

void DrawLeaderBoard(const Judge* judge) {
    dprintf(1, "================================LEADER BOARD=================================\n");
    dprintf(1, " %-5s | %-3s | %-12s | %-12s | %-5s | %-5s | %-5s  | %-5s\n", "Place", "Car", "Team", "Driver", "Laps",
            "Lane", "Dist", "State");

    for (int i = 0; i < judge->carCount; ++i) {
        Car* car = judge->leaderBoard[i];

        dprintf(1, "   %-3d | \x1b[%d;1m%-6s\x1b[0m | %-12s | %-12s | %-5d | %-5d |  %-5d | %-5s", i + 1,
                car->team.color, "🏎", car->team.name, car->driverName, car->laps, car->cell->road.lane,
                car->cell->road.distToFinish,
                car->isBlocked && !car->isOut  ? "BLOCKED"
                : car->isBlocked && car->isOut ? "OUT"
                                               : "IN RACE");

        if (i == judge->turn && !IsRoundOver(judge)) {
            dprintf(1, " <-");
        }
        dprintf(1, "\n");
    }
    dprintf(1, "\nRounds played: %d\n", judge->roundsPlayed);
}
