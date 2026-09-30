#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "race.h"
#include "track.h"

void CreateRace(Race* r, const Track* track, const Rules* rules) {
	assert(r);
	assert(track);
	assert(rules);
	
	r->track = track;
	r->rules = rules;
	r->carCount = rules->teamCount * rules->carsInTeam;
	if (r->carCount > MAX_CARS) {
		dprintf(2, "cars limit exceeded, max: %d, got: %d\n", MAX_CARS, r->carCount);
		exit(2);
	}
	
	Team team;
	Point pos = track->start.pos;
	team.id = 1;
	
	for (int carID = 1; team.id <= rules->teamCount; ++team.id) {
		team.color = 33;
		
		for (int j = 0; j < rules->carsInTeam; ++j, ++carID) {
			if (track->map[pos.y][pos.x] == CELL_WALL) {
				dprintf(2, "track capacity exceeded, max: %d, got: %d\n", carID - 1, r->carCount);
				exit(2);
			}
			r->cars[carID - 1] = (Car) {
				.id = carID,
				.lane = track->start.lane,
				.dir = track->start.dir,
				.pos = (Point) {
					.x = pos.x,
					.y = pos.y
				},
				.team = team
			};
			pos.x -= track->start.dir.x;
			pos.y -= track->start.dir.y;
		}
	}
}

void DrawRace(Race* r) {
	assert(r);
}
