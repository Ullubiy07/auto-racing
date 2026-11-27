#include "car.h"
#include "cli.h"

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>

static CarClient client = {.socket = -1};

static void CarClientSignalHandler(int sig) {
    if (sig == SIGINT) {
        int savedErrno = errno;
        client.isInterrupted = 1;
        shutdown(client.socket, SHUT_RDWR);
        errno = savedErrno;
    }
}

int main(int argc, char* argv[]) {
    CarClientSettings settings;
    if (!ParseCLIArgs(&settings, argc, argv)) {
        return 1;
    }

    struct sigaction action = {0};
    action.sa_handler = CarClientSignalHandler;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("set car signal handler");
        DestroyCarClient(&client);
        return 1;
    }

    int result = 0;
    while (!client.isInterrupted) {
        result = InitCarClient(&client, &settings);
        if (result == 0) {
            result = RunCarClient(&client);
        }

        if (result != 0 || !client.isDisconnected) {
            break;
        }

        settings.resumeId = client.car.id;
        DestroyCarClient(&client);
    }

    action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &action, NULL);

    DestroyCarClient(&client);
    return client.isInterrupted ? 0 : result;
}
