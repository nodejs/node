/* crypto/bio/b_sock.c */
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

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#define USE_SOCKETS
#include "cryptlib.h"
#include <openssl/bio.h>
#if defined(OPENSSL_SYS_NETWARE) && defined(NETWARE_BSDSOCK)
# include <netdb.h>
# if defined(NETWARE_CLIB)
#  include <sys/ioctl.h>
NETDB_DEFINE_CONTEXT
# endif
#endif
#ifndef OPENSSL_NO_SOCK
# include <openssl/dso.h>
# define SOCKET_PROTOCOL IPPROTO_TCP
# ifdef SO_MAXCONN
#  define MAX_LISTEN  SO_MAXCONN
# elif defined(SOMAXCONN)
#  define MAX_LISTEN  SOMAXCONN
# else
#  define MAX_LISTEN  32
# endif
# if defined(OPENSSL_SYS_WINDOWS) || (defined(OPENSSL_SYS_NETWARE) && !defined(NETWARE_BSDSOCK))
static int wsa_init_done = 0;
# endif

# if defined(__GLIBC__)
#  define HAVE_GETHOSTBYNAME_R
#  define GETHOSTNAME_R_BUF     (2 * 1024)
# endif

/*
 * WSAAPI specifier is required to make indirect calls to run-time
 * linked WinSock 2 functions used in this module, to be specific
 * [get|free]addrinfo and getnameinfo. This is because WinSock uses
 * uses non-C calling convention, __stdcall vs. __cdecl, on x86
 * Windows. On non-WinSock platforms WSAAPI needs to be void.
 */
# ifndef WSAAPI
#  define WSAAPI
# endif

# if 0
static unsigned long BIO_ghbn_hits = 0L;
static unsigned long BIO_ghbn_miss = 0L;

#  define GHBN_NUM        4
static struct ghbn_cache_st {
    char name[129];
    struct hostent *ent;
    unsigned long order;
} ghbn_cache[GHBN_NUM];
# endif

static int get_ip(const char *str, unsigned char *ip);
# if 0
static void ghbn_free(struct hostent *a);
static struct hostent *ghbn_dup(struct hostent *a);
# endif
int BIO_get_host_ip(const char *str, unsigned char *ip)
{
    int i;
    int err = 1;
    int locked = 0;
    struct hostent *he = NULL;
# ifdef HAVE_GETHOSTBYNAME_R
    char buf[GETHOSTNAME_R_BUF];
    struct hostent hostent;
    int h_errnop;
# endif

    i = get_ip(str, ip);
    if (i < 0) {
        BIOerr(BIO_F_BIO_GET_HOST_IP, BIO_R_INVALID_IP_ADDRESS);
        goto err;
    }

    /*
     * At this point, we have something that is most probably correct in some
     * way, so let's init the socket.
     */
    if (BIO_sock_init() != 1)
        return 0;               /* don't generate another error code here */

    /*
     * If the string actually contained an IP address, we need not do
     * anything more
     */
    if (i > 0)
        return (1);

    /* if gethostbyname_r is supported, use it. */
# ifdef HAVE_GETHOSTBYNAME_R
    memset(&hostent, 0x00, sizeof(hostent));
    /* gethostbyname_r() sets |he| to NULL on error, we check it further down */
    gethostbyname_r(str, &hostent, buf, sizeof(buf), &he, &h_errnop);
# else
    /* do a gethostbyname */
    CRYPTO_w_lock(CRYPTO_LOCK_GETHOSTBYNAME);
    locked = 1;
    he = BIO_gethostbyname(str);
# endif

    if (he == NULL) {
        BIOerr(BIO_F_BIO_GET_HOST_IP, BIO_R_BAD_HOSTNAME_LOOKUP);
        goto err;
    }

    /* cast to short because of win16 winsock definition */
    if ((short)he->h_addrtype != AF_INET) {
        BIOerr(BIO_F_BIO_GET_HOST_IP,
               BIO_R_GETHOSTBYNAME_ADDR_IS_NOT_AF_INET);
        goto err;
    }
    for (i = 0; i < 4; i++)
        ip[i] = he->h_addr_list[0][i];
    err = 0;

 err:
    if (locked)
        CRYPTO_w_unlock(CRYPTO_LOCK_GETHOSTBYNAME);
    if (err) {
        ERR_add_error_data(2, "host=", str);
        return 0;
    } else
        return 1;
}

int BIO_get_port(const char *str, unsigned short *port_ptr)
{
    int i;
    struct servent *s;

    if (str == NULL) {
        BIOerr(BIO_F_BIO_GET_PORT, BIO_R_NO_PORT_DEFINED);
        return (0);
    }
    i = atoi(str);
    if (i != 0)
        *port_ptr = (unsigned short)i;
    else {
        CRYPTO_w_lock(CRYPTO_LOCK_GETSERVBYNAME);
        /*
         * Note: under VMS with SOCKETSHR, it seems like the first parameter
         * is 'char *', instead of 'const char *'
         */
# ifndef CONST_STRICT
        s = getservbyname((char *)str, "tcp");
# else
        s = getservbyname(str, "tcp");
# endif
        if (s != NULL)
            *port_ptr = ntohs((unsigned short)s->s_port);
        CRYPTO_w_unlock(CRYPTO_LOCK_GETSERVBYNAME);
        if (s == NULL) {
            if (strcmp(str, "http") == 0)
                *port_ptr = 80;
            else if (strcmp(str, "telnet") == 0)
                *port_ptr = 23;
            else if (strcmp(str, "socks") == 0)
                *port_ptr = 1080;
            else if (strcmp(str, "https") == 0)
                *port_ptr = 443;
            else if (strcmp(str, "ssl") == 0)
                *port_ptr = 443;
            else if (strcmp(str, "ftp") == 0)
                *port_ptr = 21;
            else if (strcmp(str, "gopher") == 0)
                *port_ptr = 70;
# if 0
            else if (strcmp(str, "wais") == 0)
                *port_ptr = 21;
# endif
            else {
                SYSerr(SYS_F_GETSERVBYNAME, get_last_socket_error());
                ERR_add_error_data(3, "service='", str, "'");
                return (0);
            }
        }
    }
    return (1);
}

int BIO_sock_error(int sock)
{
    int j, i;
    union {
        size_t s;
        int i;
    } size;

# if defined(OPENSSL_SYS_BEOS_R5)
    return 0;
# endif

    /* heuristic way to adapt for platforms that expect 64-bit optlen */
    size.s = 0, size.i = sizeof(j);
    /*
     * Note: under Windows the third parameter is of type (char *) whereas
     * under other systems it is (void *) if you don't have a cast it will
     * choke the compiler: if you do have a cast then you can either go for
     * (char *) or (void *).
     */
    i = getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&j, (void *)&size);
    if (i < 0)
        return (1);
    else
        return (j);
}

# if 0
long BIO_ghbn_ctrl(int cmd, int iarg, char *parg)
{
    int i;
    char **p;

    switch (cmd) {
    case BIO_GHBN_CTRL_HITS:
        return (BIO_ghbn_hits);
        /* break; */
    case BIO_GHBN_CTRL_MISSES:
        return (BIO_ghbn_miss);
        /* break; */
    case BIO_GHBN_CTRL_CACHE_SIZE:
        return (GHBN_NUM);
        /* break; */
    case BIO_GHBN_CTRL_GET_ENTRY:
        if ((iarg >= 0) && (iarg < GHBN_NUM) && (ghbn_cache[iarg].order > 0)) {
            p = (char **)parg;
            if (p == NULL)
                return (0);
            *p = ghbn_cache[iarg].name;
            ghbn_cache[iarg].name[128] = '\0';
            return (1);
        }
        return (0);
        /* break; */
    case BIO_GHBN_CTRL_FLUSH:
        for (i = 0; i < GHBN_NUM; i++)
            ghbn_cache[i].order = 0;
        break;
    default:
        return (0);
    }
    return (1);
}
# endif

# if 0
static struct hostent *ghbn_dup(struct hostent *a)
{
    struct hostent *ret;
    int i, j;

    MemCheck_off();
    ret = (struct hostent *)OPENSSL_malloc(sizeof(struct hostent));
    if (ret == NULL)
        return (NULL);
    memset(ret, 0, sizeof(struct hostent));

    for (i = 0; a->h_aliases[i] != NULL; i++) ;
    i++;
    ret->h_aliases = (char **)OPENSSL_malloc(i * sizeof(char *));
    if (ret->h_aliases == NULL)
        goto err;
    memset(ret->h_aliases, 0, i * sizeof(char *));

    for (i = 0; a->h_addr_list[i] != NULL; i++) ;
    i++;
    ret->h_addr_list = (char **)OPENSSL_malloc(i * sizeof(char *));
    if (ret->h_addr_list == NULL)
        goto err;
    memset(ret->h_addr_list, 0, i * sizeof(char *));

    j = strlen(a->h_name) + 1;
    if ((ret->h_name = OPENSSL_malloc(j)) == NULL)
        goto err;
    memcpy((char *)ret->h_name, a->h_name, j);
    for (i = 0; a->h_aliases[i] != NULL; i++) {
        j = strlen(a->h_aliases[i]) + 1;
        if ((ret->h_aliases[i] = OPENSSL_malloc(j)) == NULL)
            goto err;
        memcpy(ret->h_aliases[i], a->h_aliases[i], j);
    }
    ret->h_length = a->h_length;
    ret->h_addrtype = a->h_addrtype;
    for (i = 0; a->h_addr_list[i] != NULL; i++) {
        if ((ret->h_addr_list[i] = OPENSSL_malloc(a->h_length)) == NULL)
            goto err;
        memcpy(ret->h_addr_list[i], a->h_addr_list[i], a->h_length);
    }
    if (0) {
 err:
        if (ret != NULL)
            ghbn_free(ret);
        ret = NULL;
    }
    MemCheck_on();
    return (ret);
}

static void ghbn_free(struct hostent *a)
{
    int i;

    if (a == NULL)
        return;

    if (a->h_aliases != NULL) {
        for (i = 0; a->h_aliases[i] != NULL; i++)
            OPENSSL_free(a->h_aliases[i]);
        OPENSSL_free(a->h_aliases);
    }
    if (a->h_addr_list != NULL) {
        for (i = 0; a->h_addr_list[i] != NULL; i++)
            OPENSSL_free(a->h_addr_list[i]);
        OPENSSL_free(a->h_addr_list);
    }
    if (a->h_name != NULL)
        OPENSSL_free(a->h_name);
    OPENSSL_free(a);
}

# endif

struct hostent *BIO_gethostbyname(const char *name)
{
# if 1
    /*
     * Caching gethostbyname() results forever is wrong, so we have to let
     * the true gethostbyname() worry about this
     */
#  if (defined(NETWARE_BSDSOCK) && !defined(__NOVELL_LIBC__))
    return gethostbyname((char *)name);
#  else
    return gethostbyname(name);
#  endif
# else
    struct hostent *ret;
    int i, lowi = 0, j;
    unsigned long low = (unsigned long)-1;

#  if 0
    /*
     * It doesn't make sense to use locking here: The function interface is
     * not thread-safe, because threads can never be sure when some other
     * thread destroys the data they were given a pointer to.
     */
    CRYPTO_w_lock(CRYPTO_LOCK_GETHOSTBYNAME);
#  endif
    j = strlen(name);
    if (j < 128) {
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
    } else
        i = GHBN_NUM;

    if (i == GHBN_NUM) {        /* no hit */
        BIO_ghbn_miss++;
        /*
         * Note: under VMS with SOCKETSHR, it seems like the first parameter
         * is 'char *', instead of 'const char *'
         */
#  ifndef CONST_STRICT
        ret = gethostbyname((char *)name);
#  else
        ret = gethostbyname(name);
#  endif

        if (ret == NULL)
            goto end;
        if (j > 128) {          /* too big to cache */
#  if 0
            /*
             * If we were trying to make this function thread-safe (which is
             * bound to fail), we'd have to give up in this case (or allocate
             * more memory).
             */
            ret = NULL;
#  endif
            goto end;
        }

        /* else add to cache */
        if (ghbn_cache[lowi].ent != NULL)
            ghbn_free(ghbn_cache[lowi].ent); /* XXX not thread-safe */
        ghbn_cache[lowi].name[0] = '\0';

        if ((ret = ghbn_cache[lowi].ent = ghbn_dup(ret)) == NULL) {
            BIOerr(BIO_F_BIO_GETHOSTBYNAME, ERR_R_MALLOC_FAILURE);
            goto end;
        }
        strncpy(ghbn_cache[lowi].name, name, 128);
        ghbn_cache[lowi].order = BIO_ghbn_miss + BIO_ghbn_hits;
    } else {
        BIO_ghbn_hits++;
        ret = ghbn_cache[i].ent;
        ghbn_cache[i].order = BIO_ghbn_miss + BIO_ghbn_hits;
    }
 end:
#  if 0
    CRYPTO_w_unlock(CRYPTO_LOCK_GETHOSTBYNAME);
#  endif
    return (ret);
# endif
}

int BIO_sock_init(void)
{
# ifdef OPENSSL_SYS_WINDOWS
    static struct WSAData wsa_state;

    if (!wsa_init_done) {
        int err;

        wsa_init_done = 1;
        memset(&wsa_state, 0, sizeof(wsa_state));
        /*
         * Not making wsa_state available to the rest of the code is formally
         * wrong. But the structures we use are [beleived to be] invariable
         * among Winsock DLLs, while API availability is [expected to be]
         * probed at run-time with DSO_global_lookup.
         */
        if (WSAStartup(0x0202, &wsa_state) != 0) {
            err = WSAGetLastError();
            SYSerr(SYS_F_WSASTARTUP, err);
            BIOerr(BIO_F_BIO_SOCK_INIT, BIO_R_WSASTARTUP);
            return (-1);
        }
    }
# endif                         /* OPENSSL_SYS_WINDOWS */
# ifdef WATT32
    extern int _watt_do_exit;
    _watt_do_exit = 0;          /* don't make sock_init() call exit() */
    if (sock_init())
        return (-1);
# endif

# if defined(OPENSSL_SYS_NETWARE) && !defined(NETWARE_BSDSOCK)
    WORD wVerReq;
    WSADATA wsaData;
    int err;

    if (!wsa_init_done) {
        wsa_init_done = 1;
        wVerReq = MAKEWORD(2, 0);
        err = WSAStartup(wVerReq, &wsaData);
        if (err != 0) {
            SYSerr(SYS_F_WSASTARTUP, err);
            BIOerr(BIO_F_BIO_SOCK_INIT, BIO_R_WSASTARTUP);
            return (-1);
        }
    }
# endif

    return (1);
}

void BIO_sock_cleanup(void)
{
# ifdef OPENSSL_SYS_WINDOWS
    if (wsa_init_done) {
        wsa_init_done = 0;
#  if 0                         /* this call is claimed to be non-present in
                                 * Winsock2 */
        WSACancelBlockingCall();
#  endif
        WSACleanup();
    }
# elif defined(OPENSSL_SYS_NETWARE) && !defined(NETWARE_BSDSOCK)
    if (wsa_init_done) {
        wsa_init_done = 0;
        WSACleanup();
    }
# endif
}

# if !defined(OPENSSL_SYS_VMS) || __VMS_VER >= 70000000

int BIO_socket_ioctl(int fd, long type, void *arg)
{
    int i;

#  ifdef __DJGPP__
    i = ioctlsocket(fd, type, (char *)arg);
#  else
#   if defined(OPENSSL_SYS_VMS)
    /*-
     * 2011-02-18 SMS.
     * VMS ioctl() can't tolerate a 64-bit "void *arg", but we
     * observe that all the consumers pass in an "unsigned long *",
     * so we arrange a local copy with a short pointer, and use
     * that, instead.
     */
#    if __INITIAL_POINTER_SIZE == 64
#     define ARG arg_32p
#     pragma pointer_size save
#     pragma pointer_size 32
    unsigned long arg_32;
    unsigned long *arg_32p;
#     pragma pointer_size restore
    arg_32p = &arg_32;
    arg_32 = *((unsigned long *)arg);
#    else                       /* __INITIAL_POINTER_SIZE == 64 */
#     define ARG arg
#    endif                      /* __INITIAL_POINTER_SIZE == 64 [else] */
#   else                        /* defined(OPENSSL_SYS_VMS) */
#    define ARG arg
#   endif                       /* defined(OPENSSL_SYS_VMS) [else] */

    i = ioctlsocket(fd, type, ARG);
#  endif                        /* __DJGPP__ */
    if (i < 0)
        SYSerr(SYS_F_IOCTLSOCKET, get_last_socket_error());
    return (i);
}
# endif                         /* __VMS_VER */

/*
 * The reason I have implemented this instead of using sscanf is because
 * Visual C 1.52c gives an unresolved external when linking a DLL :-(
 */
static int get_ip(const char *str, unsigned char ip[4])
{
    unsigned int tmp[4];
    int num = 0, c, ok = 0;

    tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0;

    for (;;) {
        c = *(str++);
        if ((c >= '0') && (c <= '9')) {
            ok = 1;
            tmp[num] = tmp[num] * 10 + c - '0';
            if (tmp[num] > 255)
                return (0);
        } else if (c == '.') {
            if (!ok)
                return (-1);
            if (num == 3)
                return (0);
            num++;
            ok = 0;
        } else if (c == '\0' && (num == 3) && ok)
            break;
        else
            return (0);
    }
    ip[0] = tmp[0];
    ip[1] = tmp[1];
    ip[2] = tmp[2];
    ip[3] = tmp[3];
    return (1);
}

int BIO_get_accept_socket(char *host, int bind_mode)
{
    int ret = 0;
    union {
        struct sockaddr sa;
        struct sockaddr_in sa_in;
# if OPENSSL_USE_IPV6
        struct sockaddr_in6 sa_in6;
# endif
    } server, client;
    int s = INVALID_SOCKET, cs, addrlen;
    unsigned char ip[4];
    unsigned short port;
    char *str = NULL, *e;
    char *h, *p;
    unsigned long l;
    int err_num;

    if (BIO_sock_init() != 1)
        return (INVALID_SOCKET);

    if ((str = BUF_strdup(host)) == NULL)
        return (INVALID_SOCKET);

    h = p = NULL;
    h = str;
    for (e = str; *e; e++) {
        if (*e == ':') {
            p = e;
        } else if (*e == '/') {
            *e = '\0';
            break;
        }
    }
    if (p)
        *p++ = '\0';            /* points at last ':', '::port' is special
                                 * [see below] */
    else
        p = h, h = NULL;

# ifdef EAI_FAMILY
    do {
        static union {
            void *p;
            int (WSAAPI *f) (const char *, const char *,
                             const struct addrinfo *, struct addrinfo **);
        } p_getaddrinfo = {
            NULL
        };
        static union {
            void *p;
            void (WSAAPI *f) (struct addrinfo *);
        } p_freeaddrinfo = {
            NULL
        };
        struct addrinfo *res, hint;

        if (p_getaddrinfo.p == NULL) {
            if ((p_getaddrinfo.p = DSO_global_lookup("getaddrinfo")) == NULL
                || (p_freeaddrinfo.p =
                    DSO_global_lookup("freeaddrinfo")) == NULL)
                p_getaddrinfo.p = (void *)-1;
        }
        if (p_getaddrinfo.p == (void *)-1)
            break;

        /*
         * '::port' enforces IPv6 wildcard listener. Some OSes, e.g. Solaris,
         * default to IPv6 without any hint. Also note that commonly IPv6
         * wildchard socket can service IPv4 connections just as well...
         */
        memset(&hint, 0, sizeof(hint));
        hint.ai_flags = AI_PASSIVE;
        if (h) {
            if (strchr(h, ':')) {
                if (h[1] == '\0')
                    h = NULL;
#  if OPENSSL_USE_IPV6
                hint.ai_family = AF_INET6;
#  else
                h = NULL;
#  endif
            } else if (h[0] == '*' && h[1] == '\0') {
                hint.ai_family = AF_INET;
                h = NULL;
            }
        }

        if ((*p_getaddrinfo.f) (h, p, &hint, &res))
            break;

        addrlen = res->ai_addrlen <= sizeof(server) ?
            res->ai_addrlen : sizeof(server);
        memcpy(&server, res->ai_addr, addrlen);

        (*p_freeaddrinfo.f) (res);
        goto again;
    } while (0);
# endif

    if (!BIO_get_port(p, &port))
        goto err;

    memset((char *)&server, 0, sizeof(server));
    server.sa_in.sin_family = AF_INET;
    server.sa_in.sin_port = htons(port);
    addrlen = sizeof(server.sa_in);

    if (h == NULL || strcmp(h, "*") == 0)
        server.sa_in.sin_addr.s_addr = INADDR_ANY;
    else {
        if (!BIO_get_host_ip(h, &(ip[0])))
            goto err;
        l = (unsigned long)
            ((unsigned long)ip[0] << 24L) |
            ((unsigned long)ip[1] << 16L) |
            ((unsigned long)ip[2] << 8L) | ((unsigned long)ip[3]);
        server.sa_in.sin_addr.s_addr = htonl(l);
    }

 again:
    s = socket(server.sa.sa_family, SOCK_STREAM, SOCKET_PROTOCOL);
    if (s == INVALID_SOCKET) {
        SYSerr(SYS_F_SOCKET, get_last_socket_error());
        ERR_add_error_data(3, "port='", host, "'");
        BIOerr(BIO_F_BIO_GET_ACCEPT_SOCKET, BIO_R_UNABLE_TO_CREATE_SOCKET);
        goto err;
    }
# ifdef SO_REUSEADDR
    if (bind_mode == BIO_BIND_REUSEADDR) {
        int i = 1;

        ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&i, sizeof(i));
        bind_mode = BIO_BIND_NORMAL;
    }
# endif
    if (bind(s, &server.sa, addrlen) == -1) {
# ifdef SO_REUSEADDR
        err_num = get_last_socket_error();
        if ((bind_mode == BIO_BIND_REUSEADDR_IF_UNUSED) &&
#  ifdef OPENSSL_SYS_WINDOWS
            /*
             * Some versions of Windows define EADDRINUSE to a dummy value.
             */
            (err_num == WSAEADDRINUSE))
#  else
            (err_num == EADDRINUSE))
#  endif
        {
            client = server;
            if (h == NULL || strcmp(h, "*") == 0) {
#  if OPENSSL_USE_IPV6
                if (client.sa.sa_family == AF_INET6) {
                    memset(&client.sa_in6.sin6_addr, 0,
                           sizeof(client.sa_in6.sin6_addr));
                    client.sa_in6.sin6_addr.s6_addr[15] = 1;
                } else
#  endif
                if (client.sa.sa_family == AF_INET) {
                    client.sa_in.sin_addr.s_addr = htonl(0x7F000001);
                } else
                    goto err;
            }
            cs = socket(client.sa.sa_family, SOCK_STREAM, SOCKET_PROTOCOL);
            if (cs != INVALID_SOCKET) {
                int ii;
                ii = connect(cs, &client.sa, addrlen);
                closesocket(cs);
                if (ii == INVALID_SOCKET) {
                    bind_mode = BIO_BIND_REUSEADDR;
                    closesocket(s);
                    goto again;
                }
                /* else error */
            }
            /* else error */
        }
# endif
        SYSerr(SYS_F_BIND, err_num);
        ERR_add_error_data(3, "port='", host, "'");
        BIOerr(BIO_F_BIO_GET_ACCEPT_SOCKET, BIO_R_UNABLE_TO_BIND_SOCKET);
        goto err;
    }
    if (listen(s, MAX_LISTEN) == -1) {
        SYSerr(SYS_F_BIND, get_last_socket_error());
        ERR_add_error_data(3, "port='", host, "'");
        BIOerr(BIO_F_BIO_GET_ACCEPT_SOCKET, BIO_R_UNABLE_TO_LISTEN_SOCKET);
        goto err;
    }
    ret = 1;
 err:
    if (str != NULL)
        OPENSSL_free(str);
    if ((ret == 0) && (s != INVALID_SOCKET)) {
        closesocket(s);
        s = INVALID_SOCKET;
    }
    return (s);
}

int BIO_accept(int sock, char **addr)
{
    int ret = INVALID_SOCKET;
    unsigned long l;
    unsigned short port;
    char *p;

    struct {
        /*
         * As for following union. Trouble is that there are platforms
         * that have socklen_t and there are platforms that don't, on
         * some platforms socklen_t is int and on some size_t. So what
         * one can do? One can cook #ifdef spaghetti, which is nothing
         * but masochistic. Or one can do union between int and size_t.
         * One naturally does it primarily for 64-bit platforms where
         * sizeof(int) != sizeof(size_t). But would it work? Note that
         * if size_t member is initialized to 0, then later int member
         * assignment naturally does the job on little-endian platforms
         * regardless accept's expectations! What about big-endians?
         * If accept expects int*, then it works, and if size_t*, then
         * length value would appear as unreasonably large. But this
         * won't prevent it from filling in the address structure. The
         * trouble of course would be if accept returns more data than
         * actual buffer can accomodate and overwrite stack... That's
         * where early OPENSSL_assert comes into picture. Besides, the
         * only 64-bit big-endian platform found so far that expects
         * size_t* is HP-UX, where stack grows towards higher address.
         * <appro>
         */
        union {
            size_t s;
            int i;
        } len;
        union {
            struct sockaddr sa;
            struct sockaddr_in sa_in;
# if OPENSSL_USE_IPV6
            struct sockaddr_in6 sa_in6;
# endif
        } from;
    } sa;

    sa.len.s = 0;
    sa.len.i = sizeof(sa.from);
    memset(&sa.from, 0, sizeof(sa.from));
    ret = accept(sock, &sa.from.sa, (void *)&sa.len);
    if (sizeof(sa.len.i) != sizeof(sa.len.s) && sa.len.i == 0) {
        OPENSSL_assert(sa.len.s <= sizeof(sa.from));
        sa.len.i = (int)sa.len.s;
        /* use sa.len.i from this point */
    }
    if (ret == INVALID_SOCKET) {
        if (BIO_sock_should_retry(ret))
            return -2;
        SYSerr(SYS_F_ACCEPT, get_last_socket_error());
        BIOerr(BIO_F_BIO_ACCEPT, BIO_R_ACCEPT_ERROR);
        goto end;
    }

    if (addr == NULL)
        goto end;

# ifdef EAI_FAMILY
    do {
        char h[NI_MAXHOST], s[NI_MAXSERV];
        size_t nl;
        static union {
            void *p;
            int (WSAAPI *f) (const struct sockaddr *, size_t /* socklen_t */ ,
                             char *, size_t, char *, size_t, int);
        } p_getnameinfo = {
            NULL
        };
        /*
         * 2nd argument to getnameinfo is specified to be socklen_t.
         * Unfortunately there is a number of environments where socklen_t is
         * not defined. As it's passed by value, it's safe to pass it as
         * size_t... <appro>
         */

        if (p_getnameinfo.p == NULL) {
            if ((p_getnameinfo.p = DSO_global_lookup("getnameinfo")) == NULL)
                p_getnameinfo.p = (void *)-1;
        }
        if (p_getnameinfo.p == (void *)-1)
            break;

        if ((*p_getnameinfo.f) (&sa.from.sa, sa.len.i, h, sizeof(h), s,
                                sizeof(s), NI_NUMERICHOST | NI_NUMERICSERV))
            break;
        nl = strlen(h) + strlen(s) + 2;
        p = *addr;
        if (p) {
            *p = '\0';
            p = OPENSSL_realloc(p, nl);
        } else {
            p = OPENSSL_malloc(nl);
        }
        if (p == NULL) {
            BIOerr(BIO_F_BIO_ACCEPT, ERR_R_MALLOC_FAILURE);
            goto end;
        }
        *addr = p;
        BIO_snprintf(*addr, nl, "%s:%s", h, s);
        goto end;
    } while (0);
# endif
    if (sa.from.sa.sa_family != AF_INET)
        goto end;
    l = ntohl(sa.from.sa_in.sin_addr.s_addr);
    port = ntohs(sa.from.sa_in.sin_port);
    if (*addr == NULL) {
        if ((p = OPENSSL_malloc(24)) == NULL) {
            BIOerr(BIO_F_BIO_ACCEPT, ERR_R_MALLOC_FAILURE);
            goto end;
        }
        *addr = p;
    }
    BIO_snprintf(*addr, 24, "%d.%d.%d.%d:%d",
                 (unsigned char)(l >> 24L) & 0xff,
                 (unsigned char)(l >> 16L) & 0xff,
                 (unsigned char)(l >> 8L) & 0xff,
                 (unsigned char)(l) & 0xff, port);
 end:
    return (ret);
}

int BIO_set_tcp_ndelay(int s, int on)
{
    int ret = 0;
# if defined(TCP_NODELAY) && (defined(IPPROTO_TCP) || defined(SOL_TCP))
    int opt;

#  ifdef SOL_TCP
    opt = SOL_TCP;
#  else
#   ifdef IPPROTO_TCP
    opt = IPPROTO_TCP;
#   endif
#  endif

    ret = setsockopt(s, opt, TCP_NODELAY, (char *)&on, sizeof(on));
# endif
    return (ret == 0);
}

int BIO_socket_nbio(int s, int mode)
{
    int ret = -1;
    int l;

    l = mode;
# ifdef FIONBIO
    ret = BIO_socket_ioctl(s, FIONBIO, &l);
# endif
    return (ret == 0);
}
#endif
