/*
 * Copyright 2005-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_LIBCRYPTO_BIO_BIO_LOCAL_H)
#define OSSL_LIBCRYPTO_BIO_BIO_LOCAL_H

#include "internal/e_os.h"
#include "internal/sockets.h"
#include "internal/bio_addr.h"

/* BEGIN BIO_ADDRINFO/BIO_ADDR stuff. */

#ifndef OPENSSL_NO_SOCK
/*
 * Throughout this file and b_addr.c, the existence of the macro
 * AI_PASSIVE is used to detect the availability of struct addrinfo,
 * getnameinfo() and getaddrinfo().  If that macro doesn't exist,
 * we use our own implementation instead.
 */

/*
 * It's imperative that these macros get defined before openssl/bio.h gets
 * included.  Otherwise, the AI_PASSIVE hack will not work properly.
 * For clarity, we check for internal/cryptlib.h since it's a common header
 * that also includes bio.h.
 */
#ifdef OSSL_INTERNAL_CRYPTLIB_H
#error internal/cryptlib.h included before bio_local.h
#endif
#ifdef OPENSSL_BIO_H
#error openssl/bio.h included before bio_local.h
#endif

#ifdef AI_PASSIVE

/*
 * There's a bug in VMS C header file netdb.h, where struct addrinfo
 * always is the P32 variant, but the functions that handle that structure,
 * such as getaddrinfo() and freeaddrinfo() adapt to the initial pointer
 * size.  The easiest workaround is to force struct addrinfo to be the
 * 64-bit variant when compiling in P64 mode.
 */
#if defined(OPENSSL_SYS_VMS) && __INITIAL_POINTER_SIZE == 64
#define addrinfo __addrinfo64
#endif

#define bio_addrinfo_st addrinfo
#define bai_family ai_family
#define bai_socktype ai_socktype
#define bai_protocol ai_protocol
#define bai_addrlen ai_addrlen
#define bai_addr ai_addr
#define bai_next ai_next
#else
struct bio_addrinfo_st {
    int bai_family;
    int bai_socktype;
    int bai_protocol;
    size_t bai_addrlen;
    struct sockaddr *bai_addr;
    struct bio_addrinfo_st *bai_next;
};
#endif
#endif

/* END BIO_ADDRINFO/BIO_ADDR stuff. */

#include "internal/cryptlib.h"
#include "internal/bio.h"
#include "internal/refcount.h"
#include "internal/time.h"

typedef struct bio_f_buffer_ctx_struct {
    /*-
     * Buffers are setup like this:
     *
     * <---------------------- size ----------------------->
     * +---------------------------------------------------+
     * | consumed | remaining          | free space        |
     * +---------------------------------------------------+
     * <-- off --><------- len ------->
     */
    /*- BIO *bio; */ /*
                      * this is now in the BIO struct
                      */
    int ibuf_size; /* how big is the input buffer */
    int obuf_size; /* how big is the output buffer */
    char *ibuf; /* the char array */
    int ibuf_len; /* how many bytes are in it */
    int ibuf_off; /* write/read offset */
    char *obuf; /* the char array */
    int obuf_len; /* how many bytes are in it */
    int obuf_off; /* write/read offset */
#ifndef OPENSSL_NO_SOCK
    BIO_ADDR *peer;
#endif
} BIO_F_BUFFER_CTX;

struct bio_st {
    OSSL_LIB_CTX *libctx;
    const BIO_METHOD *method;
    /* bio, mode, argp, argi, argl, ret */
#ifndef OPENSSL_NO_DEPRECATED_3_0
    BIO_callback_fn callback;
#endif
    BIO_callback_fn_ex callback_ex;
    char *cb_arg; /* first argument for the callback */
    int init;
    int shutdown;
    int flags; /* extra storage */
    int retry_reason;
    int num;
    void *ptr;
    struct bio_st *next_bio; /* used by filter BIOs */
    struct bio_st *prev_bio; /* used by filter BIOs */
    CRYPTO_REF_COUNT references;
    uint64_t num_read;
    uint64_t num_write;
    CRYPTO_EX_DATA ex_data;
};

#ifndef OPENSSL_NO_SOCK

typedef struct bio_connect_st {
    int state;
    int connect_family;
    int connect_sock_type;
    char *param_hostname;
    char *param_service;
    int connect_mode;
#ifndef OPENSSL_NO_KTLS
    unsigned char record_type;
#endif
    int tfo_first;

    BIO_ADDRINFO *addr_first;
    const BIO_ADDRINFO *addr_iter;
    /*
     * int socket; this will be kept in bio->num so that it is compatible
     * with the bss_sock bio
     */
    /*
     * called when the connection is initially made callback(BIO,state,ret);
     * The callback should return 'ret'.  state is for compatibility with the
     * ssl info_callback
     */
    BIO_info_cb *info_callback;
    /*
     * Used when connect_sock_type is SOCK_DGRAM. Owned by us; we forward
     * read/write(mmsg) calls to this if present.
     */
    BIO *dgram_bio;
} BIO_CONNECT;

typedef struct bio_accept_st {
    int state;
    int accept_family;
    int bind_mode; /* Socket mode for BIO_listen */
    int accepted_mode; /* Socket mode for BIO_accept (set on accepted sock) */
    char *param_addr;
    char *param_serv;

    int accept_sock;

    BIO_ADDRINFO *addr_first;
    const BIO_ADDRINFO *addr_iter;
    BIO_ADDR cache_accepting_addr; /* Useful if we asked for port 0 */
    char *cache_accepting_name, *cache_accepting_serv;
    BIO_ADDR cache_peer_addr;
    char *cache_peer_name, *cache_peer_serv;

    BIO *bio_chain;
} BIO_ACCEPT;

#ifndef OPENSSL_NO_DGRAM
typedef struct bio_dgram_data_st {
    BIO_ADDR peer;
    BIO_ADDR local_addr;
    unsigned int connected;
    unsigned int _errno;
    unsigned int mtu;
    OSSL_TIME next_timeout;
    OSSL_TIME socket_timeout;
    unsigned int peekmode;
    char local_addr_enabled;
} bio_dgram_data;
#endif

#define BIO_CONN_S_BEFORE 1
#define BIO_CONN_S_GET_ADDR 2
#define BIO_CONN_S_CREATE_SOCKET 3
#define BIO_CONN_S_CONNECT 4
#define BIO_CONN_S_OK 5
#define BIO_CONN_S_BLOCKED_CONNECT 6
#define BIO_CONN_S_CONNECT_ERROR 7

#define BIO_ACPT_S_BEFORE 1
#define BIO_ACPT_S_GET_ADDR 2
#define BIO_ACPT_S_CREATE_SOCKET 3
#define BIO_ACPT_S_LISTEN 4
#define BIO_ACPT_S_ACCEPT 5
#define BIO_ACPT_S_OK 6

#ifdef OPENSSL_SYS_VMS
typedef unsigned int socklen_t;
#endif

extern CRYPTO_RWLOCK *bio_lookup_lock;

int BIO_ADDR_make(BIO_ADDR *ap, const struct sockaddr *sa);
const struct sockaddr *BIO_ADDR_sockaddr(const BIO_ADDR *ap);
struct sockaddr *BIO_ADDR_sockaddr_noconst(BIO_ADDR *ap);
socklen_t BIO_ADDR_sockaddr_size(const BIO_ADDR *ap);
socklen_t BIO_ADDRINFO_sockaddr_size(const BIO_ADDRINFO *bai);
const struct sockaddr *BIO_ADDRINFO_sockaddr(const BIO_ADDRINFO *bai);

#if defined(OPENSSL_SYS_WINDOWS) && defined(WSAID_WSARECVMSG)
#define BIO_HAVE_WSAMSG
extern LPFN_WSARECVMSG bio_WSARecvMsg;
extern LPFN_WSASENDMSG bio_WSASendMsg;
#endif
#endif

extern CRYPTO_REF_COUNT bio_type_count;

void bio_sock_cleanup_int(void);

#if BIO_FLAGS_UPLINK_INTERNAL == 0
/* Shortcut UPLINK calls on most platforms... */
#define UP_stdin stdin
#define UP_stdout stdout
#define UP_stderr stderr
#define UP_fprintf fprintf
#define UP_fgets fgets
#define UP_fread fread
#define UP_fwrite fwrite
#undef UP_fsetmod
#define UP_feof feof
#define UP_fclose fclose

#define UP_fopen fopen
#define UP_fseek fseek
#define UP_ftell ftell
#define UP_fflush fflush
#define UP_ferror ferror
#ifdef _WIN32
#define UP_fileno _fileno
#define UP_open _open
#define UP_read _read
#define UP_write _write
#define UP_lseek _lseek
#define UP_close _close
#else
#define UP_fileno fileno
#define UP_open open
#define UP_read read
#define UP_write write
#define UP_lseek lseek
#define UP_close close
#endif

#endif

#endif /* !defined(OSSL_LIBCRYPTO_BIO_BIO_LOCAL_H) */
