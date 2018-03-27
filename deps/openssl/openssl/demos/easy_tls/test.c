/* test.c */
/* $Id: test.c,v 1.1 2001/09/17 19:06:59 bodo Exp $ */

#define L_PORT 9999
#define C_PORT 443

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "test.h"
#include "easy-tls.h"

void test_process_init(int fd, int client_p, void *apparg)
{
    fprintf(stderr,
            "test_process_init(fd = %d, client_p = %d, apparg = %p)\n", fd,
            client_p, apparg);
}

void test_errflush(int child_p, char *errbuf, size_t num, void *apparg)
{
    fputs(errbuf, stderr);
}

int main(int argc, char *argv[])
{
    int s, fd, r;
    FILE *conn_in;
    FILE *conn_out;
    char buf[256];
    SSL_CTX *ctx;
    int client_p = 0;
    int port;
    int tls = 0;
    char infobuf[TLS_INFO_SIZE + 1];

    if (argc > 1 && argv[1][0] == '-') {
        fputs("Usage: test [port]                   -- server\n"
              "       test num.num.num.num [port]   -- client\n", stderr);
        exit(1);
    }

    if (argc > 1) {
        if (strchr(argv[1], '.')) {
            client_p = 1;
        }
    }

    fputs(client_p ? "Client\n" : "Server\n", stderr);

    {
        struct tls_create_ctx_args a = tls_create_ctx_defaultargs();
        a.client_p = client_p;
        a.certificate_file = "cert.pem";
        a.key_file = "cert.pem";
        a.ca_file = "cacerts.pem";

        ctx = tls_create_ctx(a, NULL);
        if (ctx == NULL)
            exit(1);
    }

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == -1) {
        perror("socket");
        exit(1);
    }

    if (client_p) {
        struct sockaddr_in addr;
        size_t addr_len = sizeof(addr);

        addr.sin_family = AF_INET;
        assert(argc > 1);
        if (argc > 2)
            sscanf(argv[2], "%d", &port);
        else
            port = C_PORT;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(argv[1]);

        r = connect(s, &addr, addr_len);
        if (r != 0) {
            perror("connect");
            exit(1);
        }
        fd = s;
        fprintf(stderr, "Connect (fd = %d).\n", fd);
    } else {
        /* server */
        {
            int i = 1;

            r = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&i, sizeof(i));
            if (r == -1) {
                perror("setsockopt");
                exit(1);
            }
        }

        {
            struct sockaddr_in addr;
            size_t addr_len = sizeof(addr);

            if (argc > 1)
                sscanf(argv[1], "%d", &port);
            else
                port = L_PORT;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            r = bind(s, &addr, addr_len);
            if (r != 0) {
                perror("bind");
                exit(1);
            }
        }

        r = listen(s, 1);
        if (r == -1) {
            perror("listen");
            exit(1);
        }

        fprintf(stderr, "Listening at port %i.\n", port);

        fd = accept(s, NULL, 0);
        if (fd == -1) {
            perror("accept");
            exit(1);
        }

        fprintf(stderr, "Accept (fd = %d).\n", fd);
    }

    conn_in = fdopen(fd, "r");
    if (conn_in == NULL) {
        perror("fdopen");
        exit(1);
    }
    conn_out = fdopen(fd, "w");
    if (conn_out == NULL) {
        perror("fdopen");
        exit(1);
    }

    setvbuf(conn_in, NULL, _IOLBF, 256);
    setvbuf(conn_out, NULL, _IOLBF, 256);

    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        if (buf[0] == 'W') {
            fprintf(conn_out, "%.*s\r\n", (int)(strlen(buf + 1) - 1),
                    buf + 1);
            fprintf(stderr, ">>> %.*s\n", (int)(strlen(buf + 1) - 1),
                    buf + 1);
        } else if (buf[0] == 'C') {
            fprintf(stderr, "Closing.\n");
            fclose(conn_in);
            fclose(conn_out);
            exit(0);
        } else if (buf[0] == 'R') {
            int lines = 0;

            sscanf(buf + 1, "%d", &lines);
            do {
                if (fgets(buf, sizeof(buf), conn_in) == NULL) {
                    if (ferror(conn_in)) {
                        fprintf(stderr, "ERROR\n");
                        exit(1);
                    }
                    fprintf(stderr, "CLOSED\n");
                    return 0;
                }
                fprintf(stderr, "<<< %s", buf);
            } while (--lines > 0);
        } else if (buf[0] == 'T') {
            int infofd;

            tls++;
            {
                struct tls_start_proxy_args a = tls_start_proxy_defaultargs();
                a.fd = fd;
                a.client_p = client_p;
                a.ctx = ctx;
                a.infofd = &infofd;
                r = tls_start_proxy(a, NULL);
            }
            assert(r != 1);
            if (r != 0) {
                fprintf(stderr, "tls_start_proxy failed: %d\n", r);
                switch (r) {
                case -1:
                    fputs("socketpair", stderr);
                    break;
                case 2:
                    fputs("FD_SETSIZE exceeded", stderr);
                    break;
                case -3:
                    fputs("pipe", stderr);
                    break;
                case -4:
                    fputs("fork", stderr);
                    break;
                case -5:
                    fputs("dup2", stderr);
                    break;
                default:
                    fputs("?", stderr);
                }
                if (r < 0)
                    perror("");
                else
                    fputc('\n', stderr);
                exit(1);
            }

            r = read(infofd, infobuf, sizeof(infobuf) - 1);
            if (r > 0) {
                const char *info = infobuf;
                const char *eol;

                infobuf[r] = '\0';
                while ((eol = strchr(info, '\n')) != NULL) {
                    fprintf(stderr, "+++ `%.*s'\n", eol - info, info);
                    info = eol + 1;
                }
                close(infofd);
            }
        } else {
            fprintf(stderr, "W...  write line to network\n"
                    "R[n]  read line (n lines) from network\n"
                    "C     close\n"
                    "T     start %sTLS proxy\n", tls ? "another " : "");
        }
    }
    return 0;
}
