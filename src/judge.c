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
        sem_t* sema = judge->carSemaphores[car->id];
        snapshots[i].car = car;

        while (sem_wait(sema) && errno == EINTR);
        Cell* cell = &judge->map[car->pos.y][car->pos.x];
        snapshots[i].laps = car->laps;
        snapshots[i].distToFinish = cell->road.distToFinish;
        snapshots[i].lane = cell->road.lane;
        snapshots[i].hasPassedStart = car->hasPassedStart;
        snapshots[i].isOut = car->isOut;
        sem_post(sema);
    }

    qsort(snapshots, judge->carCount, sizeof(CarSnapshot), CompareCars);

    for (int i = 0; i < judge->carCount; ++i) {
        judge->leaderBoard[i] = snapshots[i].car;
    }
}

void InitJudge(Judge* judge, Car* cars, sem_t** carSemaphores, int carCount, Cell** map, const JudgeRules* rules) {
    judge->rules = rules;
    judge->carCount = carCount;
    judge->carSemaphores = carSemaphores;
    judge->map = map;

    for (int i = 0; i < carCount; ++i) {
        judge->leaderBoard[i] = &cars[i];
    }
    UpdateLeaderBoard(judge);
}

bool IsRaceOver(const Judge* judge) {
    bool hasWinner = false;
    int carsInRace = 0;
    
    for (int i = 0; i < judge->carCount; ++i) {
        Car* car = judge->leaderBoard[i];
        sem_t* sema = judge->carSemaphores[car->id];

        while (sem_wait(sema) && errno == EINTR);
        hasWinner |= (car->laps >= judge->rules->maxLaps);
        sem_post(sema);
        carsInRace += !car->isOut;
    }
    return carsInRace == 0 || hasWinner;
}

int CountCarsInRace(const Judge* judge) {
    int carsInRace = 0;
    for (int i = 0; i < judge->carCount; ++i) {
        Car* car = judge->leaderBoard[i];
        carsInRace += !car->isOut;
    }
    return carsInRace;
}

void DrawLeaderBoard(Judge* judge, int fd) {
    UpdateLeaderBoard(judge);

    dprintf(fd, "================================LEADER BOARD=================================\n");
    dprintf(fd, " %-5s | %-3s | %-12s | %-12s | %-5s | %-5s | %-5s  | %-5s\n", "Place", "Car", "Team", "Driver", "Laps",
            "Lane", "Dist", "State");

    for (int i = 0; i < judge->carCount; ++i) {
        Car* car = judge->leaderBoard[i];
        sem_t* sema = judge->carSemaphores[car->id];

        while (sem_wait(sema) && errno == EINTR);
        Cell* cell = &judge->map[car->pos.y][car->pos.x];
        dprintf(fd, "   %-3d | \x1b[%d;1m%-6s\x1b[0m | %-12s | %-12s | %-5d | %-5d |  %-5d | %-5s\n", i + 1,
                car->team.color, "🏎", car->team.name, car->driverName, car->laps, cell->road.lane,
                cell->road.distToFinish, car->isOut ? "OUT" : "IN RACE");
        sem_post(sema);
    }
}
