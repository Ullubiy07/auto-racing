#ifndef CONTROL_CLI_H
#define CONTROL_CLI_H

#include "control.h"

// Разобрать адрес сервера и одну команду; false — ошибка аргументов.
bool ParseCLIArgs(ControlSettings* settings, int argc, char* argv[]);

#endif
