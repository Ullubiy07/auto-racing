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

static void UpdateLeaderBoard(Judge* judge) {
    qsort(judge->leaderBoard, judge->carCount, sizeof(Car*), CompareCars);
}

void InitJudge(Judge* judge, Car* cars, int carCount, const JudgeRules* rules) {
    judge->rules = rules;
    judge->carCount = carCount;
    judge->activeCarCount = carCount;
    judge->carsInCurrentRound = carCount;

    for (int i = 0; i < carCount; ++i) {
        judge->leaderBoard[i] = &cars[i];
    }
    UpdateLeaderBoard(judge);
}

bool IsRaceOver(const Judge* judge) {
    bool hasWinner = false;
    for (int i = 0; i < judge->carCount; ++i) {
        hasWinner |= (judge->leaderBoard[i]->laps >= judge->rules->maxLaps);
    }

    return judge->activeCarCount == 0 || hasWinner;
}

void AnnounceWinner(const Judge* judge) {
    Car* winner = judge->leaderBoard[0];
    dprintf(1, "\n\x1b[%d;1mWinner: %s, 🏎\x1b[0m\n", winner->team.color, winner->driverName);
}

void DrawLeaderBoard(Judge* judge) {
    UpdateLeaderBoard(judge);
    
    dprintf(1, "================================LEADER BOARD=================================\n");
    dprintf(1, " %-5s | %-3s | %-12s | %-12s | %-5s | %-5s | %-5s  | %-5s\n", "Place", "Car", "Team", "Driver", "Laps",
            "Lane", "Dist", "State");

    for (int i = 0; i < judge->carCount; ++i) {
        Car* car = judge->leaderBoard[i];

        dprintf(1, "   %-3d | \x1b[%d;1m%-6s\x1b[0m | %-12s | %-12s | %-5d | %-5d |  %-5d | %-5s\n", i + 1,
                car->team.color, "🏎", car->team.name, car->driverName, car->laps, car->cell->road.lane,
                car->cell->road.distToFinish, car->isOut ? "OUT" : "IN RACE");
    }
}
