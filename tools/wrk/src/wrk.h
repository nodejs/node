#ifndef WRK_H
#define WRK_H

#include "config.h"
#include <pthread.h>
#include <inttypes.h>
#include <sys/types.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "stats.h"
#include "ae.h"
#include "script.h"
#include "http_parser.h"

#define VERSION  "3.1.0"
#define RECVBUF  8192
#define SAMPLES  100000000

#define SOCKET_TIMEOUT_MS   2000
#define CALIBRATE_DELAY_MS  500
#define TIMEOUT_INTERVAL_MS 2000

typedef struct {
    pthread_t thread;
    aeEventLoop *loop;
    uint64_t connections;
    int interval;
    uint64_t stop_at;
    uint64_t complete;
    uint64_t requests;
    uint64_t bytes;
    uint64_t start;
    uint64_t rate;
    uint64_t missed;
    stats *latency;
    tinymt64_t rand;
    lua_State *L;
    errors errors;
    struct connection *cs;
} thread;

typedef struct connection {
    thread *thread;
    http_parser parser;
    enum {
        FIELD, VALUE
    } state;
    int fd;
    SSL *ssl;
    uint64_t start;
    char *request;
    size_t length;
    size_t written;
    uint64_t pending;
    buffer headers;
    buffer body;
    char buf[RECVBUF];
} connection;

#endif /* WRK_H */
