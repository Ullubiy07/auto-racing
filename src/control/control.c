#include "control.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "common.h"

static int SendControlCommand(int socket, const ControlRequest* request) {
    ClientType type = CLIENT_CONTROL;
    if (SendAll(socket, &type, sizeof(type)) == -1) {
        perror("send control client type");
        return 1;
    }

    if (SendAll(socket, request, sizeof(*request)) == -1) {
        perror("send control command");
        return 1;
    }

    ControlResponse response;
    if (RecvAll(socket, &response, sizeof(response)) == -1) {
        perror("receive control result");
        return 1;
    }

    response.message[sizeof(response.message) - 1] = '\0';
    dprintf(response.accepted ? 1 : 2, "%s\n", response.message);
    return response.accepted ? 0 : 1;
}

int RunControl(const ControlSettings* settings) {
    int connection = socket(AF_INET, SOCK_STREAM, 0);
    if (connection == -1) {
        perror("create control socket");
        return 1;
    }

    struct sockaddr_in server = {.sin_family = AF_INET, .sin_port = htons(settings->port)};
    inet_pton(AF_INET, settings->ip, &server.sin_addr);

    struct timeval timeout = {.tv_sec = 5};
    if (setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        perror("set control receive timeout");
        close(connection);
        return 1;
    }

    if (connect(connection, (const struct sockaddr*) &server, sizeof(server)) == -1) {
        perror("connect control to server");
        close(connection);
        return 1;
    }

    int result = SendControlCommand(connection, &settings->request);
    close(connection);
    return result;
}
