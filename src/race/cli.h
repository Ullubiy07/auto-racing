#ifndef CLI_H
#define CLI_H

#include "race.h"

// Заполнить настройки из аргументов; false при неверном вводе.
bool ParseCLIArgs(RaceSettings* settings, int argc, char* argv[]);

#endif
