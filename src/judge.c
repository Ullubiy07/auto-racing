#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#include "judge.h"

typedef struct {
    Car* car;
    int laps;
    int distToFinish;
    int lane;
    bool hasPassedStart;
    bool isOut;
} CarSnapshot;

static int CompareCars(const void* a, const void* b) {
    CarSnapshot* carA = (CarSnapshot*) a;
    CarSnapshot* carB = (CarSnapshot*) b;

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
    if (carA->distToFinish != carB->distToFinish) {
        return carA->distToFinish - carB->distToFinish;
    }
    return carA->lane - carB->lane;
}

static void UpdateLeaderBoard(Judge* judge) {
    CarSnapshot snapshots[MAX_CARS];
    for (int i = 0; i < judge->carCount; ++i) {
        Car* car = judge->leaderBoard[i];
        snapshots[i].car = car;

        while (sem_wait(&car->sema) && errno == EINTR);
        snapshots[i].laps = car->laps;
        snapshots[i].distToFinish = car->cell->road.distToFinish;
        snapshots[i].lane = car->cell->road.lane;
        snapshots[i].hasPassedStart = car->hasPassedStart;
        snapshots[i].isOut = car->isOut;
        sem_post(&car->sema);
    }

    qsort(snapshots, judge->carCount, sizeof(CarSnapshot), CompareCars);

    for (int i = 0; i < judge->carCount; ++i) {
        judge->leaderBoard[i] = snapshots[i].car;
    }
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
        Car* car = judge->leaderBoard[i];

        while (sem_wait(&car->sema) && errno == EINTR);
        hasWinner |= (car->laps >= judge->rules->maxLaps);
        sem_post(&car->sema);
    }
    return judge->activeCarCount == 0 || hasWinner;
}

void DrawLeaderBoard(Judge* judge, int fd) {
    UpdateLeaderBoard(judge);

    dprintf(fd, "================================LEADER BOARD=================================\n");
    dprintf(fd, " %-5s | %-3s | %-12s | %-12s | %-5s | %-5s | %-5s  | %-5s\n", "Place", "Car", "Team", "Driver", "Laps",
            "Lane", "Dist", "State");

    for (int i = 0; i < judge->carCount; ++i) {
        Car* car = judge->leaderBoard[i];

        while (sem_wait(&car->sema) && errno == EINTR);
        dprintf(fd, "   %-3d | \x1b[%d;1m%-6s\x1b[0m | %-12s | %-12s | %-5d | %-5d |  %-5d | %-5s\n", i + 1,
                car->team.color, "🏎", car->team.name, car->driverName, car->laps, car->cell->road.lane,
                car->cell->road.distToFinish, car->isOut ? "OUT" : "IN RACE");
        sem_post(&car->sema);
    }
}
