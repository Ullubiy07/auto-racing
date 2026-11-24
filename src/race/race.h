#ifndef RACE_H
#define RACE_H

#include "common.h"
#include "track.h"
#include "judge.h"

#include <pthread.h>
#include <stdatomic.h>
#include <netinet/in.h>

enum { MAX_CARS_IN_TEAM = 4 };

// Состав и цвет команды.
typedef struct {
    char name[MAX_TEAM_NAME_SIZE];
    char drivers[MAX_CARS_IN_TEAM][MAX_DRIVER_NAME_SIZE];
    int driverCount;
    int color;
} TeamConfig;

// Настройки гонки
typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;

    TeamConfig teams[MAX_TEAMS];
    int teamCount;  // Количество команд
    int carCount;

    JudgeRules rules;  // Правила гонки

    int drawDelayMs;
    int carMinDelayMs;
    int carMaxDelayMs;
    char mapFile[250];
    char logFile[250];
    bool mapOnly;
} RaceSettings;

// Итоговые данные для лога.
typedef struct {
    double duration;
    char* status;
    char* winner;
} RaceStats;

// Гонка и ID машины, передаваемые потоку обработки.
typedef struct {
    struct Race* race;
    int carId;
} CarHandlerArgs;

// Состояние гонки
typedef struct Race {
    const RaceSettings* settings;  // Настройки для инициализации гонки
    Track track;                   // Гоночная трасса

    Car cars[MAX_CARS];
    Judge judge;  // Таблица лидеров для вывода и лога

    RaceStats stats;  // Статистика для сохранения лога

    atomic_bool isInterrupted;
    atomic_bool isOver;
    atomic_int activeHandlersCount;  // Ещё работающие обработчики

    pthread_mutex_t mutex;  // Защищает проверку и выполнение хода целиком
    pthread_cond_t startCondition;  // Общий старт обработчиков

    bool isStarted;
    int winnerId;

    int connectedCarsCount;
    int startedHandlersCount;  // Созданные потоки для pthread_join

    int socket;
    ClientConnection clients[MAX_CARS];
    CarHandlerArgs handlerArgs[MAX_CARS];
} Race;

// Подготовить трассу и синхронизацию; 0 — успех.
int InitRace(Race* race, const RaceSettings* settings);

// Принять машины и провести гонку; 0 — завершение без ошибки.
int RunRace(Race* race);

// Завершить обработчики, закрыть сокеты и уничтожить трассу.
void DestroyRace(Race* race);

// Запросить остановку гонки.
void TerminateRace(Race* race);

// Сохранить итоговый лог после остановки обработчиков.
void SaveRaceLog(Race* race);

#endif
