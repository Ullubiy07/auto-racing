#include <assert.h>

#include "judge.h"

int main(void) {
    JudgeRules rules = {.maxLaps = 3};
    Judge judge;
    InitJudge(&judge, &rules);

    assert(judge.carCount == 0);
    assert(CountCarsInRace(&judge) == 0);

    CarStanding standings[] = {
        {.car = {.id = 0, .laps = 2, .hasPassedStart = true}, .lane = 2, .distToFinish = 10},
        {.car = {.id = 1, .laps = 2, .hasPassedStart = true}, .lane = 1, .distToFinish = 10},
        {.car = {.id = 2, .laps = 2, .hasPassedStart = true}, .lane = 3, .distToFinish = 5},
        {.car = {.id = 3, .laps = 1, .hasPassedStart = true}, .lane = 0, .distToFinish = 0},
        {.car = {.id = 4, .laps = 2}, .lane = 0, .distToFinish = 0},
        {.car = {.id = 5, .laps = 2, .hasPassedStart = true, .isOut = true}, .lane = 0, .distToFinish = 0}};

    UpdateLeaderBoard(&judge, standings, 6);

    const int expected[] = {2, 1, 0, 3, 4, 5};
    for (int i = 0; i < 6; ++i) {
        assert(judge.leaderBoard[i].car.id == expected[i]);
        assert(standings[i].car.id == i);
    }

    assert(CountCarsInRace(&judge) == 5);
    assert(!IsRaceOver(&judge));

    standings[0].car.laps = 3;
    assert(!IsRaceOver(&judge));  // Сохранённый снимок не изменился.

    UpdateLeaderBoard(&judge, standings, 6);
    assert(IsRaceOver(&judge));
    assert(judge.leaderBoard[0].car.id == 0);

    standings[0].car.laps = 0;
    standings[0].car.isOut = true;
    UpdateLeaderBoard(&judge, standings, 1);
    assert(CountCarsInRace(&judge) == 0);
    assert(IsRaceOver(&judge));

    UpdateLeaderBoard(&judge, NULL, 0);
    assert(judge.carCount == 0);
    return 0;
}
