#include "cli.h"

#include <arpa/inet.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void PrintfBold(const char* text) {
    dprintf(1, "\033[1m%s\033[0m", text);
}

static void PrintHelpUsage(const char* progName) {
    dprintf(1, "Usage: %s [OPTION]...\n", progName);
    dprintf(1, "Connect a car to a race server over TCP.\n\n");
    dprintf(1, "Options:\n");

    PrintfBold("  -n, --driver NAME\n");
    dprintf(1, "         Set the driver name (required, max: %d characters)\n", MAX_DRIVER_NAME_SIZE - 1);

    PrintfBold("  -t, --team NAME\n");
    dprintf(1, "         Set the team name (required, max: %d characters)\n", MAX_TEAM_NAME_SIZE - 1);

    PrintfBold("  -c, --color N\n");
    dprintf(1, "         Set the team ANSI color code, from 31 to 37 (required)\n");

    PrintfBold("  -a, --ip ADDRESS\n");
    dprintf(1, "         Set the server IPv4 address (default: 127.0.0.1)\n");

    PrintfBold("  -p, --port PORT\n");
    dprintf(1, "         Set the server port, from 1 to 65535 (required)\n");

    PrintfBold("  -h, --help\n");
    dprintf(1, "         Print help message\n");

    dprintf(1, "\nExample:\n");
    dprintf(1, "  %s --driver Ullubiy --team \"Aston Martin\" --color 33 --ip 127.0.0.1 --port 8080\n", progName);
}

bool ParseCLIArgs(CarClientSettings* args, int argc, char* argv[]) {
    bool hasDriver = false;
    bool hasTeam = false;
    bool hasColor = false;
    bool hasPort = false;

    static struct option longOptions[] = {{"driver", required_argument, NULL, 'n'},
                                          {"team", required_argument, NULL, 't'},
                                          {"color", required_argument, NULL, 'c'},
                                          {"ip", required_argument, NULL, 'a'},
                                          {"port", required_argument, NULL, 'p'},
                                          {"help", no_argument, NULL, 'h'},
                                          {NULL, 0, NULL, 0}};
    int opt;
    memset(args, 0, sizeof(*args));
    strcpy(args->ip, "127.0.0.1");

    while ((opt = getopt_long(argc, argv, "n:t:c:a:p:h", longOptions, NULL)) != -1) {
        switch (opt) {
        case 'n':
            if (optarg[0] == '\0' || strlen(optarg) >= sizeof(args->driverName)) {
                dprintf(2, "Driver name must contain between 1 and %zu bytes\n", sizeof(args->driverName) - 1);
                return false;
            }
            strcpy(args->driverName, optarg);
            hasDriver = true;
            break;

        case 't':
            if (optarg[0] == '\0' || strlen(optarg) >= sizeof(args->team.name)) {
                dprintf(2, "Team name must contain between 1 and %zu bytes\n", sizeof(args->team.name) - 1);
                return false;
            }
            strcpy(args->team.name, optarg);
            hasTeam = true;
            break;

        case 'c':
            args->team.color = atoi(optarg);
            if (args->team.color < 31 || args->team.color > 37) {
                dprintf(2, "Color must be between 31 and 37\n");
                return false;
            }
            hasColor = true;
            break;

        case 'a': {
            struct in_addr address;
            if (inet_pton(AF_INET, optarg, &address) != 1 || strlen(optarg) >= sizeof(args->ip)) {
                dprintf(2, "Invalid server IPv4 address: %s\n", optarg);
                return false;
            }
            strcpy(args->ip, optarg);
            break;
        }

        case 'p': {
            int port = atoi(optarg);
            if (port < 1 || port > 65535) {
                dprintf(2, "Server port must be an integer between 1 and 65535\n");
                return false;
            }
            args->port = port;
            hasPort = true;
            break;
        }

        case 'h':
            PrintHelpUsage(argv[0]);
            exit(0);

        case '?':
        default:
            return false;
        }
    }

    if (optind < argc) {
        dprintf(2, "Unexpected argument: %s\n", argv[optind]);
        return false;
    }
    if (!hasDriver || !hasTeam || !hasColor || !hasPort) {
        dprintf(2, "Specify --driver, --team, --color and --port\n");
        return false;
    }
    return true;
}
