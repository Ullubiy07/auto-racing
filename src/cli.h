#ifndef CLI_H
#define CLI_H

#include "race.h"

// Парсинг аргументов командной строки в настройки гонки
bool ParseCLIArgs(RaceSettings* settings, int argc, char* argv[]);

#endif
