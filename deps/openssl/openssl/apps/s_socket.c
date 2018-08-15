/*
 * apps/s_socket.c - socket-related functions used by s_client and s_server
 */
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#ifdef FLAT_INC
# include "e_os2.h"
#else
# include "../e_os2.h"
#endif

/*
 * With IPv6, it looks like Digital has mixed up the proper order of
 * recursive header file inclusion, resulting in the compiler complaining
 * that u_int isn't defined, but only if _POSIX_C_SOURCE is defined, which is
 * needed to have fileno() declared correctly...  So let's define u_int
 */
#if defined(OPENSSL_SYS_VMS_DECC) && !defined(__U_INT)
# define __U_INT
typedef unsigned int u_int;
#endif

#define USE_SOCKETS
#define NON_MAIN
#include "apps.h"
#undef USE_SOCKETS
#undef NON_MAIN
#include "s_apps.h"
#include <openssl/ssl.h>

#ifdef FLAT_INC
# include "e_os.h"
#else
# include "../e_os.h"
#endif

#ifndef OPENSSL_NO_SOCK

# if defined(OPENSSL_SYS_NETWARE) && defined(NETWARE_BSDSOCK)
#  include "netdb.h"
# endif

static struct hostent *GetHostByName(char *name);
# if defined(OPENSSL_SYS_WINDOWS) || (defined(OPENSSL_SYS_NETWARE) && !defined(NETWARE_BSDSOCK))
static void ssl_sock_cleanup(void);
# endif
static int ssl_sock_init(void);
static int init_client_ip(int *sock, unsigned char ip[4], int port, int type);
static int init_server(int *sock, int port, int type);
static int init_server_long(int *sock, int port, char *ip, int type);
static int do_accept(int acc_sock, int *sock);
static int host_ip(char *str, unsigned char ip[4]);

# ifdef OPENSSL_SYS_WIN16
#  define SOCKET_PROTOCOL 0     /* more microsoft stupidity */
# else
#  define SOCKET_PROTOCOL IPPROTO_TCP
# endif

# if defined(OPENSSL_SYS_NETWARE) && !defined(NETWARE_BSDSOCK)
static int wsa_init_done = 0;
# endif

# ifdef OPENSSL_SYS_WINDOWS
static struct WSAData wsa_state;
static int wsa_init_done = 0;

#  ifdef OPENSSL_SYS_WIN16
static HWND topWnd = 0;
static FARPROC lpTopWndProc = NULL;
static FARPROC lpTopHookProc = NULL;
extern HINSTANCE _hInstance;    /* nice global CRT provides */

static LONG FAR PASCAL topHookProc(HWND hwnd, UINT message, WPARAM wParam,
                                   LPARAM lParam)
{
    if (hwnd == topWnd) {
        switch (message) {
        case WM_DESTROY:
        case WM_CLOSE:
            SetWindowLong(topWnd, GWL_WNDPROC, (LONG) lpTopWndProc);
            ssl_sock_cleanup();
            break;
        }
    }
    return CallWindowProc(lpTopWndProc, hwnd, message, wParam, lParam);
}

static BOOL CALLBACK enumproc(HWND hwnd, LPARAM lParam)
{
    topWnd = hwnd;
    return (FALSE);
}

#  endif                        /* OPENSSL_SYS_WIN32 */
# endif                         /* OPENSSL_SYS_WINDOWS */

# ifdef OPENSSL_SYS_WINDOWS
static void ssl_sock_cleanup(void)
{
    if (wsa_init_done) {
        wsa_init_done = 0;
#  ifndef OPENSSL_SYS_WINCE
        WSACancelBlockingCall();
#  endif
        WSACleanup();
    }
}
# elif defined(OPENSSL_SYS_NETWARE) && !defined(NETWARE_BSDSOCK)
static void sock_cleanup(void)
{
    if (wsa_init_done) {
        wsa_init_done = 0;
        WSACleanup();
    }
}
# endif

static int ssl_sock_init(void)
{
# ifdef WATT32
    extern int _watt_do_exit;
    _watt_do_exit = 0;
    if (sock_init())
        return (0);
# elif defined(OPENSSL_SYS_WINDOWS)
    if (!wsa_init_done) {
        int err;

#  ifdef SIGINT
        signal(SIGINT, (void (*)(int))ssl_sock_cleanup);
#  endif
        wsa_init_done = 1;
        memset(&wsa_state, 0, sizeof(wsa_state));
        if (WSAStartup(0x0101, &wsa_state) != 0) {
            err = WSAGetLastError();
            BIO_printf(bio_err, "unable to start WINSOCK, error code=%d\n",
                       err);
            return (0);
        }
#  ifdef OPENSSL_SYS_WIN16
        EnumTaskWindows(GetCurrentTask(), enumproc, 0L);
        lpTopWndProc = (FARPROC) GetWindowLong(topWnd, GWL_WNDPROC);
        lpTopHookProc = MakeProcInstance((FARPROC) topHookProc, _hInstance);

        SetWindowLong(topWnd, GWL_WNDPROC, (LONG) lpTopHookProc);
#  endif                        /* OPENSSL_SYS_WIN16 */
    }
# elif defined(OPENSSL_SYS_NETWARE) && !defined(NETWARE_BSDSOCK)
    WORD wVerReq;
    WSADATA wsaData;
    int err;

    if (!wsa_init_done) {

#  ifdef SIGINT
        signal(SIGINT, (void (*)(int))sock_cleanup);
#  endif

        wsa_init_done = 1;
        wVerReq = MAKEWORD(2, 0);
        err = WSAStartup(wVerReq, &wsaData);
        if (err != 0) {
            BIO_printf(bio_err, "unable to start WINSOCK2, error code=%d\n",
                       err);
            return (0);
        }
    }
# endif                         /* OPENSSL_SYS_WINDOWS */
    return (1);
}

int init_client(int *sock, char *host, int port, int type)
{
    unsigned char ip[4];

    memset(ip, '\0', sizeof(ip));
    if (!host_ip(host, &(ip[0])))
        return 0;
    return init_client_ip(sock, ip, port, type);
}

static int init_client_ip(int *sock, unsigned char ip[4], int port, int type)
{
    unsigned long addr;
    struct sockaddr_in them;
    int s, i;

    if (!ssl_sock_init())
        return (0);

    memset((char *)&them, 0, sizeof(them));
    them.sin_family = AF_INET;
    them.sin_port = htons((unsigned short)port);
    addr = (unsigned long)
        ((unsigned long)ip[0] << 24L) |
        ((unsigned long)ip[1] << 16L) |
        ((unsigned long)ip[2] << 8L) | ((unsigned long)ip[3]);
    them.sin_addr.s_addr = htonl(addr);

    if (type == SOCK_STREAM)
        s = socket(AF_INET, SOCK_STREAM, SOCKET_PROTOCOL);
    else                        /* ( type == SOCK_DGRAM) */
        s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (s == INVALID_SOCKET) {
        perror("socket");
        return (0);
    }
# if defined(SO_KEEPALIVE) && !defined(OPENSSL_SYS_MPE)
    if (type == SOCK_STREAM) {
        i = 0;
        i = setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&i, sizeof(i));
        if (i < 0) {
            closesocket(s);
            perror("keepalive");
            return (0);
        }
    }
# endif

    if (connect(s, (struct sockaddr *)&them, sizeof(them)) == -1) {
        closesocket(s);
        perror("connect");
        return (0);
    }
    *sock = s;
    return (1);
}

int do_server(int port, int type, int *ret,
              int (*cb) (int s, int stype, unsigned char *context),
              unsigned char *context, int naccept)
{
    int sock;
    int accept_socket = 0;
    int i;

    if (!init_server(&accept_socket, port, type))
        return (0);

    if (ret != NULL) {
        *ret = accept_socket;
        /* return(1); */
    }
    for (;;) {
        if (type == SOCK_STREAM) {
            if (do_accept(accept_socket, &sock) == 0) {
                SHUTDOWN(accept_socket);
                return (0);
            }
        } else
            sock = accept_socket;
        i = (*cb) (sock, type, context);
        if (type == SOCK_STREAM)
            SHUTDOWN2(sock);
        if (naccept != -1)
            naccept--;
        if (i < 0 || naccept == 0) {
            SHUTDOWN2(accept_socket);
            return (i);
        }
    }
}

static int init_server_long(int *sock, int port, char *ip, int type)
{
    int ret = 0;
    struct sockaddr_in server;
    int s = -1;

    if (!ssl_sock_init())
        return (0);

    memset((char *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons((unsigned short)port);
    if (ip == NULL)
        server.sin_addr.s_addr = INADDR_ANY;
    else
/* Added for T3E, address-of fails on bit field (beckman@acl.lanl.gov) */
# ifndef BIT_FIELD_LIMITS
        memcpy(&server.sin_addr.s_addr, ip, 4);
# else
        memcpy(&server.sin_addr, ip, 4);
# endif

    if (type == SOCK_STREAM)
        s = socket(AF_INET, SOCK_STREAM, SOCKET_PROTOCOL);
    else                        /* type == SOCK_DGRAM */
        s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (s == INVALID_SOCKET)
        goto err;
# if defined SOL_SOCKET && defined SO_REUSEADDR
    {
        int j = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&j, sizeof(j));
    }
# endif
    if (bind(s, (struct sockaddr *)&server, sizeof(server)) == -1) {
# ifndef OPENSSL_SYS_WINDOWS
        perror("bind");
# endif
        goto err;
    }
    /* Make it 128 for linux */
    if (type == SOCK_STREAM && listen(s, 128) == -1)
        goto err;
    *sock = s;
    ret = 1;
 err:
    if ((ret == 0) && (s != -1)) {
        SHUTDOWN(s);
    }
    return (ret);
}

static int init_server(int *sock, int port, int type)
{
    return (init_server_long(sock, port, NULL, type));
}

static int do_accept(int acc_sock, int *sock)
{
    int ret;

    if (!ssl_sock_init())
        return 0;

# ifndef OPENSSL_SYS_WINDOWS
 redoit:
# endif

    /*
     * Note: under VMS with SOCKETSHR the fourth parameter is currently of
     * type (int *) whereas under other systems it is (void *) if you don't
     * have a cast it will choke the compiler: if you do have a cast then you
     * can either go for (int *) or (void *).
     */
    ret = accept(acc_sock, NULL, NULL);
    if (ret == INVALID_SOCKET) {
# if defined(OPENSSL_SYS_WINDOWS) || (defined(OPENSSL_SYS_NETWARE) && !defined(NETWARE_BSDSOCK))
        int i;
        i = WSAGetLastError();
        BIO_printf(bio_err, "accept error %d\n", i);
# else
        if (errno == EINTR) {
            /*
             * check_timeout();
             */
            goto redoit;
        }
        fprintf(stderr, "errno=%d ", errno);
        perror("accept");
# endif
        return 0;
    }

    *sock = ret;
    return 1;
}

int extract_host_port(char *str, char **host_ptr, unsigned char *ip,
                      short *port_ptr)
{
    char *h, *p;

    h = str;
    p = strchr(str, ':');
    if (p == NULL) {
        BIO_printf(bio_err, "no port defined\n");
        return (0);
    }
    *(p++) = '\0';

    if ((ip != NULL) && !host_ip(str, ip))
        goto err;
    if (host_ptr != NULL)
        *host_ptr = h;

    if (!extract_port(p, port_ptr))
        goto err;
    return (1);
 err:
    return (0);
}

static int host_ip(char *str, unsigned char ip[4])
{
    unsigned int in[4];
    int i;

    if (sscanf(str, "%u.%u.%u.%u", &(in[0]), &(in[1]), &(in[2]), &(in[3])) ==
        4) {
        for (i = 0; i < 4; i++)
            if (in[i] > 255) {
                BIO_printf(bio_err, "invalid IP address\n");
                goto err;
            }
        ip[0] = in[0];
        ip[1] = in[1];
        ip[2] = in[2];
        ip[3] = in[3];
    } else {                    /* do a gethostbyname */
        struct hostent *he;

        if (!ssl_sock_init())
            return (0);

        he = GetHostByName(str);
        if (he == NULL) {
            BIO_printf(bio_err, "gethostbyname failure\n");
            goto err;
        }
        /* cast to short because of win16 winsock definition */
        if ((short)he->h_addrtype != AF_INET) {
            BIO_printf(bio_err, "gethostbyname addr is not AF_INET\n");
            return (0);
        }
        ip[0] = he->h_addr_list[0][0];
        ip[1] = he->h_addr_list[0][1];
        ip[2] = he->h_addr_list[0][2];
        ip[3] = he->h_addr_list[0][3];
    }
    return (1);
 err:
    return (0);
}

int extract_port(char *str, short *port_ptr)
{
    int i;
    struct servent *s;

    i = atoi(str);
    if (i != 0)
        *port_ptr = (unsigned short)i;
    else {
        s = getservbyname(str, "tcp");
        if (s == NULL) {
            BIO_printf(bio_err, "getservbyname failure for %s\n", str);
            return (0);
        }
        *port_ptr = ntohs((unsigned short)s->s_port);
    }
    return (1);
}

# define GHBN_NUM        4
static struct ghbn_cache_st {
    char name[128];
    struct hostent ent;
    unsigned long order;
} ghbn_cache[GHBN_NUM];

static unsigned long ghbn_hits = 0L;
static unsigned long ghbn_miss = 0L;

static struct hostent *GetHostByName(char *name)
{
    struct hostent *ret;
    int i, lowi = 0;
    unsigned long low = (unsigned long)-1;

    for (i = 0; i < GHBN_NUM; i++) {
        if (low > ghbn_cache[i].order) {
            low = ghbn_cache[i].order;
            lowi = i;
        }
        if (ghbn_cache[i].order > 0) {
            if (strncmp(name, ghbn_cache[i].name, 128) == 0)
                break;
        }
    }
    if (i == GHBN_NUM) {        /* no hit */
        ghbn_miss++;
        ret = gethostbyname(name);
        if (ret == NULL)
            return (NULL);
        /* else add to cache */
        if (strlen(name) < sizeof(ghbn_cache[0].name)) {
            strcpy(ghbn_cache[lowi].name, name);
            memcpy((char *)&(ghbn_cache[lowi].ent), ret,
                   sizeof(struct hostent));
            ghbn_cache[lowi].order = ghbn_miss + ghbn_hits;
        }
        return (ret);
    } else {
        ghbn_hits++;
        ret = &(ghbn_cache[i].ent);
        ghbn_cache[i].order = ghbn_miss + ghbn_hits;
        return (ret);
    }
}

#endif
