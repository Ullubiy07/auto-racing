#ifndef VIEWER_CLI_H
#define VIEWER_CLI_H

#include "viewer.h"

// Разобрать IP и порт сервера; false — ошибка аргументов.
bool ParseCLIArgs(ViewerClientSettings* settings, int argc, char* argv[]);

#endif
