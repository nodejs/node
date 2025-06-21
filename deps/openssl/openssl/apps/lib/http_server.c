/*
 * Copyright 1995-2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Very basic HTTP server */

#if !defined(_POSIX_C_SOURCE) && defined(OPENSSL_SYS_VMS)
/*
 * On VMS, you need to define this to get the declaration of fileno().  The
 * value 2 is to make sure no function defined in POSIX-2 is left undefined.
 */
# define _POSIX_C_SOURCE 2
#endif

#include <string.h>
#include <ctype.h>
#include "http_server.h"
#include "internal/sockets.h"
#include <openssl/err.h>
#include <openssl/rand.h>
#include "s_apps.h"

#if defined(__TANDEM)
# if defined(OPENSSL_TANDEM_FLOSS)
#  include <floss.h(floss_fork)>
# endif
#endif

static int verbosity = LOG_INFO;

#define HTTP_PREFIX "HTTP/"
#define HTTP_VERSION_PATT "1." /* allow 1.x */
#define HTTP_PREFIX_VERSION HTTP_PREFIX""HTTP_VERSION_PATT
#define HTTP_1_0 HTTP_PREFIX_VERSION"0" /* "HTTP/1.0" */

#ifdef HTTP_DAEMON

int multi = 0; /* run multiple responder processes */
int acfd = (int) INVALID_SOCKET;

static int print_syslog(const char *str, size_t len, void *levPtr)
{
    int level = *(int *)levPtr;
    int ilen = len > MAXERRLEN ? MAXERRLEN : len;

    syslog(level, "%.*s", ilen, str);

    return ilen;
}
#endif

void log_message(const char *prog, int level, const char *fmt, ...)
{
    va_list ap;

    if (verbosity < level)
        return;

    va_start(ap, fmt);
#ifdef HTTP_DAEMON
    if (multi) {
        char buf[1024];

        if (vsnprintf(buf, sizeof(buf), fmt, ap) > 0)
            syslog(level, "%s", buf);
        if (level <= LOG_ERR)
            ERR_print_errors_cb(print_syslog, &level);
    } else
#endif
    {
        BIO_printf(bio_err, "%s: ", prog);
        BIO_vprintf(bio_err, fmt, ap);
        BIO_printf(bio_err, "\n");
        (void)BIO_flush(bio_err);
    }
    va_end(ap);
}

#ifdef HTTP_DAEMON
void socket_timeout(int signum)
{
    if (acfd != (int)INVALID_SOCKET)
        (void)shutdown(acfd, SHUT_RD);
}

static void killall(int ret, pid_t *kidpids)
{
    int i;

    for (i = 0; i < multi; ++i)
        if (kidpids[i] != 0)
            (void)kill(kidpids[i], SIGTERM);
    OPENSSL_free(kidpids);
    ossl_sleep(1000);
    exit(ret);
}

static int termsig = 0;

static void noteterm(int sig)
{
    termsig = sig;
}

/*
 * Loop spawning up to `multi` child processes, only child processes return
 * from this function.  The parent process loops until receiving a termination
 * signal, kills extant children and exits without returning.
 */
void spawn_loop(const char *prog)
{
    pid_t *kidpids = NULL;
    int status;
    int procs = 0;
    int i;

    openlog(prog, LOG_PID, LOG_DAEMON);

    if (setpgid(0, 0)) {
        syslog(LOG_ERR, "fatal: error detaching from parent process group: %s",
               strerror(errno));
        exit(1);
    }
    kidpids = app_malloc(multi * sizeof(*kidpids), "child PID array");
    for (i = 0; i < multi; ++i)
        kidpids[i] = 0;

    signal(SIGINT, noteterm);
    signal(SIGTERM, noteterm);

    while (termsig == 0) {
        pid_t fpid;

        /*
         * Wait for a child to replace when we're at the limit.
         * Slow down if a child exited abnormally or waitpid() < 0
         */
        while (termsig == 0 && procs >= multi) {
            if ((fpid = waitpid(-1, &status, 0)) > 0) {
                for (i = 0; i < procs; ++i) {
                    if (kidpids[i] == fpid) {
                        kidpids[i] = 0;
                        --procs;
                        break;
                    }
                }
                if (i >= multi) {
                    syslog(LOG_ERR, "fatal: internal error: "
                           "no matching child slot for pid: %ld",
                           (long) fpid);
                    killall(1, kidpids);
                }
                if (status != 0) {
                    if (WIFEXITED(status))
                        syslog(LOG_WARNING, "child process: %ld, exit status: %d",
                               (long)fpid, WEXITSTATUS(status));
                    else if (WIFSIGNALED(status))
                        syslog(LOG_WARNING, "child process: %ld, term signal %d%s",
                               (long)fpid, WTERMSIG(status),
# ifdef WCOREDUMP
                               WCOREDUMP(status) ? " (core dumped)" :
# endif
                               "");
                    ossl_sleep(1000);
                }
                break;
            } else if (errno != EINTR) {
                syslog(LOG_ERR, "fatal: waitpid(): %s", strerror(errno));
                killall(1, kidpids);
            }
        }
        if (termsig)
            break;

        switch (fpid = fork()) {
        case -1: /* error */
            /* System critically low on memory, pause and try again later */
            ossl_sleep(30000);
            break;
        case 0: /* child */
            OPENSSL_free(kidpids);
            signal(SIGINT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            if (termsig)
                _exit(0);
            if (RAND_poll() <= 0) {
                syslog(LOG_ERR, "fatal: RAND_poll() failed");
                _exit(1);
            }
            return;
        default:            /* parent */
            for (i = 0; i < multi; ++i) {
                if (kidpids[i] == 0) {
                    kidpids[i] = fpid;
                    procs++;
                    break;
                }
            }
            if (i >= multi) {
                syslog(LOG_ERR, "fatal: internal error: no free child slots");
                killall(1, kidpids);
            }
            break;
        }
    }

    /* The loop above can only break on termsig */
    syslog(LOG_INFO, "terminating on signal: %d", termsig);
    killall(0, kidpids);
}
#endif

#ifndef OPENSSL_NO_SOCK
BIO *http_server_init_bio(const char *prog, const char *port)
{
    BIO *acbio = NULL, *bufbio;
    int asock;
    char name[40];

    snprintf(name, sizeof(name), "[::]:%s", port); /* port may be "0" */
    bufbio = BIO_new(BIO_f_buffer());
    if (bufbio == NULL)
        goto err;
    acbio = BIO_new(BIO_s_accept());
    if (acbio == NULL
        || BIO_set_accept_ip_family(acbio, BIO_FAMILY_IPANY) <= 0 /* IPv4/6 */
        || BIO_set_bind_mode(acbio, BIO_BIND_REUSEADDR) <= 0
        || BIO_set_accept_name(acbio, name) <= 0) {
        log_message(prog, LOG_ERR, "Error setting up accept BIO");
        goto err;
    }

    BIO_set_accept_bios(acbio, bufbio);
    bufbio = NULL;
    if (BIO_do_accept(acbio) <= 0) {
        log_message(prog, LOG_ERR, "Error starting accept");
        goto err;
    }

    /* Report back what address and port are used */
    BIO_get_fd(acbio, &asock);
    if (!report_server_accept(bio_out, asock, 1, 1)) {
        log_message(prog, LOG_ERR, "Error printing ACCEPT string");
        goto err;
    }

    return acbio;

 err:
    BIO_free_all(acbio);
    BIO_free(bufbio);
    return NULL;
}

/*
 * Decode %xx URL-decoding in-place. Ignores malformed sequences.
 */
static int urldecode(char *p)
{
    unsigned char *out = (unsigned char *)p;
    unsigned char *save = out;

    for (; *p; p++) {
        if (*p != '%') {
            *out++ = *p;
        } else if (isxdigit(_UC(p[1])) && isxdigit(_UC(p[2]))) {
            /* Don't check, can't fail because of ixdigit() call. */
            *out++ = (OPENSSL_hexchar2int(p[1]) << 4)
                | OPENSSL_hexchar2int(p[2]);
            p += 2;
        } else {
            return -1;
        }
    }
    *out = '\0';
    return (int)(out - save);
}

/* if *pcbio != NULL, continue given connected session, else accept new */
/* if found_keep_alive != NULL, return this way connection persistence state */
int http_server_get_asn1_req(const ASN1_ITEM *it, ASN1_VALUE **preq,
                             char **ppath, BIO **pcbio, BIO *acbio,
                             int *found_keep_alive,
                             const char *prog, const char *port,
                             int accept_get, int timeout)
{
    BIO *cbio = *pcbio, *getbio = NULL, *b64 = NULL;
    int len;
    char reqbuf[2048], inbuf[2048];
    char *meth, *url, *end;
    ASN1_VALUE *req;
    int ret = 0;

    *preq = NULL;
    if (ppath != NULL)
        *ppath = NULL;

    if (cbio == NULL) {
        log_message(prog, LOG_DEBUG,
                    "Awaiting new connection on port %s...", port);
        if (BIO_do_accept(acbio) <= 0)
            /* Connection loss before accept() is routine, ignore silently */
            return ret;

        *pcbio = cbio = BIO_pop(acbio);
    } else {
        log_message(prog, LOG_DEBUG, "Awaiting next request...");
    }
    if (cbio == NULL) {
        /* Cannot call http_server_send_status(cbio, ...) */
        ret = -1;
        goto out;
    }

# ifdef HTTP_DAEMON
    if (timeout > 0) {
        (void)BIO_get_fd(cbio, &acfd);
        alarm(timeout);
    }
# endif

    /* Read the request line. */
    len = BIO_gets(cbio, reqbuf, sizeof(reqbuf));
    if (len == 0)
        return ret;
    ret = 1;
    if (len < 0) {
        log_message(prog, LOG_WARNING, "Request line read error");
        (void)http_server_send_status(cbio, 400, "Bad Request");
        goto out;
    }
    if ((end = strchr(reqbuf, '\r')) != NULL
            || (end = strchr(reqbuf, '\n')) != NULL)
        *end = '\0';
    log_message(prog, LOG_INFO, "Received request, 1st line: %s", reqbuf);

    meth = reqbuf;
    url = meth + 3;
    if ((accept_get && strncmp(meth, "GET ", 4) == 0)
            || (url++, strncmp(meth, "POST ", 5) == 0)) {
        static const char http_version_str[] = " "HTTP_PREFIX_VERSION;
        static const size_t http_version_str_len = sizeof(http_version_str) - 1;

        /* Expecting (GET|POST) {sp} /URL {sp} HTTP/1.x */
        *(url++) = '\0';
        while (*url == ' ')
            url++;
        if (*url != '/') {
            log_message(prog, LOG_WARNING,
                        "Invalid %s -- URL does not begin with '/': %s",
                        meth, url);
            (void)http_server_send_status(cbio, 400, "Bad Request");
            goto out;
        }
        url++;

        /* Splice off the HTTP version identifier. */
        for (end = url; *end != '\0'; end++)
            if (*end == ' ')
                break;
        if (strncmp(end, http_version_str, http_version_str_len) != 0) {
            log_message(prog, LOG_WARNING,
                        "Invalid %s -- bad HTTP/version string: %s",
                        meth, end + 1);
            (void)http_server_send_status(cbio, 400, "Bad Request");
            goto out;
        }
        *end = '\0';
        /* above HTTP 1.0, connection persistence is the default */
        if (found_keep_alive != NULL)
            *found_keep_alive = end[http_version_str_len] > '0';

        /*-
         * Skip "GET / HTTP..." requests often used by load-balancers.
         * 'url' was incremented above to point to the first byte *after*
         * the leading slash, so in case 'GET / ' it is now an empty string.
         */
        if (strlen(meth) == 3 && url[0] == '\0') {
            (void)http_server_send_status(cbio, 200, "OK");
            goto out;
        }

        len = urldecode(url);
        if (len < 0) {
            log_message(prog, LOG_WARNING,
                        "Invalid %s request -- bad URL encoding: %s",
                        meth, url);
            (void)http_server_send_status(cbio, 400, "Bad Request");
            goto out;
        }
        if (strlen(meth) == 3) { /* GET */
            if ((getbio = BIO_new_mem_buf(url, len)) == NULL
                || (b64 = BIO_new(BIO_f_base64())) == NULL) {
                log_message(prog, LOG_ERR,
                            "Could not allocate base64 bio with size = %d",
                            len);
                goto fatal;
            }
            BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
            getbio = BIO_push(b64, getbio);
        }
    } else {
        log_message(prog, LOG_WARNING,
                    "HTTP request does not begin with %sPOST: %s",
                    accept_get ? "GET or " : "", reqbuf);
        (void)http_server_send_status(cbio, 400, "Bad Request");
        goto out;
    }

    /* chop any further/duplicate leading or trailing '/' */
    while (*url == '/')
        url++;
    while (end >= url + 2 && end[-2] == '/' && end[-1] == '/')
        end--;
    *end = '\0';

    /* Read and skip past the headers. */
    for (;;) {
        char *key, *value, *line_end = NULL;

        len = BIO_gets(cbio, inbuf, sizeof(inbuf));
        if (len <= 0) {
            log_message(prog, LOG_WARNING, "Error reading HTTP header");
            (void)http_server_send_status(cbio, 400, "Bad Request");
            goto out;
        }

        if (inbuf[0] == '\r' || inbuf[0] == '\n')
            break;

        key = inbuf;
        value = strchr(key, ':');
        if (value == NULL) {
            log_message(prog, LOG_WARNING,
                        "Error parsing HTTP header: missing ':'");
            (void)http_server_send_status(cbio, 400, "Bad Request");
            goto out;
        }
        *(value++) = '\0';
        while (*value == ' ')
            value++;
        line_end = strchr(value, '\r');
        if (line_end == NULL) {
            line_end = strchr(value, '\n');
            if (line_end == NULL) {
                log_message(prog, LOG_WARNING,
                            "Error parsing HTTP header: missing end of line");
                (void)http_server_send_status(cbio, 400, "Bad Request");
                goto out;
            }
        }
        *line_end = '\0';
        /* https://tools.ietf.org/html/rfc7230#section-6.3 Persistence */
        if (found_keep_alive != NULL
            && OPENSSL_strcasecmp(key, "Connection") == 0) {
            if (OPENSSL_strcasecmp(value, "keep-alive") == 0)
                *found_keep_alive = 1;
            else if (OPENSSL_strcasecmp(value, "close") == 0)
                *found_keep_alive = 0;
        }
    }

# ifdef HTTP_DAEMON
    /* Clear alarm before we close the client socket */
    alarm(0);
    timeout = 0;
# endif

    /* Try to read and parse request */
    req = ASN1_item_d2i_bio(it, getbio != NULL ? getbio : cbio, NULL);
    if (req == NULL) {
        log_message(prog, LOG_WARNING,
                    "Error parsing DER-encoded request content");
        (void)http_server_send_status(cbio, 400, "Bad Request");
    } else if (ppath != NULL && (*ppath = OPENSSL_strdup(url)) == NULL) {
        log_message(prog, LOG_ERR,
                    "Out of memory allocating %zu bytes", strlen(url) + 1);
        ASN1_item_free(req, it);
        goto fatal;
    }

    *preq = req;

 out:
    BIO_free_all(getbio);
# ifdef HTTP_DAEMON
    if (timeout > 0)
        alarm(0);
    acfd = (int)INVALID_SOCKET;
# endif
    return ret;

 fatal:
    (void)http_server_send_status(cbio, 500, "Internal Server Error");
    if (ppath != NULL) {
        OPENSSL_free(*ppath);
        *ppath = NULL;
    }
    BIO_free_all(cbio);
    *pcbio = NULL;
    ret = -1;
    goto out;
}

/* assumes that cbio does not do an encoding that changes the output length */
int http_server_send_asn1_resp(BIO *cbio, int keep_alive,
                               const char *content_type,
                               const ASN1_ITEM *it, const ASN1_VALUE *resp)
{
    int ret = BIO_printf(cbio, HTTP_1_0" 200 OK\r\n%s"
                         "Content-type: %s\r\n"
                         "Content-Length: %d\r\n\r\n",
                         keep_alive ? "Connection: keep-alive\r\n" : "",
                         content_type,
                         ASN1_item_i2d(resp, NULL, it)) > 0
            && ASN1_item_i2d_bio(it, cbio, resp) > 0;

    (void)BIO_flush(cbio);
    return ret;
}

int http_server_send_status(BIO *cbio, int status, const char *reason)
{
    int ret = BIO_printf(cbio, HTTP_1_0" %d %s\r\n\r\n",
                         /* This implicitly cancels keep-alive */
                         status, reason) > 0;

    (void)BIO_flush(cbio);
    return ret;
}
#endif
