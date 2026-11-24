#include "cli.h"

#include <arpa/inet.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void PrintHelp(const char* program) {
    dprintf(1, "Usage: %s --port PORT [--ip ADDRESS] (--disconnect ID | --restore ID)\n", program);
    dprintf(1, "  --disconnect ID   Disconnect a car and keep its race state\n");
    dprintf(1, "  --restore ID      Let the car reconnect and continue\n");
    dprintf(1, "  --ip ADDRESS      Server IPv4 address (default: 127.0.0.1)\n");
    dprintf(1, "  --port PORT       Server port (required)\n");
    dprintf(1, "  --help            Print this help\n");
}

bool ParseCLIArgs(ControlSettings* settings, int argc, char* argv[]) {
    *settings = (ControlSettings){0};
    strcpy(settings->ip, "127.0.0.1");
    bool hasAction = false;

    static struct option options[] = {{"ip", required_argument, NULL, 'a'},
                                      {"port", required_argument, NULL, 'p'},
                                      {"disconnect", required_argument, NULL, 'd'},
                                      {"restore", required_argument, NULL, 'r'},
                                      {"help", no_argument, NULL, 'h'},
                                      {NULL, 0, NULL, 0}};

    int option;
    while ((option = getopt_long(argc, argv, "a:p:d:r:h", options, NULL)) != -1) {
        switch (option) {
        case 'a': {
            struct in_addr address;
            if (inet_pton(AF_INET, optarg, &address) != 1) {
                dprintf(2, "Invalid server IPv4 address\n");
                return false;
            }

            strcpy(settings->ip, optarg);
            break;
        }
        case 'p':
            settings->port = atoi(optarg);
            if (settings->port < 1 || settings->port > 65535) {
                dprintf(2, "Port must be between 1 and 65535\n");
                return false;
            }

            break;
        case 'd':
        case 'r':
            if (hasAction || optarg[0] == '\0' || strspn(optarg, "0123456789") != strlen(optarg)) {
                dprintf(2, "Specify one action and a non-negative car ID\n");
                return false;
            }

            settings->request.action = option == 'd' ? CONTROL_DISCONNECT : CONTROL_RESTORE;
            settings->request.carId = atoi(optarg);
            if (settings->request.carId < 0 || settings->request.carId >= MAX_CARS) {
                dprintf(2, "Car ID must be between 0 and %d\n", MAX_CARS - 1);
                return false;
            }

            hasAction = true;
            break;
        case 'h':
            PrintHelp(argv[0]);
            exit(0);
        default:
            return false;
        }
    }

    if (!hasAction || settings->port == 0 || optind < argc) {
        dprintf(2, "Specify --port and either --disconnect ID or --restore ID\n");
        return false;
    }

    return true;
}
