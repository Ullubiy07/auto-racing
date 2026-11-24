#ifndef CONTROL_PROTOCOL_H
#define CONTROL_PROTOCOL_H

#include <stdbool.h>

typedef enum { CONTROL_DISCONNECT, CONTROL_RESTORE } ControlAction;

// Команда относится к ID, назначенному сервером при регистрации машины.
typedef struct {
    ControlAction action;
    int carId;
} ControlRequest;

typedef struct {
    bool accepted;
    char message[96];  // Результат команды или причина отказа
} ControlResponse;

#endif
