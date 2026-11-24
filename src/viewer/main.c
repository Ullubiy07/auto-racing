#include "viewer.h"
#include "cli.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>

static ViewerClient client = {.socket = -1};

static void ViewerSignalHandler(int signal) {
    (void) signal;
    int savedErrno = errno;
    client.isInterrupted = 1;
    shutdown(client.socket, SHUT_RDWR);
    errno = savedErrno;
}

int main(int argc, char* argv[]) {
    ViewerClientSettings settings;
    if (!ParseCLIArgs(&settings, argc, argv)) {
        return 1;
    }

    struct sigaction action = {0};
    action.sa_handler = ViewerSignalHandler;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("set viewer signal handler");
        return 1;
    }

    if (sigaction(SIGTERM, &action, NULL) == -1) {
        perror("set viewer signal handler");
        return 1;
    }

    int result = InitViewerClient(&client, &settings);
    if (result == 0) {
        result = RunViewerClient(&client);
    }

    action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    DestroyViewerClient(&client);
    return client.isInterrupted ? 0 : result;
}
