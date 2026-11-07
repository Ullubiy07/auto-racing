#ifndef COMMON_H
#define COMMON_H

#include <semaphore.h>
#include <stdatomic.h>
#include <pthread.h>

#include "track.h"

enum {
	MAX_CARS = 26,
	MAX_TEAMS = 7,
	MAX_TEAM_NAME_SIZE = 13,
	MAX_DRIVER_NAME_SIZE = 13
};

typedef struct {
	atomic_bool isStarted;
	atomic_bool isOver;
	atomic_bool isInterrupted;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} RaceState;

typedef struct {
    int minSleepTimeMs;
    int maxSleepTimeMs;
    int maxLaps;
} CarSettings;

typedef struct {
	int carCount;
	CarSettings carSettings;
	RaceState state;  // Состояние гонки
	bool carRegistered[MAX_CARS];
} RaceResources;

typedef struct {
	sem_t* cells[MAX_MAP_HEIGHT][MAX_MAP_WIDTH];
} CellSemaphores;

int InitCellSemaphores(CellSemaphores* semas, int racePid, bool create);

void CloseCellSemaphores(CellSemaphores* semas);

void UnlinkCellSemaphores(int racePid);

#endif
