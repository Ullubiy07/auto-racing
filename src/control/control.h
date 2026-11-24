#ifndef CONTROL_H
#define CONTROL_H

#include <netinet/in.h>

#include "control_protocol.h"

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
    ControlRequest request;
} ControlSettings;

// Подключиться, выполнить команду и закрыть соединение; 0 — успех.
int RunControl(const ControlSettings* settings);

#endif
