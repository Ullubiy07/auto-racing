#include <fcntl.h>
#include <stdio.h>
#include "track.h"

#include "common.h"

int InitCellSemaphores(CellSemaphores* semas, int racePid, bool create) {
    char name[40];

    for (int i = 0; i < MAX_MAP_HEIGHT; ++i) {
        for (int j = 0; j < MAX_MAP_WIDTH; ++j) {
            snprintf(name, sizeof(name), "cell-%d-%d-%d", racePid, i, j);
            if (create) {
                semas->cells[i][j] = sem_open(name, O_CREAT | O_EXCL, 0666, 1);
            } else {
                semas->cells[i][j] = sem_open(name, 0);
            }

            if (semas->cells[i][j] == SEM_FAILED) {
                perror("Open cell semaphore failed");

                int opened = i * MAX_MAP_WIDTH + j;
                int count = 0;
                for (int k = 0; k < MAX_MAP_HEIGHT && count < opened; ++k) {
                    for (int t = 0; t < MAX_MAP_WIDTH && count < opened; ++t, ++count) {
                        if (sem_close(semas->cells[k][t])) {
                            perror("Close cell semaphore failed");
                        }

                        snprintf(name, sizeof(name), "cell-%d-%d-%d", racePid, k, t);
                        if (create && sem_unlink(name)) {
                            perror("Unlink cell semaphore failed");
                        }
                    }
                }
                return 1;
            }
        }
    }
    return 0;
}

void CloseCellSemaphores(CellSemaphores* semas) {
    for (int i = 0; i < MAX_MAP_HEIGHT; ++i) {
        for (int j = 0; j < MAX_MAP_WIDTH; ++j) {
            if (sem_close(semas->cells[i][j])) {
                perror("Close cell semaphore failed");
            }
        }
    }
}

void UnlinkCellSemaphores(int racePid) {
    char name[40];
    for (int i = 0; i < MAX_MAP_HEIGHT; ++i) {
        for (int j = 0; j < MAX_MAP_WIDTH; ++j) {
            snprintf(name, sizeof(name), "cell-%d-%d-%d", racePid, i, j);
            if (sem_unlink(name)) {
                perror("Unlink cell semaphore failed");
            }
        }
    }
}
