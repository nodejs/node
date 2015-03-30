/* crypto/bio/bss_conn.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include <errno.h>
#define USE_SOCKETS
#include "cryptlib.h"
#include <openssl/bio.h>

#ifndef OPENSSL_NO_SOCK

# ifdef OPENSSL_SYS_WIN16
#  define SOCKET_PROTOCOL 0     /* more microsoft stupidity */
# else
#  define SOCKET_PROTOCOL IPPROTO_TCP
# endif

# if (defined(OPENSSL_SYS_VMS) && __VMS_VER < 70000000)
/* FIONBIO used as a switch to enable ioctl, and that isn't in VMS < 7.0 */
#  undef FIONBIO
# endif

typedef struct bio_connect_st {
    int state;
    char *param_hostname;
    char *param_port;
    int nbio;
    unsigned char ip[4];
    unsigned short port;
    struct sockaddr_in them;
    /*
     * int socket; this will be kept in bio->num so that it is compatible
     * with the bss_sock bio
     */
    /*
     * called when the connection is initially made callback(BIO,state,ret);
     * The callback should return 'ret'.  state is for compatibility with the
     * ssl info_callback
     */
    int (*info_callback) (const BIO *bio, int state, int ret);
} BIO_CONNECT;

static int conn_write(BIO *h, const char *buf, int num);
static int conn_read(BIO *h, char *buf, int size);
static int conn_puts(BIO *h, const char *str);
static long conn_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int conn_new(BIO *h);
static int conn_free(BIO *data);
static long conn_callback_ctrl(BIO *h, int cmd, bio_info_cb *);

static int conn_state(BIO *b, BIO_CONNECT *c);
static void conn_close_socket(BIO *data);
BIO_CONNECT *BIO_CONNECT_new(void);
void BIO_CONNECT_free(BIO_CONNECT *a);

static BIO_METHOD methods_connectp = {
    BIO_TYPE_CONNECT,
    "socket connect",
    conn_write,
    conn_read,
    conn_puts,
    NULL,                       /* connect_gets, */
    conn_ctrl,
    conn_new,
    conn_free,
    conn_callback_ctrl,
};

static int conn_state(BIO *b, BIO_CONNECT *c)
{
    int ret = -1, i;
    unsigned long l;
    char *p, *q;
    int (*cb) (const BIO *, int, int) = NULL;

    if (c->info_callback != NULL)
        cb = c->info_callback;

    for (;;) {
        switch (c->state) {
        case BIO_CONN_S_BEFORE:
            p = c->param_hostname;
            if (p == NULL) {
                BIOerr(BIO_F_CONN_STATE, BIO_R_NO_HOSTNAME_SPECIFIED);
                goto exit_loop;
            }
            for (; *p != '\0'; p++) {
                if ((*p == ':') || (*p == '/'))
                    break;
            }

            i = *p;
            if ((i == ':') || (i == '/')) {

                *(p++) = '\0';
                if (i == ':') {
                    for (q = p; *q; q++)
                        if (*q == '/') {
                            *q = '\0';
                            break;
                        }
                    if (c->param_port != NULL)
                        OPENSSL_free(c->param_port);
                    c->param_port = BUF_strdup(p);
                }
            }

            if (c->param_port == NULL) {
                BIOerr(BIO_F_CONN_STATE, BIO_R_NO_PORT_SPECIFIED);
                ERR_add_error_data(2, "host=", c->param_hostname);
                goto exit_loop;
            }
            c->state = BIO_CONN_S_GET_IP;
            break;

        case BIO_CONN_S_GET_IP:
            if (BIO_get_host_ip(c->param_hostname, &(c->ip[0])) <= 0)
                goto exit_loop;
            c->state = BIO_CONN_S_GET_PORT;
            break;

        case BIO_CONN_S_GET_PORT:
            if (c->param_port == NULL) {
                /* abort(); */
                goto exit_loop;
            } else if (BIO_get_port(c->param_port, &c->port) <= 0)
                goto exit_loop;
            c->state = BIO_CONN_S_CREATE_SOCKET;
            break;

        case BIO_CONN_S_CREATE_SOCKET:
            /* now setup address */
            memset((char *)&c->them, 0, sizeof(c->them));
            c->them.sin_family = AF_INET;
            c->them.sin_port = htons((unsigned short)c->port);
            l = (unsigned long)
                ((unsigned long)c->ip[0] << 24L) |
                ((unsigned long)c->ip[1] << 16L) |
                ((unsigned long)c->ip[2] << 8L) | ((unsigned long)c->ip[3]);
            c->them.sin_addr.s_addr = htonl(l);
            c->state = BIO_CONN_S_CREATE_SOCKET;

            ret = socket(AF_INET, SOCK_STREAM, SOCKET_PROTOCOL);
            if (ret == INVALID_SOCKET) {
                SYSerr(SYS_F_SOCKET, get_last_socket_error());
                ERR_add_error_data(4, "host=", c->param_hostname,
                                   ":", c->param_port);
                BIOerr(BIO_F_CONN_STATE, BIO_R_UNABLE_TO_CREATE_SOCKET);
                goto exit_loop;
            }
            b->num = ret;
            c->state = BIO_CONN_S_NBIO;
            break;

        case BIO_CONN_S_NBIO:
            if (c->nbio) {
                if (!BIO_socket_nbio(b->num, 1)) {
                    BIOerr(BIO_F_CONN_STATE, BIO_R_ERROR_SETTING_NBIO);
                    ERR_add_error_data(4, "host=",
                                       c->param_hostname, ":", c->param_port);
                    goto exit_loop;
                }
            }
            c->state = BIO_CONN_S_CONNECT;

# if defined(SO_KEEPALIVE) && !defined(OPENSSL_SYS_MPE)
            i = 1;
            i = setsockopt(b->num, SOL_SOCKET, SO_KEEPALIVE, (char *)&i,
                           sizeof(i));
            if (i < 0) {
                SYSerr(SYS_F_SOCKET, get_last_socket_error());
                ERR_add_error_data(4, "host=", c->param_hostname,
                                   ":", c->param_port);
                BIOerr(BIO_F_CONN_STATE, BIO_R_KEEPALIVE);
                goto exit_loop;
            }
# endif
            break;

        case BIO_CONN_S_CONNECT:
            BIO_clear_retry_flags(b);
            ret = connect(b->num,
                          (struct sockaddr *)&c->them, sizeof(c->them));
            b->retry_reason = 0;
            if (ret < 0) {
                if (BIO_sock_should_retry(ret)) {
                    BIO_set_retry_special(b);
                    c->state = BIO_CONN_S_BLOCKED_CONNECT;
                    b->retry_reason = BIO_RR_CONNECT;
                } else {
                    SYSerr(SYS_F_CONNECT, get_last_socket_error());
                    ERR_add_error_data(4, "host=",
                                       c->param_hostname, ":", c->param_port);
                    BIOerr(BIO_F_CONN_STATE, BIO_R_CONNECT_ERROR);
                }
                goto exit_loop;
            } else
                c->state = BIO_CONN_S_OK;
            break;

        case BIO_CONN_S_BLOCKED_CONNECT:
            i = BIO_sock_error(b->num);
            if (i) {
                BIO_clear_retry_flags(b);
                SYSerr(SYS_F_CONNECT, i);
                ERR_add_error_data(4, "host=",
                                   c->param_hostname, ":", c->param_port);
                BIOerr(BIO_F_CONN_STATE, BIO_R_NBIO_CONNECT_ERROR);
                ret = 0;
                goto exit_loop;
            } else
                c->state = BIO_CONN_S_OK;
            break;

        case BIO_CONN_S_OK:
            ret = 1;
            goto exit_loop;
        default:
            /* abort(); */
            goto exit_loop;
        }

        if (cb != NULL) {
            if (!(ret = cb((BIO *)b, c->state, ret)))
                goto end;
        }
    }

    /* Loop does not exit */
 exit_loop:
    if (cb != NULL)
        ret = cb((BIO *)b, c->state, ret);
 end:
    return (ret);
}

BIO_CONNECT *BIO_CONNECT_new(void)
{
    BIO_CONNECT *ret;

    if ((ret = (BIO_CONNECT *)OPENSSL_malloc(sizeof(BIO_CONNECT))) == NULL)
        return (NULL);
    ret->state = BIO_CONN_S_BEFORE;
    ret->param_hostname = NULL;
    ret->param_port = NULL;
    ret->info_callback = NULL;
    ret->nbio = 0;
    ret->ip[0] = 0;
    ret->ip[1] = 0;
    ret->ip[2] = 0;
    ret->ip[3] = 0;
    ret->port = 0;
    memset((char *)&ret->them, 0, sizeof(ret->them));
    return (ret);
}

void BIO_CONNECT_free(BIO_CONNECT *a)
{
    if (a == NULL)
        return;

    if (a->param_hostname != NULL)
        OPENSSL_free(a->param_hostname);
    if (a->param_port != NULL)
        OPENSSL_free(a->param_port);
    OPENSSL_free(a);
}

BIO_METHOD *BIO_s_connect(void)
{
    return (&methods_connectp);
}

static int conn_new(BIO *bi)
{
    bi->init = 0;
    bi->num = INVALID_SOCKET;
    bi->flags = 0;
    if ((bi->ptr = (char *)BIO_CONNECT_new()) == NULL)
        return (0);
    else
        return (1);
}

static void conn_close_socket(BIO *bio)
{
    BIO_CONNECT *c;

    c = (BIO_CONNECT *)bio->ptr;
    if (bio->num != INVALID_SOCKET) {
        /* Only do a shutdown if things were established */
        if (c->state == BIO_CONN_S_OK)
            shutdown(bio->num, 2);
        closesocket(bio->num);
        bio->num = INVALID_SOCKET;
    }
}

static int conn_free(BIO *a)
{
    BIO_CONNECT *data;

    if (a == NULL)
        return (0);
    data = (BIO_CONNECT *)a->ptr;

    if (a->shutdown) {
        conn_close_socket(a);
        BIO_CONNECT_free(data);
        a->ptr = NULL;
        a->flags = 0;
        a->init = 0;
    }
    return (1);
}

static int conn_read(BIO *b, char *out, int outl)
{
    int ret = 0;
    BIO_CONNECT *data;

    data = (BIO_CONNECT *)b->ptr;
    if (data->state != BIO_CONN_S_OK) {
        ret = conn_state(b, data);
        if (ret <= 0)
            return (ret);
    }

    if (out != NULL) {
        clear_socket_error();
        ret = readsocket(b->num, out, outl);
        BIO_clear_retry_flags(b);
        if (ret <= 0) {
            if (BIO_sock_should_retry(ret))
                BIO_set_retry_read(b);
        }
    }
    return (ret);
}

static int conn_write(BIO *b, const char *in, int inl)
{
    int ret;
    BIO_CONNECT *data;

    data = (BIO_CONNECT *)b->ptr;
    if (data->state != BIO_CONN_S_OK) {
        ret = conn_state(b, data);
        if (ret <= 0)
            return (ret);
    }

    clear_socket_error();
    ret = writesocket(b->num, in, inl);
    BIO_clear_retry_flags(b);
    if (ret <= 0) {
        if (BIO_sock_should_retry(ret))
            BIO_set_retry_write(b);
    }
    return (ret);
}

static long conn_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    BIO *dbio;
    int *ip;
    const char **pptr;
    long ret = 1;
    BIO_CONNECT *data;

    data = (BIO_CONNECT *)b->ptr;

    switch (cmd) {
    case BIO_CTRL_RESET:
        ret = 0;
        data->state = BIO_CONN_S_BEFORE;
        conn_close_socket(b);
        b->flags = 0;
        break;
    case BIO_C_DO_STATE_MACHINE:
        /* use this one to start the connection */
        if (data->state != BIO_CONN_S_OK)
            ret = (long)conn_state(b, data);
        else
            ret = 1;
        break;
    case BIO_C_GET_CONNECT:
        if (ptr != NULL) {
            pptr = (const char **)ptr;
            if (num == 0) {
                *pptr = data->param_hostname;

            } else if (num == 1) {
                *pptr = data->param_port;
            } else if (num == 2) {
                *pptr = (char *)&(data->ip[0]);
            } else if (num == 3) {
                *((int *)ptr) = data->port;
            }
            if ((!b->init) || (ptr == NULL))
                *pptr = "not initialized";
            ret = 1;
        }
        break;
    case BIO_C_SET_CONNECT:
        if (ptr != NULL) {
            b->init = 1;
            if (num == 0) {
                if (data->param_hostname != NULL)
                    OPENSSL_free(data->param_hostname);
                data->param_hostname = BUF_strdup(ptr);
            } else if (num == 1) {
                if (data->param_port != NULL)
                    OPENSSL_free(data->param_port);
                data->param_port = BUF_strdup(ptr);
            } else if (num == 2) {
                char buf[16];
                unsigned char *p = ptr;

                BIO_snprintf(buf, sizeof buf, "%d.%d.%d.%d",
                             p[0], p[1], p[2], p[3]);
                if (data->param_hostname != NULL)
                    OPENSSL_free(data->param_hostname);
                data->param_hostname = BUF_strdup(buf);
                memcpy(&(data->ip[0]), ptr, 4);
            } else if (num == 3) {
                char buf[DECIMAL_SIZE(int) + 1];

                BIO_snprintf(buf, sizeof buf, "%d", *(int *)ptr);
                if (data->param_port != NULL)
                    OPENSSL_free(data->param_port);
                data->param_port = BUF_strdup(buf);
                data->port = *(int *)ptr;
            }
        }
        break;
    case BIO_C_SET_NBIO:
        data->nbio = (int)num;
        break;
    case BIO_C_GET_FD:
        if (b->init) {
            ip = (int *)ptr;
            if (ip != NULL)
                *ip = b->num;
            ret = b->num;
        } else
            ret = -1;
        break;
    case BIO_CTRL_GET_CLOSE:
        ret = b->shutdown;
        break;
    case BIO_CTRL_SET_CLOSE:
        b->shutdown = (int)num;
        break;
    case BIO_CTRL_PENDING:
    case BIO_CTRL_WPENDING:
        ret = 0;
        break;
    case BIO_CTRL_FLUSH:
        break;
    case BIO_CTRL_DUP:
        {
            dbio = (BIO *)ptr;
            if (data->param_port)
                BIO_set_conn_port(dbio, data->param_port);
            if (data->param_hostname)
                BIO_set_conn_hostname(dbio, data->param_hostname);
            BIO_set_nbio(dbio, data->nbio);
            /*
             * FIXME: the cast of the function seems unlikely to be a good
             * idea
             */
            (void)BIO_set_info_callback(dbio,
                                        (bio_info_cb *)data->info_callback);
        }
        break;
    case BIO_CTRL_SET_CALLBACK:
        {
# if 0                          /* FIXME: Should this be used? -- Richard
                                 * Levitte */
            BIOerr(BIO_F_CONN_CTRL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
            ret = -1;
# else
            ret = 0;
# endif
        }
        break;
    case BIO_CTRL_GET_CALLBACK:
        {
            int (**fptr) (const BIO *bio, int state, int xret);

            fptr = (int (**)(const BIO *bio, int state, int xret))ptr;
            *fptr = data->info_callback;
        }
        break;
    default:
        ret = 0;
        break;
    }
    return (ret);
}

static long conn_callback_ctrl(BIO *b, int cmd, bio_info_cb *fp)
{
    long ret = 1;
    BIO_CONNECT *data;

    data = (BIO_CONNECT *)b->ptr;

    switch (cmd) {
    case BIO_CTRL_SET_CALLBACK:
        {
            data->info_callback =
                (int (*)(const struct bio_st *, int, int))fp;
        }
        break;
    default:
        ret = 0;
        break;
    }
    return (ret);
}

static int conn_puts(BIO *bp, const char *str)
{
    int n, ret;

    n = strlen(str);
    ret = conn_write(bp, str, n);
    return (ret);
}

BIO *BIO_new_connect(const char *str)
{
    BIO *ret;

    ret = BIO_new(BIO_s_connect());
    if (ret == NULL)
        return (NULL);
    if (BIO_set_conn_hostname(ret, str))
        return (ret);
    else {
        BIO_free(ret);
        return (NULL);
    }
}

#endif
