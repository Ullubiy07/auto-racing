#include "cli.h"

#include <arpa/inet.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void PrintHelp(const char* program) {
    dprintf(1, "Usage: %s [OPTION]...\n", program);
    dprintf(1, "Watch a race: track, leaderboard, race status and winner.\n\n");
    dprintf(1, "  -a, --ip ADDRESS   Server IPv4 address (default: 127.0.0.1)\n");
    dprintf(1, "  -p, --port PORT    Server port, from 1 to 65535 (required)\n");
    dprintf(1, "  -h, --help         Print this help\n\n");
    dprintf(1, "Example: %s --ip 127.0.0.1 --port 8080\n", program);
    dprintf(1, "Use Ctrl+C to disconnect; the race continues.\n");
}

bool ParseCLIArgs(ViewerClientSettings* settings, int argc, char* argv[]) {
    *settings = (ViewerClientSettings){0};
    strcpy(settings->ip, "127.0.0.1");

    static struct option options[] = {
        {"ip", required_argument, NULL, 'a'},
        {"port", required_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int option;
    while ((option = getopt_long(argc, argv, "a:p:h", options, NULL)) != -1) {
        switch (option) {
        case 'a': {
            struct in_addr address;
            if (inet_pton(AF_INET, optarg, &address) != 1) {
                dprintf(2, "Invalid server IPv4 address: %s\n", optarg);
                return false;
            }

            strcpy(settings->ip, optarg);
            break;
        }
        case 'p':
            settings->port = atoi(optarg);
            if (settings->port < 1 || settings->port > 65535) {
                dprintf(2, "Server port must be between 1 and 65535\n");
                return false;
            }

            break;
        case 'h':
            PrintHelp(argv[0]);
            exit(0);
        default:
            return false;
        }
    }

    if (optind < argc) {
        dprintf(2, "Unexpected argument: %s\n", argv[optind]);
        return false;
    }

    if (settings->port == 0) {
        dprintf(2, "Specify the server port with --port PORT\n");
        return false;
    }

    return true;
}
