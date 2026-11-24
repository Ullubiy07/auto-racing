#ifndef RACE_H
#define RACE_H

#include "common.h"
#include "track.h"
#include "judge.h"
#include "viewer_protocol.h"
#include "viewer_server.h"

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
    int carCount;   // Сколько машин дождаться перед стартом

    JudgeRules rules;  // Правила гонки

    int drawDelayMs;  // Интервал снимков для viewer, мс
    int carMinDelayMs;
    int carMaxDelayMs;
    char mapFile[250];
    char logFile[250];
    bool mapOnly;  // Показать карту на сервере перед стартом
} RaceSettings;

// Итоговые данные для лога.
typedef struct {
    double duration;  // Длительность в секундах
    char* status;
    char* winner;  // Имя победителя или NULL
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

    Car cars[MAX_CARS];  // Индекс совпадает с ID машины
    Judge judge;         // Таблица лидеров для вывода и лога

    RaceStats stats;  // Статистика для сохранения лога

    atomic_bool isInterrupted;       // Запрошена остановка по SIGINT
    atomic_bool isOver;              // Новые ходы больше не принимаются
    atomic_int activeHandlersCount;  // Ещё работающие обработчики

    pthread_mutex_t mutex;          // Защищает проверку и выполнение хода целиком
    pthread_cond_t startCondition;  // Общий старт обработчиков

    bool isStarted;   // Обработчикам разрешено выполнять ходы
    RaceState state;  // Этап гонки для наблюдателя; защищён mutex
    int winnerId;     // -1, если победителя нет

    int connectedCarsCount;    // Успешно зарегистрированные машины
    int startedHandlersCount;  // Созданные потоки для pthread_join

    int socket;                          // Слушающий сокет сервера
    ClientConnection clients[MAX_CARS];  // Соединения машин по ID
    CarHandlerArgs handlerArgs[MAX_CARS];
    ViewerServer viewers;
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
