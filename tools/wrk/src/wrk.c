// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include "wrk.h"
#include "main.h"

static struct config {
    struct addrinfo addr;
    uint64_t threads;
    uint64_t connections;
    uint64_t duration;
    uint64_t timeout;
    uint64_t pipeline;
    bool     latency;
    bool     dynamic;
    char    *script;
    SSL_CTX *ctx;
} cfg;

static struct {
    stats *latency;
    stats *requests;
    pthread_mutex_t mutex;
} statistics;

static struct sock sock = {
    .connect  = sock_connect,
    .close    = sock_close,
    .read     = sock_read,
    .write    = sock_write,
    .readable = sock_readable
};

static struct http_parser_settings parser_settings = {
    .on_message_complete = response_complete
};

static volatile sig_atomic_t stop = 0;

static void handler(int sig) {
    stop = 1;
}

static void usage() {
    printf("Usage: wrk <options> <url>                            \n"
           "  Options:                                            \n"
           "    -c, --connections <N>  Connections to keep open   \n"
           "    -d, --duration    <T>  Duration of test           \n"
           "    -t, --threads     <N>  Number of threads to use   \n"
           "                                                      \n"
           "    -s, --script      <S>  Load Lua script file       \n"
           "    -H, --header      <H>  Add header to request      \n"
           "        --latency          Print latency statistics   \n"
           "        --timeout     <T>  Socket/request timeout     \n"
           "    -v, --version          Print version details      \n"
           "                                                      \n"
           "  Numeric arguments may include a SI unit (1k, 1M, 1G)\n"
           "  Time arguments may include a time unit (2s, 2m, 2h)\n");
}

int main(int argc, char **argv) {
    struct addrinfo *addrs, *addr;
    struct http_parser_url parser_url;
    char *url, **headers;
    int rc;

    headers = zmalloc((argc / 2) * sizeof(char *));

    if (parse_args(&cfg, &url, headers, argc, argv)) {
        usage();
        exit(1);
    }

    if (http_parser_parse_url(url, strlen(url), 0, &parser_url)) {
        fprintf(stderr, "invalid URL: %s\n", url);
        exit(1);
    }

    char *schema  = extract_url_part(url, &parser_url, UF_SCHEMA);
    char *host    = extract_url_part(url, &parser_url, UF_HOST);
    char *port    = extract_url_part(url, &parser_url, UF_PORT);
    char *service = port ? port : schema;
    char *path    = "/";

    if (parser_url.field_set & (1 << UF_PATH)) {
        path = &url[parser_url.field_data[UF_PATH].off];
    }

    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    if ((rc = getaddrinfo(host, service, &hints, &addrs)) != 0) {
        const char *msg = gai_strerror(rc);
        fprintf(stderr, "unable to resolve %s:%s %s\n", host, service, msg);
        exit(1);
    }

    for (addr = addrs; addr != NULL; addr = addr->ai_next) {
        int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd == -1) continue;
        rc = connect(fd, addr->ai_addr, addr->ai_addrlen);
        close(fd);
        if (rc == 0) break;
    }

    if (addr == NULL) {
        char *msg = strerror(errno);
        fprintf(stderr, "unable to connect to %s:%s %s\n", host, service, msg);
        exit(1);
    }

    if (!strncmp("https", schema, 5)) {
        if ((cfg.ctx = ssl_init()) == NULL) {
            fprintf(stderr, "unable to initialize SSL\n");
            ERR_print_errors_fp(stderr);
            exit(1);
        }
        sock.connect  = ssl_connect;
        sock.close    = ssl_close;
        sock.read     = ssl_read;
        sock.write    = ssl_write;
        sock.readable = ssl_readable;
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  SIG_IGN);
    cfg.addr = *addr;

    pthread_mutex_init(&statistics.mutex, NULL);
    statistics.latency  = stats_alloc(SAMPLES);
    statistics.requests = stats_alloc(SAMPLES);

    thread *threads = zcalloc(cfg.threads * sizeof(thread));
    uint64_t connections = cfg.connections / cfg.threads;
    uint64_t stop_at     = time_us() + (cfg.duration * 1000000);

    for (uint64_t i = 0; i < cfg.threads; i++) {
        thread *t = &threads[i];
        t->loop        = aeCreateEventLoop(10 + cfg.connections * 3);
        t->connections = connections;
        t->stop_at     = stop_at;

        t->L = script_create(schema, host, port, path);
        script_headers(t->L, headers);
        script_init(t->L, cfg.script, argc - optind, &argv[optind]);

        if (i == 0) {
            cfg.pipeline = script_verify_request(t->L);
            cfg.dynamic = !script_is_static(t->L);
            if (script_want_response(t->L)) {
                parser_settings.on_header_field = header_field;
                parser_settings.on_header_value = header_value;
                parser_settings.on_body         = response_body;
            }
        }

        if (!t->loop || pthread_create(&t->thread, NULL, &thread_main, t)) {
            char *msg = strerror(errno);
            fprintf(stderr, "unable to create thread %"PRIu64": %s\n", i, msg);
            exit(2);
        }
    }

    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = 0,
    };
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    char *time = format_time_s(cfg.duration);
    printf("Running %s test @ %s\n", time, url);
    printf("  %"PRIu64" threads and %"PRIu64" connections\n", cfg.threads, cfg.connections);

    uint64_t start    = time_us();
    uint64_t complete = 0;
    uint64_t bytes    = 0;
    errors errors     = { 0 };

    for (uint64_t i = 0; i < cfg.threads; i++) {
        thread *t = &threads[i];
        pthread_join(t->thread, NULL);

        complete += t->complete;
        bytes    += t->bytes;

        errors.connect += t->errors.connect;
        errors.read    += t->errors.read;
        errors.write   += t->errors.write;
        errors.timeout += t->errors.timeout;
        errors.status  += t->errors.status;
    }

    uint64_t runtime_us = time_us() - start;
    long double runtime_s   = runtime_us / 1000000.0;
    long double req_per_s   = complete   / runtime_s;
    long double bytes_per_s = bytes      / runtime_s;

    print_stats_header();
    print_stats("Latency", statistics.latency, format_time_us);
    print_stats("Req/Sec", statistics.requests, format_metric);
    if (cfg.latency) print_stats_latency(statistics.latency);

    char *runtime_msg = format_time_us(runtime_us);

    printf("  %"PRIu64" requests in %s, %sB read\n", complete, runtime_msg, format_binary(bytes));
    if (errors.connect || errors.read || errors.write || errors.timeout) {
        printf("  Socket errors: connect %d, read %d, write %d, timeout %d\n",
               errors.connect, errors.read, errors.write, errors.timeout);
    }

    if (errors.status) {
        printf("  Non-2xx or 3xx responses: %d\n", errors.status);
    }

    printf("Requests/sec: %9.2Lf\n", req_per_s);
    printf("Transfer/sec: %10sB\n", format_binary(bytes_per_s));

    lua_State *L = threads[0].L;
    if (script_has_done(L)) {
        script_summary(L, runtime_us, complete, bytes);
        script_errors(L, &errors);
        script_done(L, statistics.latency, statistics.requests);
    }

    return 0;
}

void *thread_main(void *arg) {
    thread *thread = arg;
    aeEventLoop *loop = thread->loop;

    thread->cs = zmalloc(thread->connections * sizeof(connection));
    tinymt64_init(&thread->rand, time_us());
    thread->latency = stats_alloc(100000);

    char *request = NULL;
    size_t length = 0;

    if (!cfg.dynamic) {
        script_request(thread->L, &request, &length);
    }

    connection *c = thread->cs;

    for (uint64_t i = 0; i < thread->connections; i++, c++) {
        c->thread = thread;
        c->ssl     = cfg.ctx ? SSL_new(cfg.ctx) : NULL;
        c->request = request;
        c->length  = length;
        connect_socket(thread, c);
    }

    aeCreateTimeEvent(loop, CALIBRATE_DELAY_MS, calibrate, thread, NULL);
    aeCreateTimeEvent(loop, TIMEOUT_INTERVAL_MS, check_timeouts, thread, NULL);

    thread->start = time_us();
    aeMain(loop);

    aeDeleteEventLoop(loop);
    zfree(thread->cs);

    uint64_t max = thread->latency->max;
    stats_free(thread->latency);

    pthread_mutex_lock(&statistics.mutex);
    for (uint64_t i = 0; i < thread->missed; i++) {
        stats_record(statistics.latency, max);
    }
    pthread_mutex_unlock(&statistics.mutex);

    return NULL;
}

static int connect_socket(thread *thread, connection *c) {
    struct addrinfo addr = cfg.addr;
    struct aeEventLoop *loop = thread->loop;
    int fd, flags;

    fd = socket(addr.ai_family, addr.ai_socktype, addr.ai_protocol);

    flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    if (connect(fd, addr.ai_addr, addr.ai_addrlen) == -1) {
        if (errno != EINPROGRESS) goto error;
    }

    flags = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));

    flags = AE_READABLE | AE_WRITABLE;
    if (aeCreateFileEvent(loop, fd, flags, socket_connected, c) == AE_OK) {
        c->parser.data = c;
        c->fd = fd;
        return fd;
    }

  error:
    thread->errors.connect++;
    close(fd);
    return -1;
}

static int reconnect_socket(thread *thread, connection *c) {
    aeDeleteFileEvent(thread->loop, c->fd, AE_WRITABLE | AE_READABLE);
    sock.close(c);
    close(c->fd);
    return connect_socket(thread, c);
}

static int calibrate(aeEventLoop *loop, long long id, void *data) {
    thread *thread = data;

    (void) stats_summarize(thread->latency);
    long double latency = stats_percentile(thread->latency, 90.0) / 1000.0L;
    long double interval = MAX(latency * 2, 10);
    long double rate = (interval / latency) * thread->connections;

    if (latency == 0) return CALIBRATE_DELAY_MS;

    thread->interval = interval;
    thread->rate     = ceil(rate / 10);
    thread->start    = time_us();
    thread->requests = 0;
    stats_reset(thread->latency);

    aeCreateTimeEvent(loop, thread->interval, sample_rate, thread, NULL);

    return AE_NOMORE;
}

static int check_timeouts(aeEventLoop *loop, long long id, void *data) {
    thread *thread = data;
    connection *c  = thread->cs;
    uint64_t now   = time_us();

    uint64_t maxAge = now - (cfg.timeout * 1000);

    for (uint64_t i = 0; i < thread->connections; i++, c++) {
        if (maxAge > c->start) {
            thread->errors.timeout++;
        }
    }

    if (stop || now >= thread->stop_at) {
        aeStop(loop);
    }

    return TIMEOUT_INTERVAL_MS;
}

static int sample_rate(aeEventLoop *loop, long long id, void *data) {
    thread *thread = data;

    uint64_t elapsed_ms = (time_us() - thread->start) / 1000;
    uint64_t requests = (thread->requests / (double) elapsed_ms) * 1000;
    uint64_t missed = thread->rate - MIN(thread->rate, thread->latency->limit);
    uint64_t count = thread->rate - missed;

    pthread_mutex_lock(&statistics.mutex);
    stats_sample(statistics.latency, &thread->rand, count, thread->latency);
    stats_record(statistics.requests, requests);
    pthread_mutex_unlock(&statistics.mutex);

    thread->missed  += missed;
    thread->requests = 0;
    thread->start    = time_us();
    stats_rewind(thread->latency);

    return thread->interval;
}

static int header_field(http_parser *parser, const char *at, size_t len) {
    connection *c = parser->data;
    if (c->state == VALUE) {
        *c->headers.cursor++ = '\0';
        c->state = FIELD;
    }
    buffer_append(&c->headers, at, len);
    return 0;
}

static int header_value(http_parser *parser, const char *at, size_t len) {
    connection *c = parser->data;
    if (c->state == FIELD) {
        *c->headers.cursor++ = '\0';
        c->state = VALUE;
    }
    buffer_append(&c->headers, at, len);
    return 0;
}

static int response_body(http_parser *parser, const char *at, size_t len) {
    connection *c = parser->data;
    buffer_append(&c->body, at, len);
    return 0;
}

static int response_complete(http_parser *parser) {
    connection *c = parser->data;
    thread *thread = c->thread;
    uint64_t now = time_us();
    int status = parser->status_code;

    thread->complete++;
    thread->requests++;

    if (status > 399) {
        thread->errors.status++;
    }

    if (c->headers.buffer) {
        *c->headers.cursor++ = '\0';
        script_response(thread->L, status, &c->headers, &c->body);
        c->state = FIELD;
    }

    if (now >= thread->stop_at) {
        aeStop(thread->loop);
        goto done;
    }

    if (--c->pending == 0) {
        stats_record(thread->latency, now - c->start);
        aeCreateFileEvent(thread->loop, c->fd, AE_WRITABLE, socket_writeable, c);
    }

    if (!http_should_keep_alive(parser)) {
        reconnect_socket(thread, c);
        goto done;
    }

    http_parser_init(parser, HTTP_RESPONSE);

  done:
    return 0;
}

static void socket_connected(aeEventLoop *loop, int fd, void *data, int mask) {
    connection *c = data;

    switch (sock.connect(c)) {
        case OK:    break;
        case ERROR: goto error;
        case RETRY: return;
    }

    http_parser_init(&c->parser, HTTP_RESPONSE);
    c->written = 0;

    aeCreateFileEvent(c->thread->loop, fd, AE_READABLE, socket_readable, c);
    aeCreateFileEvent(c->thread->loop, fd, AE_WRITABLE, socket_writeable, c);

    return;

  error:
    c->thread->errors.connect++;
    reconnect_socket(c->thread, c);

}

static void socket_writeable(aeEventLoop *loop, int fd, void *data, int mask) {
    connection *c = data;
    thread *thread = c->thread;

    if (!c->written && cfg.dynamic) {
        script_request(thread->L, &c->request, &c->length);
    }

    char  *buf = c->request + c->written;
    size_t len = c->length  - c->written;
    size_t n;

    switch (sock.write(c, buf, len, &n)) {
        case OK:    break;
        case ERROR: goto error;
        case RETRY: return;
    }

    if (!c->written) {
        c->start = time_us();
        c->pending = cfg.pipeline;
    }

    c->written += n;
    if (c->written == c->length) {
        c->written = 0;
        aeDeleteFileEvent(loop, fd, AE_WRITABLE);
    }

    return;

  error:
    thread->errors.write++;
    reconnect_socket(thread, c);
}


static void socket_readable(aeEventLoop *loop, int fd, void *data, int mask) {
    connection *c = data;
    size_t n;

    do {
        switch (sock.read(c, &n)) {
            case OK:    break;
            case ERROR: goto error;
            case RETRY: return;
        }

        if (http_parser_execute(&c->parser, &parser_settings, c->buf, n) != n) goto error;
        c->thread->bytes += n;
    } while (n == RECVBUF && sock.readable(c) > 0);

    return;

  error:
    c->thread->errors.read++;
    reconnect_socket(c->thread, c);
}

static uint64_t time_us() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec * 1000000) + t.tv_usec;
}

static char *extract_url_part(char *url, struct http_parser_url *parser_url, enum http_parser_url_fields field) {
    char *part = NULL;

    if (parser_url->field_set & (1 << field)) {
        uint16_t off = parser_url->field_data[field].off;
        uint16_t len = parser_url->field_data[field].len;
        part = zcalloc(len + 1 * sizeof(char));
        memcpy(part, &url[off], len);
    }

    return part;
}

static struct option longopts[] = {
    { "connections", required_argument, NULL, 'c' },
    { "duration",    required_argument, NULL, 'd' },
    { "threads",     required_argument, NULL, 't' },
    { "script",      required_argument, NULL, 's' },
    { "header",      required_argument, NULL, 'H' },
    { "latency",     no_argument,       NULL, 'L' },
    { "timeout",     required_argument, NULL, 'T' },
    { "help",        no_argument,       NULL, 'h' },
    { "version",     no_argument,       NULL, 'v' },
    { NULL,          0,                 NULL,  0  }
};

static int parse_args(struct config *cfg, char **url, char **headers, int argc, char **argv) {
    char c, **header = headers;

    memset(cfg, 0, sizeof(struct config));
    cfg->threads     = 2;
    cfg->connections = 10;
    cfg->duration    = 10;
    cfg->timeout     = SOCKET_TIMEOUT_MS;

    while ((c = getopt_long(argc, argv, "t:c:d:s:H:T:Lrv?", longopts, NULL)) != -1) {
        switch (c) {
            case 't':
                if (scan_metric(optarg, &cfg->threads)) return -1;
                break;
            case 'c':
                if (scan_metric(optarg, &cfg->connections)) return -1;
                break;
            case 'd':
                if (scan_time(optarg, &cfg->duration)) return -1;
                break;
            case 's':
                cfg->script = optarg;
                break;
            case 'H':
                *header++ = optarg;
                break;
            case 'L':
                cfg->latency = true;
                break;
            case 'T':
                if (scan_time(optarg, &cfg->timeout)) return -1;
                cfg->timeout *= 1000;
                break;
            case 'v':
                printf("wrk %s [%s] ", VERSION, aeGetApiName());
                printf("Copyright (C) 2012 Will Glozer\n");
                break;
            case 'h':
            case '?':
            case ':':
            default:
                return -1;
        }
    }

    if (optind == argc || !cfg->threads || !cfg->duration) return -1;

    if (!cfg->connections || cfg->connections < cfg->threads) {
        fprintf(stderr, "number of connections must be >= threads\n");
        return -1;
    }

    *url    = argv[optind];
    *header = NULL;

    return 0;
}

static void print_stats_header() {
    printf("  Thread Stats%6s%11s%8s%12s\n", "Avg", "Stdev", "Max", "+/- Stdev");
}

static void print_units(long double n, char *(*fmt)(long double), int width) {
    char *msg = fmt(n);
    int len = strlen(msg), pad = 2;

    if (isalpha(msg[len-1])) pad--;
    if (isalpha(msg[len-2])) pad--;
    width -= pad;

    printf("%*.*s%.*s", width, width, msg, pad, "  ");

    free(msg);
}

static void print_stats(char *name, stats *stats, char *(*fmt)(long double)) {
    uint64_t max = stats->max;
    long double mean  = stats_summarize(stats);
    long double stdev = stats_stdev(stats, mean);

    printf("    %-10s", name);
    print_units(mean,  fmt, 8);
    print_units(stdev, fmt, 10);
    print_units(max,   fmt, 9);
    printf("%8.2Lf%%\n", stats_within_stdev(stats, mean, stdev, 1));
}

static void print_stats_latency(stats *stats) {
    long double percentiles[] = { 50.0, 75.0, 90.0, 99.0 };
    printf("  Latency Distribution\n");
    for (size_t i = 0; i < sizeof(percentiles) / sizeof(long double); i++) {
        long double p = percentiles[i];
        uint64_t n = stats_percentile(stats, p);
        printf("%7.0Lf%%", p);
        print_units(n, format_time_us, 10);
        printf("\n");
    }
}
