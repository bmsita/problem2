/*
 * Mini Key-Value Store Client (UDS)
 *
 * Usage: ./kv_client "SET name Rojalin"
 *        ./kv_client "GET name"
 */

#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/kvstore.sock"
#define BUF_SIZE 1024

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s \"COMMAND\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1)
        die("socket");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        die("connect");

    const char *cmd = argv[1];
    if (write(fd, cmd, strlen(cmd)) < 0)
        die("write");

    if (write(fd, "\n", 1) < 0)
        die("write(newline)");

    char buf[BUF_SIZE + 1];
    ssize_t n = read(fd, buf, BUF_SIZE);
    if (n < 0)
        die("read");
    buf[n] = '\0';

    printf("[client] Server reply: %s", buf);

    close(fd);
    return 0;
}
