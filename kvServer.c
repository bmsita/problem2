/*
 * Mini Key-Value Store Server using Unix Domain Sockets
 *
 * Commands supported:
 *   SET <key> <value>
 *   GET <key>
 *
 * Example client commands:
 *   SET name Rojalin
 *   GET name
 */

#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/kvstore.sock"
#define BUF_SIZE 1024
#define MAX_PAIRS 100

static int listen_fd = -1;

/* Key-Value store structure */
struct kv_pair {
    char key[64];
    char value[256];
};

static struct kv_pair store[MAX_PAIRS];
static int store_count = 0;

/* Utility: find key index */
static int find_key(const char *key) {
    for (int i = 0; i < store_count; i++) {
        if (strcmp(store[i].key, key) == 0)
            return i;
    }
    return -1;
}

/* Utility: set key-value */
static const char *set_value(const char *key, const char *value) {
    int idx = find_key(key);
    if (idx >= 0) {
        strncpy(store[idx].value, value, sizeof(store[idx].value) - 1);
        store[idx].value[sizeof(store[idx].value) - 1] = '\0';
    } else if (store_count < MAX_PAIRS) {
        strncpy(store[store_count].key, key, sizeof(store[store_count].key) - 1);
        strncpy(store[store_count].value, value, sizeof(store[store_count].value) - 1);
        store_count++;
    } else {
        return "ERROR: Store full\n";
    }
    return "OK\n";
}

/* Utility: get value */
static const char *get_value(const char *key) {
    int idx = find_key(key);
    if (idx >= 0)
        return store[idx].value;
    return "NOT FOUND\n";
}

/* Cleanup on exit */
static void cleanup(void) {
    if (listen_fd != -1)
        close(listen_fd);
    unlink(SOCKET_PATH);
}

/* Signal handler */
static void on_signal(int sig) {
    (void)sig;
    exit(0);
}

/* Read line helper */
static ssize_t read_line(int fd, char *buf, size_t maxlen) {
    size_t n = 0;
    while (n + 1 < maxlen) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 0)
            break;
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        buf[n++] = c;
        if (c == '\n') break;
    }
    buf[n] = '\0';
    return (ssize_t)n;
}

/* Write all helper */
static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len > 0) {
        ssize_t w = write(fd, p, len);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += w;
        len -= (size_t)w;
    }
    return 0;
}

/* Main server logic */
int main(void) {
    atexit(cleanup);

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "[server] Listening on %s\n", SOCKET_PATH);

    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        char buf[BUF_SIZE];
        ssize_t n = read_line(client_fd, buf, sizeof(buf));
        if (n <= 0) {
            close(client_fd);
            continue;
        }

        // Parse command
        char cmd[16], key[64], value[256];
        memset(cmd, 0, sizeof(cmd));
        memset(key, 0, sizeof(key));
        memset(value, 0, sizeof(value));

        int tokens = sscanf(buf, "%15s %63s %255[^\n]", cmd, key, value);

        char reply[512];
        if (tokens >= 2 && strcasecmp(cmd, "SET") == 0) {
            const char *res = set_value(key, value);
            snprintf(reply, sizeof(reply), "%s", res);
        } else if (tokens >= 2 && strcasecmp(cmd, "GET") == 0) {
            const char *val = get_value(key);
            snprintf(reply, sizeof(reply), "%s\n", val);
        } else {
            snprintf(reply, sizeof(reply), "ERROR: Invalid command. Use SET or GET.\n");
        }

        write_all(client_fd, reply, strlen(reply));
        close(client_fd);
    }
}
