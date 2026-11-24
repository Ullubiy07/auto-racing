#ifndef CAR_CLI_H
#define CAR_CLI_H

#include "car.h"

#include <stdbool.h>

// Заполнить настройки из аргументов; false при неверном вводе.
bool ParseCLIArgs(CarClientSettings* settings, int argc, char* argv[]);

#endif
