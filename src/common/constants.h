#ifndef CONSTANTS_H
#define CONSTANTS_H

// Ограничения размеров карты и числа участников.
enum {
    MAX_MAP_HEIGHT = 40,
    MAX_MAP_WIDTH = 50,

    MAX_CARS = 26,
    MAX_TEAMS = 7,
    MAX_TEAM_NAME_SIZE = 13,  // Размеры имён включают завершающий '\0'
    MAX_DRIVER_NAME_SIZE = 13
};

#endif
