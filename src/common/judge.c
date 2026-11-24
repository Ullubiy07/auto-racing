#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "judge.h"

static int CompareCars(const void* a, const void* b) {
    const CarStanding* standingA = a;
    const CarStanding* standingB = b;
    const Car* carA = &standingA->car;
    const Car* carB = &standingB->car;

    if (carA->isOut != carB->isOut) {
        return carA->isOut - carB->isOut;
    }

    if (!carA->hasPassedStart && carB->hasPassedStart) {
        return 1;
    }
    if (!carB->hasPassedStart && carA->hasPassedStart) {
        return -1;
    }

    if (carA->laps != carB->laps) {
        return carB->laps - carA->laps;
    }
    if (standingA->distToFinish != standingB->distToFinish) {
        return standingA->distToFinish - standingB->distToFinish;
    }

    return standingA->lane - standingB->lane;
}

void InitJudge(Judge* judge, const JudgeRules* rules) {
    judge->rules = rules;
    judge->carCount = 0;
}

void UpdateLeaderBoard(Judge* judge, const CarStanding* standings, int carCount) {
    judge->carCount = carCount;

    for (int i = 0; i < carCount; ++i) {
        judge->leaderBoard[i] = standings[i];
    }

    qsort(judge->leaderBoard, carCount, sizeof(CarStanding), CompareCars);
}

bool IsRaceOver(const Judge* judge) {
    bool hasWinner = false;
    int carsInRace = 0;

    for (int i = 0; i < judge->carCount; ++i) {
        const Car* car = &judge->leaderBoard[i].car;

        hasWinner |= (car->laps >= judge->rules->maxLaps);
        carsInRace += !car->isOut;
    }
    return carsInRace == 0 || hasWinner;
}

int CountCarsInRace(const Judge* judge) {
    int carsInRace = 0;
    for (int i = 0; i < judge->carCount; ++i) {
        const Car* car = &judge->leaderBoard[i].car;
        carsInRace += !car->isOut;
    }
    return carsInRace;
}

void DrawLeaderBoard(const Judge* judge, int fd) {
    bool color = isatty(fd);
    dprintf(fd, "================================LEADER BOARD=================================\n");
    dprintf(fd, " %-5s | %-3s | %-12s | %-12s | %-5s | %-5s | %-5s  | %-5s\n", "Place", "Car", "Team", "Driver", "Laps",
            "Lane", "Dist", "State");

    for (int i = 0; i < judge->carCount; ++i) {
        const CarStanding* standing = &judge->leaderBoard[i];
        const Car* car = &standing->car;

        dprintf(fd, "   %-3d | ", i + 1);
        if (color) {
            dprintf(fd, "\x1b[%d;1m", car->team.color);
        }

        dprintf(fd, "%-6s", "🏎");
        if (color) {
            dprintf(fd, "\x1b[0m");
        }

        dprintf(fd, " | %-12s | %-12s | %-5d | %-5d |  %-5d | %-5s%s\n", car->team.name, car->driverName, car->laps,
                standing->lane, standing->distToFinish, car->isOut ? "OUT" : "IN RACE", color ? "\x1b[K" : "");
    }
}
