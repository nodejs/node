/* crypto/bio/bio.h */
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

#ifndef HEADER_BIO_H
# define HEADER_BIO_H

# include <openssl/e_os2.h>

# ifndef OPENSSL_NO_FP_API
#  include <stdio.h>
# endif
# include <stdarg.h>

# include <openssl/crypto.h>

# ifndef OPENSSL_NO_SCTP
#  ifndef OPENSSL_SYS_VMS
#   include <stdint.h>
#  else
#   include <inttypes.h>
#  endif
# endif

#ifdef  __cplusplus
extern "C" {
#endif

/* These are the 'types' of BIOs */
# define BIO_TYPE_NONE           0
# define BIO_TYPE_MEM            (1|0x0400)
# define BIO_TYPE_FILE           (2|0x0400)

# define BIO_TYPE_FD             (4|0x0400|0x0100)
# define BIO_TYPE_SOCKET         (5|0x0400|0x0100)
# define BIO_TYPE_NULL           (6|0x0400)
# define BIO_TYPE_SSL            (7|0x0200)
# define BIO_TYPE_MD             (8|0x0200)/* passive filter */
# define BIO_TYPE_BUFFER         (9|0x0200)/* filter */
# define BIO_TYPE_CIPHER         (10|0x0200)/* filter */
# define BIO_TYPE_BASE64         (11|0x0200)/* filter */
# define BIO_TYPE_CONNECT        (12|0x0400|0x0100)/* socket - connect */
# define BIO_TYPE_ACCEPT         (13|0x0400|0x0100)/* socket for accept */
# define BIO_TYPE_PROXY_CLIENT   (14|0x0200)/* client proxy BIO */
# define BIO_TYPE_PROXY_SERVER   (15|0x0200)/* server proxy BIO */
# define BIO_TYPE_NBIO_TEST      (16|0x0200)/* server proxy BIO */
# define BIO_TYPE_NULL_FILTER    (17|0x0200)
# define BIO_TYPE_BER            (18|0x0200)/* BER -> bin filter */
# define BIO_TYPE_BIO            (19|0x0400)/* (half a) BIO pair */
# define BIO_TYPE_LINEBUFFER     (20|0x0200)/* filter */
# define BIO_TYPE_DGRAM          (21|0x0400|0x0100)
# ifndef OPENSSL_NO_SCTP
#  define BIO_TYPE_DGRAM_SCTP     (24|0x0400|0x0100)
# endif
# define BIO_TYPE_ASN1           (22|0x0200)/* filter */
# define BIO_TYPE_COMP           (23|0x0200)/* filter */

# define BIO_TYPE_DESCRIPTOR     0x0100/* socket, fd, connect or accept */
# define BIO_TYPE_FILTER         0x0200
# define BIO_TYPE_SOURCE_SINK    0x0400

/*
 * BIO_FILENAME_READ|BIO_CLOSE to open or close on free.
 * BIO_set_fp(in,stdin,BIO_NOCLOSE);
 */
# define BIO_NOCLOSE             0x00
# define BIO_CLOSE               0x01

/*
 * These are used in the following macros and are passed to BIO_ctrl()
 */
# define BIO_CTRL_RESET          1/* opt - rewind/zero etc */
# define BIO_CTRL_EOF            2/* opt - are we at the eof */
# define BIO_CTRL_INFO           3/* opt - extra tit-bits */
# define BIO_CTRL_SET            4/* man - set the 'IO' type */
# define BIO_CTRL_GET            5/* man - get the 'IO' type */
# define BIO_CTRL_PUSH           6/* opt - internal, used to signify change */
# define BIO_CTRL_POP            7/* opt - internal, used to signify change */
# define BIO_CTRL_GET_CLOSE      8/* man - set the 'close' on free */
# define BIO_CTRL_SET_CLOSE      9/* man - set the 'close' on free */
# define BIO_CTRL_PENDING        10/* opt - is their more data buffered */
# define BIO_CTRL_FLUSH          11/* opt - 'flush' buffered output */
# define BIO_CTRL_DUP            12/* man - extra stuff for 'duped' BIO */
# define BIO_CTRL_WPENDING       13/* opt - number of bytes still to write */
/* callback is int cb(BIO *bio,state,ret); */
# define BIO_CTRL_SET_CALLBACK   14/* opt - set callback function */
# define BIO_CTRL_GET_CALLBACK   15/* opt - set callback function */

# define BIO_CTRL_SET_FILENAME   30/* BIO_s_file special */

/* dgram BIO stuff */
# define BIO_CTRL_DGRAM_CONNECT       31/* BIO dgram special */
# define BIO_CTRL_DGRAM_SET_CONNECTED 32/* allow for an externally connected
                                         * socket to be passed in */
# define BIO_CTRL_DGRAM_SET_RECV_TIMEOUT 33/* setsockopt, essentially */
# define BIO_CTRL_DGRAM_GET_RECV_TIMEOUT 34/* getsockopt, essentially */
# define BIO_CTRL_DGRAM_SET_SEND_TIMEOUT 35/* setsockopt, essentially */
# define BIO_CTRL_DGRAM_GET_SEND_TIMEOUT 36/* getsockopt, essentially */

# define BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP 37/* flag whether the last */
# define BIO_CTRL_DGRAM_GET_SEND_TIMER_EXP 38/* I/O operation tiemd out */

/* #ifdef IP_MTU_DISCOVER */
# define BIO_CTRL_DGRAM_MTU_DISCOVER       39/* set DF bit on egress packets */
/* #endif */

# define BIO_CTRL_DGRAM_QUERY_MTU          40/* as kernel for current MTU */
# define BIO_CTRL_DGRAM_GET_FALLBACK_MTU   47
# define BIO_CTRL_DGRAM_GET_MTU            41/* get cached value for MTU */
# define BIO_CTRL_DGRAM_SET_MTU            42/* set cached value for MTU.
                                              * want to use this if asking
                                              * the kernel fails */

# define BIO_CTRL_DGRAM_MTU_EXCEEDED       43/* check whether the MTU was
                                              * exceed in the previous write
                                              * operation */

# define BIO_CTRL_DGRAM_GET_PEER           46
# define BIO_CTRL_DGRAM_SET_PEER           44/* Destination for the data */

# define BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT   45/* Next DTLS handshake timeout
                                              * to adjust socket timeouts */

# define BIO_CTRL_DGRAM_GET_MTU_OVERHEAD   49

# ifndef OPENSSL_NO_SCTP
/* SCTP stuff */
#  define BIO_CTRL_DGRAM_SCTP_SET_IN_HANDSHAKE    50
#  define BIO_CTRL_DGRAM_SCTP_ADD_AUTH_KEY                51
#  define BIO_CTRL_DGRAM_SCTP_NEXT_AUTH_KEY               52
#  define BIO_CTRL_DGRAM_SCTP_AUTH_CCS_RCVD               53
#  define BIO_CTRL_DGRAM_SCTP_GET_SNDINFO         60
#  define BIO_CTRL_DGRAM_SCTP_SET_SNDINFO         61
#  define BIO_CTRL_DGRAM_SCTP_GET_RCVINFO         62
#  define BIO_CTRL_DGRAM_SCTP_SET_RCVINFO         63
#  define BIO_CTRL_DGRAM_SCTP_GET_PRINFO                  64
#  define BIO_CTRL_DGRAM_SCTP_SET_PRINFO                  65
#  define BIO_CTRL_DGRAM_SCTP_SAVE_SHUTDOWN               70
# endif

/* modifiers */
# define BIO_FP_READ             0x02
# define BIO_FP_WRITE            0x04
# define BIO_FP_APPEND           0x08
# define BIO_FP_TEXT             0x10

# define BIO_FLAGS_READ          0x01
# define BIO_FLAGS_WRITE         0x02
# define BIO_FLAGS_IO_SPECIAL    0x04
# define BIO_FLAGS_RWS (BIO_FLAGS_READ|BIO_FLAGS_WRITE|BIO_FLAGS_IO_SPECIAL)
# define BIO_FLAGS_SHOULD_RETRY  0x08
# ifndef BIO_FLAGS_UPLINK
/*
 * "UPLINK" flag denotes file descriptors provided by application. It
 * defaults to 0, as most platforms don't require UPLINK interface.
 */
#  define BIO_FLAGS_UPLINK        0
# endif

/* Used in BIO_gethostbyname() */
# define BIO_GHBN_CTRL_HITS              1
# define BIO_GHBN_CTRL_MISSES            2
# define BIO_GHBN_CTRL_CACHE_SIZE        3
# define BIO_GHBN_CTRL_GET_ENTRY         4
# define BIO_GHBN_CTRL_FLUSH             5

/* Mostly used in the SSL BIO */
/*-
 * Not used anymore
 * #define BIO_FLAGS_PROTOCOL_DELAYED_READ 0x10
 * #define BIO_FLAGS_PROTOCOL_DELAYED_WRITE 0x20
 * #define BIO_FLAGS_PROTOCOL_STARTUP   0x40
 */

# define BIO_FLAGS_BASE64_NO_NL  0x100

/*
 * This is used with memory BIOs: it means we shouldn't free up or change the
 * data in any way.
 */
# define BIO_FLAGS_MEM_RDONLY    0x200

typedef struct bio_st BIO;

void BIO_set_flags(BIO *b, int flags);
int BIO_test_flags(const BIO *b, int flags);
void BIO_clear_flags(BIO *b, int flags);

# define BIO_get_flags(b) BIO_test_flags(b, ~(0x0))
# define BIO_set_retry_special(b) \
                BIO_set_flags(b, (BIO_FLAGS_IO_SPECIAL|BIO_FLAGS_SHOULD_RETRY))
# define BIO_set_retry_read(b) \
                BIO_set_flags(b, (BIO_FLAGS_READ|BIO_FLAGS_SHOULD_RETRY))
# define BIO_set_retry_write(b) \
                BIO_set_flags(b, (BIO_FLAGS_WRITE|BIO_FLAGS_SHOULD_RETRY))

/* These are normally used internally in BIOs */
# define BIO_clear_retry_flags(b) \
                BIO_clear_flags(b, (BIO_FLAGS_RWS|BIO_FLAGS_SHOULD_RETRY))
# define BIO_get_retry_flags(b) \
                BIO_test_flags(b, (BIO_FLAGS_RWS|BIO_FLAGS_SHOULD_RETRY))

/* These should be used by the application to tell why we should retry */
# define BIO_should_read(a)              BIO_test_flags(a, BIO_FLAGS_READ)
# define BIO_should_write(a)             BIO_test_flags(a, BIO_FLAGS_WRITE)
# define BIO_should_io_special(a)        BIO_test_flags(a, BIO_FLAGS_IO_SPECIAL)
# define BIO_retry_type(a)               BIO_test_flags(a, BIO_FLAGS_RWS)
# define BIO_should_retry(a)             BIO_test_flags(a, BIO_FLAGS_SHOULD_RETRY)

/*
 * The next three are used in conjunction with the BIO_should_io_special()
 * condition.  After this returns true, BIO *BIO_get_retry_BIO(BIO *bio, int
 * *reason); will walk the BIO stack and return the 'reason' for the special
 * and the offending BIO. Given a BIO, BIO_get_retry_reason(bio) will return
 * the code.
 */
/*
 * Returned from the SSL bio when the certificate retrieval code had an error
 */
# define BIO_RR_SSL_X509_LOOKUP          0x01
/* Returned from the connect BIO when a connect would have blocked */
# define BIO_RR_CONNECT                  0x02
/* Returned from the accept BIO when an accept would have blocked */
# define BIO_RR_ACCEPT                   0x03

/* These are passed by the BIO callback */
# define BIO_CB_FREE     0x01
# define BIO_CB_READ     0x02
# define BIO_CB_WRITE    0x03
# define BIO_CB_PUTS     0x04
# define BIO_CB_GETS     0x05
# define BIO_CB_CTRL     0x06

/*
 * The callback is called before and after the underling operation, The
 * BIO_CB_RETURN flag indicates if it is after the call
 */
# define BIO_CB_RETURN   0x80
# define BIO_CB_return(a) ((a)|BIO_CB_RETURN)
# define BIO_cb_pre(a)   (!((a)&BIO_CB_RETURN))
# define BIO_cb_post(a)  ((a)&BIO_CB_RETURN)

long (*BIO_get_callback(const BIO *b)) (struct bio_st *, int, const char *,
                                        int, long, long);
void BIO_set_callback(BIO *b,
                      long (*callback) (struct bio_st *, int, const char *,
                                        int, long, long));
char *BIO_get_callback_arg(const BIO *b);
void BIO_set_callback_arg(BIO *b, char *arg);

const char *BIO_method_name(const BIO *b);
int BIO_method_type(const BIO *b);

typedef void bio_info_cb (struct bio_st *, int, const char *, int, long,
                          long);

typedef struct bio_method_st {
    int type;
    const char *name;
    int (*bwrite) (BIO *, const char *, int);
    int (*bread) (BIO *, char *, int);
    int (*bputs) (BIO *, const char *);
    int (*bgets) (BIO *, char *, int);
    long (*ctrl) (BIO *, int, long, void *);
    int (*create) (BIO *);
    int (*destroy) (BIO *);
    long (*callback_ctrl) (BIO *, int, bio_info_cb *);
} BIO_METHOD;

struct bio_st {
    BIO_METHOD *method;
    /* bio, mode, argp, argi, argl, ret */
    long (*callback) (struct bio_st *, int, const char *, int, long, long);
    char *cb_arg;               /* first argument for the callback */
    int init;
    int shutdown;
    int flags;                  /* extra storage */
    int retry_reason;
    int num;
    void *ptr;
    struct bio_st *next_bio;    /* used by filter BIOs */
    struct bio_st *prev_bio;    /* used by filter BIOs */
    int references;
    unsigned long num_read;
    unsigned long num_write;
    CRYPTO_EX_DATA ex_data;
};

DECLARE_STACK_OF(BIO)

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
    /*- BIO *bio; *//*
     * this is now in the BIO struct
     */
    int ibuf_size;              /* how big is the input buffer */
    int obuf_size;              /* how big is the output buffer */
    char *ibuf;                 /* the char array */
    int ibuf_len;               /* how many bytes are in it */
    int ibuf_off;               /* write/read offset */
    char *obuf;                 /* the char array */
    int obuf_len;               /* how many bytes are in it */
    int obuf_off;               /* write/read offset */
} BIO_F_BUFFER_CTX;

/* Prefix and suffix callback in ASN1 BIO */
typedef int asn1_ps_func (BIO *b, unsigned char **pbuf, int *plen,
                          void *parg);

# ifndef OPENSSL_NO_SCTP
/* SCTP parameter structs */
struct bio_dgram_sctp_sndinfo {
    uint16_t snd_sid;
    uint16_t snd_flags;
    uint32_t snd_ppid;
    uint32_t snd_context;
};

struct bio_dgram_sctp_rcvinfo {
    uint16_t rcv_sid;
    uint16_t rcv_ssn;
    uint16_t rcv_flags;
    uint32_t rcv_ppid;
    uint32_t rcv_tsn;
    uint32_t rcv_cumtsn;
    uint32_t rcv_context;
};

struct bio_dgram_sctp_prinfo {
    uint16_t pr_policy;
    uint32_t pr_value;
};
# endif

/* connect BIO stuff */
# define BIO_CONN_S_BEFORE               1
# define BIO_CONN_S_GET_IP               2
# define BIO_CONN_S_GET_PORT             3
# define BIO_CONN_S_CREATE_SOCKET        4
# define BIO_CONN_S_CONNECT              5
# define BIO_CONN_S_OK                   6
# define BIO_CONN_S_BLOCKED_CONNECT      7
# define BIO_CONN_S_NBIO                 8
/*
 * #define BIO_CONN_get_param_hostname BIO_ctrl
 */

# define BIO_C_SET_CONNECT                       100
# define BIO_C_DO_STATE_MACHINE                  101
# define BIO_C_SET_NBIO                          102
# define BIO_C_SET_PROXY_PARAM                   103
# define BIO_C_SET_FD                            104
# define BIO_C_GET_FD                            105
# define BIO_C_SET_FILE_PTR                      106
# define BIO_C_GET_FILE_PTR                      107
# define BIO_C_SET_FILENAME                      108
# define BIO_C_SET_SSL                           109
# define BIO_C_GET_SSL                           110
# define BIO_C_SET_MD                            111
# define BIO_C_GET_MD                            112
# define BIO_C_GET_CIPHER_STATUS                 113
# define BIO_C_SET_BUF_MEM                       114
# define BIO_C_GET_BUF_MEM_PTR                   115
# define BIO_C_GET_BUFF_NUM_LINES                116
# define BIO_C_SET_BUFF_SIZE                     117
# define BIO_C_SET_ACCEPT                        118
# define BIO_C_SSL_MODE                          119
# define BIO_C_GET_MD_CTX                        120
# define BIO_C_GET_PROXY_PARAM                   121
# define BIO_C_SET_BUFF_READ_DATA                122/* data to read first */
# define BIO_C_GET_CONNECT                       123
# define BIO_C_GET_ACCEPT                        124
# define BIO_C_SET_SSL_RENEGOTIATE_BYTES         125
# define BIO_C_GET_SSL_NUM_RENEGOTIATES          126
# define BIO_C_SET_SSL_RENEGOTIATE_TIMEOUT       127
# define BIO_C_FILE_SEEK                         128
# define BIO_C_GET_CIPHER_CTX                    129
# define BIO_C_SET_BUF_MEM_EOF_RETURN            130/* return end of input
                                                     * value */
# define BIO_C_SET_BIND_MODE                     131
# define BIO_C_GET_BIND_MODE                     132
# define BIO_C_FILE_TELL                         133
# define BIO_C_GET_SOCKS                         134
# define BIO_C_SET_SOCKS                         135

# define BIO_C_SET_WRITE_BUF_SIZE                136/* for BIO_s_bio */
# define BIO_C_GET_WRITE_BUF_SIZE                137
# define BIO_C_MAKE_BIO_PAIR                     138
# define BIO_C_DESTROY_BIO_PAIR                  139
# define BIO_C_GET_WRITE_GUARANTEE               140
# define BIO_C_GET_READ_REQUEST                  141
# define BIO_C_SHUTDOWN_WR                       142
# define BIO_C_NREAD0                            143
# define BIO_C_NREAD                             144
# define BIO_C_NWRITE0                           145
# define BIO_C_NWRITE                            146
# define BIO_C_RESET_READ_REQUEST                147
# define BIO_C_SET_MD_CTX                        148

# define BIO_C_SET_PREFIX                        149
# define BIO_C_GET_PREFIX                        150
# define BIO_C_SET_SUFFIX                        151
# define BIO_C_GET_SUFFIX                        152

# define BIO_C_SET_EX_ARG                        153
# define BIO_C_GET_EX_ARG                        154

# define BIO_set_app_data(s,arg)         BIO_set_ex_data(s,0,arg)
# define BIO_get_app_data(s)             BIO_get_ex_data(s,0)

/* BIO_s_connect() and BIO_s_socks4a_connect() */
# define BIO_set_conn_hostname(b,name) BIO_ctrl(b,BIO_C_SET_CONNECT,0,(char *)name)
# define BIO_set_conn_port(b,port) BIO_ctrl(b,BIO_C_SET_CONNECT,1,(char *)port)
# define BIO_set_conn_ip(b,ip)     BIO_ctrl(b,BIO_C_SET_CONNECT,2,(char *)ip)
# define BIO_set_conn_int_port(b,port) BIO_ctrl(b,BIO_C_SET_CONNECT,3,(char *)port)
# define BIO_get_conn_hostname(b)  BIO_ptr_ctrl(b,BIO_C_GET_CONNECT,0)
# define BIO_get_conn_port(b)      BIO_ptr_ctrl(b,BIO_C_GET_CONNECT,1)
# define BIO_get_conn_ip(b)               BIO_ptr_ctrl(b,BIO_C_GET_CONNECT,2)
# define BIO_get_conn_int_port(b) BIO_ctrl(b,BIO_C_GET_CONNECT,3,NULL)

# define BIO_set_nbio(b,n)       BIO_ctrl(b,BIO_C_SET_NBIO,(n),NULL)

/* BIO_s_accept() */
# define BIO_set_accept_port(b,name) BIO_ctrl(b,BIO_C_SET_ACCEPT,0,(char *)name)
# define BIO_get_accept_port(b)  BIO_ptr_ctrl(b,BIO_C_GET_ACCEPT,0)
/* #define BIO_set_nbio(b,n)    BIO_ctrl(b,BIO_C_SET_NBIO,(n),NULL) */
# define BIO_set_nbio_accept(b,n) BIO_ctrl(b,BIO_C_SET_ACCEPT,1,(n)?(void *)"a":NULL)
# define BIO_set_accept_bios(b,bio) BIO_ctrl(b,BIO_C_SET_ACCEPT,2,(char *)bio)

# define BIO_BIND_NORMAL                 0
# define BIO_BIND_REUSEADDR_IF_UNUSED    1
# define BIO_BIND_REUSEADDR              2
# define BIO_set_bind_mode(b,mode) BIO_ctrl(b,BIO_C_SET_BIND_MODE,mode,NULL)
# define BIO_get_bind_mode(b,mode) BIO_ctrl(b,BIO_C_GET_BIND_MODE,0,NULL)

/* BIO_s_accept() and BIO_s_connect() */
# define BIO_do_connect(b)       BIO_do_handshake(b)
# define BIO_do_accept(b)        BIO_do_handshake(b)
# define BIO_do_handshake(b)     BIO_ctrl(b,BIO_C_DO_STATE_MACHINE,0,NULL)

/* BIO_s_proxy_client() */
# define BIO_set_url(b,url)      BIO_ctrl(b,BIO_C_SET_PROXY_PARAM,0,(char *)(url))
# define BIO_set_proxies(b,p)    BIO_ctrl(b,BIO_C_SET_PROXY_PARAM,1,(char *)(p))
/* BIO_set_nbio(b,n) */
# define BIO_set_filter_bio(b,s) BIO_ctrl(b,BIO_C_SET_PROXY_PARAM,2,(char *)(s))
/* BIO *BIO_get_filter_bio(BIO *bio); */
# define BIO_set_proxy_cb(b,cb) BIO_callback_ctrl(b,BIO_C_SET_PROXY_PARAM,3,(void *(*cb)()))
# define BIO_set_proxy_header(b,sk) BIO_ctrl(b,BIO_C_SET_PROXY_PARAM,4,(char *)sk)
# define BIO_set_no_connect_return(b,bool) BIO_int_ctrl(b,BIO_C_SET_PROXY_PARAM,5,bool)

# define BIO_get_proxy_header(b,skp) BIO_ctrl(b,BIO_C_GET_PROXY_PARAM,0,(char *)skp)
# define BIO_get_proxies(b,pxy_p) BIO_ctrl(b,BIO_C_GET_PROXY_PARAM,1,(char *)(pxy_p))
# define BIO_get_url(b,url)      BIO_ctrl(b,BIO_C_GET_PROXY_PARAM,2,(char *)(url))
# define BIO_get_no_connect_return(b)    BIO_ctrl(b,BIO_C_GET_PROXY_PARAM,5,NULL)

/* BIO_s_datagram(), BIO_s_fd(), BIO_s_socket(), BIO_s_accept() and BIO_s_connect() */
# define BIO_set_fd(b,fd,c)      BIO_int_ctrl(b,BIO_C_SET_FD,c,fd)
# define BIO_get_fd(b,c)         BIO_ctrl(b,BIO_C_GET_FD,0,(char *)c)

/* BIO_s_file() */
# define BIO_set_fp(b,fp,c)      BIO_ctrl(b,BIO_C_SET_FILE_PTR,c,(char *)fp)
# define BIO_get_fp(b,fpp)       BIO_ctrl(b,BIO_C_GET_FILE_PTR,0,(char *)fpp)

/* BIO_s_fd() and BIO_s_file() */
# define BIO_seek(b,ofs) (int)BIO_ctrl(b,BIO_C_FILE_SEEK,ofs,NULL)
# define BIO_tell(b)     (int)BIO_ctrl(b,BIO_C_FILE_TELL,0,NULL)

/*
 * name is cast to lose const, but might be better to route through a
 * function so we can do it safely
 */
# ifdef CONST_STRICT
/*
 * If you are wondering why this isn't defined, its because CONST_STRICT is
 * purely a compile-time kludge to allow const to be checked.
 */
int BIO_read_filename(BIO *b, const char *name);
# else
#  define BIO_read_filename(b,name) BIO_ctrl(b,BIO_C_SET_FILENAME, \
                BIO_CLOSE|BIO_FP_READ,(char *)name)
# endif
# define BIO_write_filename(b,name) BIO_ctrl(b,BIO_C_SET_FILENAME, \
                BIO_CLOSE|BIO_FP_WRITE,name)
# define BIO_append_filename(b,name) BIO_ctrl(b,BIO_C_SET_FILENAME, \
                BIO_CLOSE|BIO_FP_APPEND,name)
# define BIO_rw_filename(b,name) BIO_ctrl(b,BIO_C_SET_FILENAME, \
                BIO_CLOSE|BIO_FP_READ|BIO_FP_WRITE,name)

/*
 * WARNING WARNING, this ups the reference count on the read bio of the SSL
 * structure.  This is because the ssl read BIO is now pointed to by the
 * next_bio field in the bio.  So when you free the BIO, make sure you are
 * doing a BIO_free_all() to catch the underlying BIO.
 */
# define BIO_set_ssl(b,ssl,c)    BIO_ctrl(b,BIO_C_SET_SSL,c,(char *)ssl)
# define BIO_get_ssl(b,sslp)     BIO_ctrl(b,BIO_C_GET_SSL,0,(char *)sslp)
# define BIO_set_ssl_mode(b,client)      BIO_ctrl(b,BIO_C_SSL_MODE,client,NULL)
# define BIO_set_ssl_renegotiate_bytes(b,num) \
        BIO_ctrl(b,BIO_C_SET_SSL_RENEGOTIATE_BYTES,num,NULL);
# define BIO_get_num_renegotiates(b) \
        BIO_ctrl(b,BIO_C_GET_SSL_NUM_RENEGOTIATES,0,NULL);
# define BIO_set_ssl_renegotiate_timeout(b,seconds) \
        BIO_ctrl(b,BIO_C_SET_SSL_RENEGOTIATE_TIMEOUT,seconds,NULL);

/* defined in evp.h */
/* #define BIO_set_md(b,md)     BIO_ctrl(b,BIO_C_SET_MD,1,(char *)md) */

# define BIO_get_mem_data(b,pp)  BIO_ctrl(b,BIO_CTRL_INFO,0,(char *)pp)
# define BIO_set_mem_buf(b,bm,c) BIO_ctrl(b,BIO_C_SET_BUF_MEM,c,(char *)bm)
# define BIO_get_mem_ptr(b,pp)   BIO_ctrl(b,BIO_C_GET_BUF_MEM_PTR,0,(char *)pp)
# define BIO_set_mem_eof_return(b,v) \
                                BIO_ctrl(b,BIO_C_SET_BUF_MEM_EOF_RETURN,v,NULL)

/* For the BIO_f_buffer() type */
# define BIO_get_buffer_num_lines(b)     BIO_ctrl(b,BIO_C_GET_BUFF_NUM_LINES,0,NULL)
# define BIO_set_buffer_size(b,size)     BIO_ctrl(b,BIO_C_SET_BUFF_SIZE,size,NULL)
# define BIO_set_read_buffer_size(b,size) BIO_int_ctrl(b,BIO_C_SET_BUFF_SIZE,size,0)
# define BIO_set_write_buffer_size(b,size) BIO_int_ctrl(b,BIO_C_SET_BUFF_SIZE,size,1)
# define BIO_set_buffer_read_data(b,buf,num) BIO_ctrl(b,BIO_C_SET_BUFF_READ_DATA,num,buf)

/* Don't use the next one unless you know what you are doing :-) */
# define BIO_dup_state(b,ret)    BIO_ctrl(b,BIO_CTRL_DUP,0,(char *)(ret))

# define BIO_reset(b)            (int)BIO_ctrl(b,BIO_CTRL_RESET,0,NULL)
# define BIO_eof(b)              (int)BIO_ctrl(b,BIO_CTRL_EOF,0,NULL)
# define BIO_set_close(b,c)      (int)BIO_ctrl(b,BIO_CTRL_SET_CLOSE,(c),NULL)
# define BIO_get_close(b)        (int)BIO_ctrl(b,BIO_CTRL_GET_CLOSE,0,NULL)
# define BIO_pending(b)          (int)BIO_ctrl(b,BIO_CTRL_PENDING,0,NULL)
# define BIO_wpending(b)         (int)BIO_ctrl(b,BIO_CTRL_WPENDING,0,NULL)
/* ...pending macros have inappropriate return type */
size_t BIO_ctrl_pending(BIO *b);
size_t BIO_ctrl_wpending(BIO *b);
# define BIO_flush(b)            (int)BIO_ctrl(b,BIO_CTRL_FLUSH,0,NULL)
# define BIO_get_info_callback(b,cbp) (int)BIO_ctrl(b,BIO_CTRL_GET_CALLBACK,0, \
                                                   cbp)
# define BIO_set_info_callback(b,cb) (int)BIO_callback_ctrl(b,BIO_CTRL_SET_CALLBACK,cb)

/* For the BIO_f_buffer() type */
# define BIO_buffer_get_num_lines(b) BIO_ctrl(b,BIO_CTRL_GET,0,NULL)

/* For BIO_s_bio() */
# define BIO_set_write_buf_size(b,size) (int)BIO_ctrl(b,BIO_C_SET_WRITE_BUF_SIZE,size,NULL)
# define BIO_get_write_buf_size(b,size) (size_t)BIO_ctrl(b,BIO_C_GET_WRITE_BUF_SIZE,size,NULL)
# define BIO_make_bio_pair(b1,b2)   (int)BIO_ctrl(b1,BIO_C_MAKE_BIO_PAIR,0,b2)
# define BIO_destroy_bio_pair(b)    (int)BIO_ctrl(b,BIO_C_DESTROY_BIO_PAIR,0,NULL)
# define BIO_shutdown_wr(b) (int)BIO_ctrl(b, BIO_C_SHUTDOWN_WR, 0, NULL)
/* macros with inappropriate type -- but ...pending macros use int too: */
# define BIO_get_write_guarantee(b) (int)BIO_ctrl(b,BIO_C_GET_WRITE_GUARANTEE,0,NULL)
# define BIO_get_read_request(b)    (int)BIO_ctrl(b,BIO_C_GET_READ_REQUEST,0,NULL)
size_t BIO_ctrl_get_write_guarantee(BIO *b);
size_t BIO_ctrl_get_read_request(BIO *b);
int BIO_ctrl_reset_read_request(BIO *b);

/* ctrl macros for dgram */
# define BIO_ctrl_dgram_connect(b,peer)  \
                     (int)BIO_ctrl(b,BIO_CTRL_DGRAM_CONNECT,0, (char *)peer)
# define BIO_ctrl_set_connected(b, state, peer) \
         (int)BIO_ctrl(b, BIO_CTRL_DGRAM_SET_CONNECTED, state, (char *)peer)
# define BIO_dgram_recv_timedout(b) \
         (int)BIO_ctrl(b, BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP, 0, NULL)
# define BIO_dgram_send_timedout(b) \
         (int)BIO_ctrl(b, BIO_CTRL_DGRAM_GET_SEND_TIMER_EXP, 0, NULL)
# define BIO_dgram_get_peer(b,peer) \
         (int)BIO_ctrl(b, BIO_CTRL_DGRAM_GET_PEER, 0, (char *)peer)
# define BIO_dgram_set_peer(b,peer) \
         (int)BIO_ctrl(b, BIO_CTRL_DGRAM_SET_PEER, 0, (char *)peer)
# define BIO_dgram_get_mtu_overhead(b) \
         (unsigned int)BIO_ctrl((b), BIO_CTRL_DGRAM_GET_MTU_OVERHEAD, 0, NULL)

/* These two aren't currently implemented */
/* int BIO_get_ex_num(BIO *bio); */
/* void BIO_set_ex_free_func(BIO *bio,int idx,void (*cb)()); */
int BIO_set_ex_data(BIO *bio, int idx, void *data);
void *BIO_get_ex_data(BIO *bio, int idx);
int BIO_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
                         CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func);
unsigned long BIO_number_read(BIO *bio);
unsigned long BIO_number_written(BIO *bio);

/* For BIO_f_asn1() */
int BIO_asn1_set_prefix(BIO *b, asn1_ps_func *prefix,
                        asn1_ps_func *prefix_free);
int BIO_asn1_get_prefix(BIO *b, asn1_ps_func **pprefix,
                        asn1_ps_func **pprefix_free);
int BIO_asn1_set_suffix(BIO *b, asn1_ps_func *suffix,
                        asn1_ps_func *suffix_free);
int BIO_asn1_get_suffix(BIO *b, asn1_ps_func **psuffix,
                        asn1_ps_func **psuffix_free);

# ifndef OPENSSL_NO_FP_API
BIO_METHOD *BIO_s_file(void);
BIO *BIO_new_file(const char *filename, const char *mode);
BIO *BIO_new_fp(FILE *stream, int close_flag);
#  define BIO_s_file_internal    BIO_s_file
# endif
BIO *BIO_new(BIO_METHOD *type);
int BIO_set(BIO *a, BIO_METHOD *type);
int BIO_free(BIO *a);
void BIO_vfree(BIO *a);
int BIO_read(BIO *b, void *data, int len);
int BIO_gets(BIO *bp, char *buf, int size);
int BIO_write(BIO *b, const void *data, int len);
int BIO_puts(BIO *bp, const char *buf);
int BIO_indent(BIO *b, int indent, int max);
long BIO_ctrl(BIO *bp, int cmd, long larg, void *parg);
long BIO_callback_ctrl(BIO *b, int cmd,
                       void (*fp) (struct bio_st *, int, const char *, int,
                                   long, long));
char *BIO_ptr_ctrl(BIO *bp, int cmd, long larg);
long BIO_int_ctrl(BIO *bp, int cmd, long larg, int iarg);
BIO *BIO_push(BIO *b, BIO *append);
BIO *BIO_pop(BIO *b);
void BIO_free_all(BIO *a);
BIO *BIO_find_type(BIO *b, int bio_type);
BIO *BIO_next(BIO *b);
BIO *BIO_get_retry_BIO(BIO *bio, int *reason);
int BIO_get_retry_reason(BIO *bio);
BIO *BIO_dup_chain(BIO *in);

int BIO_nread0(BIO *bio, char **buf);
int BIO_nread(BIO *bio, char **buf, int num);
int BIO_nwrite0(BIO *bio, char **buf);
int BIO_nwrite(BIO *bio, char **buf, int num);

long BIO_debug_callback(BIO *bio, int cmd, const char *argp, int argi,
                        long argl, long ret);

BIO_METHOD *BIO_s_mem(void);
BIO *BIO_new_mem_buf(void *buf, int len);
BIO_METHOD *BIO_s_socket(void);
BIO_METHOD *BIO_s_connect(void);
BIO_METHOD *BIO_s_accept(void);
BIO_METHOD *BIO_s_fd(void);
# ifndef OPENSSL_SYS_OS2
BIO_METHOD *BIO_s_log(void);
# endif
BIO_METHOD *BIO_s_bio(void);
BIO_METHOD *BIO_s_null(void);
BIO_METHOD *BIO_f_null(void);
BIO_METHOD *BIO_f_buffer(void);
# ifdef OPENSSL_SYS_VMS
BIO_METHOD *BIO_f_linebuffer(void);
# endif
BIO_METHOD *BIO_f_nbio_test(void);
# ifndef OPENSSL_NO_DGRAM
BIO_METHOD *BIO_s_datagram(void);
#  ifndef OPENSSL_NO_SCTP
BIO_METHOD *BIO_s_datagram_sctp(void);
#  endif
# endif

/* BIO_METHOD *BIO_f_ber(void); */

int BIO_sock_should_retry(int i);
int BIO_sock_non_fatal_error(int error);
int BIO_dgram_non_fatal_error(int error);

int BIO_fd_should_retry(int i);
int BIO_fd_non_fatal_error(int error);
int BIO_dump_cb(int (*cb) (const void *data, size_t len, void *u),
                void *u, const char *s, int len);
int BIO_dump_indent_cb(int (*cb) (const void *data, size_t len, void *u),
                       void *u, const char *s, int len, int indent);
int BIO_dump(BIO *b, const char *bytes, int len);
int BIO_dump_indent(BIO *b, const char *bytes, int len, int indent);
# ifndef OPENSSL_NO_FP_API
int BIO_dump_fp(FILE *fp, const char *s, int len);
int BIO_dump_indent_fp(FILE *fp, const char *s, int len, int indent);
# endif
struct hostent *BIO_gethostbyname(const char *name);
/*-
 * We might want a thread-safe interface too:
 * struct hostent *BIO_gethostbyname_r(const char *name,
 *     struct hostent *result, void *buffer, size_t buflen);
 * or something similar (caller allocates a struct hostent,
 * pointed to by "result", and additional buffer space for the various
 * substructures; if the buffer does not suffice, NULL is returned
 * and an appropriate error code is set).
 */
int BIO_sock_error(int sock);
int BIO_socket_ioctl(int fd, long type, void *arg);
int BIO_socket_nbio(int fd, int mode);
int BIO_get_port(const char *str, unsigned short *port_ptr);
int BIO_get_host_ip(const char *str, unsigned char *ip);
int BIO_get_accept_socket(char *host_port, int mode);
int BIO_accept(int sock, char **ip_port);
int BIO_sock_init(void);
void BIO_sock_cleanup(void);
int BIO_set_tcp_ndelay(int sock, int turn_on);

BIO *BIO_new_socket(int sock, int close_flag);
BIO *BIO_new_dgram(int fd, int close_flag);
# ifndef OPENSSL_NO_SCTP
BIO *BIO_new_dgram_sctp(int fd, int close_flag);
int BIO_dgram_is_sctp(BIO *bio);
int BIO_dgram_sctp_notification_cb(BIO *b,
                                   void (*handle_notifications) (BIO *bio,
                                                                 void
                                                                 *context,
                                                                 void *buf),
                                   void *context);
int BIO_dgram_sctp_wait_for_dry(BIO *b);
int BIO_dgram_sctp_msg_waiting(BIO *b);
# endif
BIO *BIO_new_fd(int fd, int close_flag);
BIO *BIO_new_connect(char *host_port);
BIO *BIO_new_accept(char *host_port);

int BIO_new_bio_pair(BIO **bio1, size_t writebuf1,
                     BIO **bio2, size_t writebuf2);
/*
 * If successful, returns 1 and in *bio1, *bio2 two BIO pair endpoints.
 * Otherwise returns 0 and sets *bio1 and *bio2 to NULL. Size 0 uses default
 * value.
 */

void BIO_copy_next_retry(BIO *b);

/*
 * long BIO_ghbn_ctrl(int cmd,int iarg,char *parg);
 */

# ifdef __GNUC__
#  define __bio_h__attr__ __attribute__
# else
#  define __bio_h__attr__(x)
# endif
int BIO_printf(BIO *bio, const char *format, ...)
__bio_h__attr__((__format__(__printf__, 2, 3)));
int BIO_vprintf(BIO *bio, const char *format, va_list args)
__bio_h__attr__((__format__(__printf__, 2, 0)));
int BIO_snprintf(char *buf, size_t n, const char *format, ...)
__bio_h__attr__((__format__(__printf__, 3, 4)));
int BIO_vsnprintf(char *buf, size_t n, const char *format, va_list args)
__bio_h__attr__((__format__(__printf__, 3, 0)));
# undef __bio_h__attr__

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_BIO_strings(void);

/* Error codes for the BIO functions. */

/* Function codes. */
# define BIO_F_ACPT_STATE                                 100
# define BIO_F_BIO_ACCEPT                                 101
# define BIO_F_BIO_BER_GET_HEADER                         102
# define BIO_F_BIO_CALLBACK_CTRL                          131
# define BIO_F_BIO_CTRL                                   103
# define BIO_F_BIO_GETHOSTBYNAME                          120
# define BIO_F_BIO_GETS                                   104
# define BIO_F_BIO_GET_ACCEPT_SOCKET                      105
# define BIO_F_BIO_GET_HOST_IP                            106
# define BIO_F_BIO_GET_PORT                               107
# define BIO_F_BIO_MAKE_PAIR                              121
# define BIO_F_BIO_NEW                                    108
# define BIO_F_BIO_NEW_FILE                               109
# define BIO_F_BIO_NEW_MEM_BUF                            126
# define BIO_F_BIO_NREAD                                  123
# define BIO_F_BIO_NREAD0                                 124
# define BIO_F_BIO_NWRITE                                 125
# define BIO_F_BIO_NWRITE0                                122
# define BIO_F_BIO_PUTS                                   110
# define BIO_F_BIO_READ                                   111
# define BIO_F_BIO_SOCK_INIT                              112
# define BIO_F_BIO_WRITE                                  113
# define BIO_F_BUFFER_CTRL                                114
# define BIO_F_CONN_CTRL                                  127
# define BIO_F_CONN_STATE                                 115
# define BIO_F_DGRAM_SCTP_READ                            132
# define BIO_F_DGRAM_SCTP_WRITE                           133
# define BIO_F_FILE_CTRL                                  116
# define BIO_F_FILE_READ                                  130
# define BIO_F_LINEBUFFER_CTRL                            129
# define BIO_F_MEM_READ                                   128
# define BIO_F_MEM_WRITE                                  117
# define BIO_F_SSL_NEW                                    118
# define BIO_F_WSASTARTUP                                 119

/* Reason codes. */
# define BIO_R_ACCEPT_ERROR                               100
# define BIO_R_BAD_FOPEN_MODE                             101
# define BIO_R_BAD_HOSTNAME_LOOKUP                        102
# define BIO_R_BROKEN_PIPE                                124
# define BIO_R_CONNECT_ERROR                              103
# define BIO_R_EOF_ON_MEMORY_BIO                          127
# define BIO_R_ERROR_SETTING_NBIO                         104
# define BIO_R_ERROR_SETTING_NBIO_ON_ACCEPTED_SOCKET      105
# define BIO_R_ERROR_SETTING_NBIO_ON_ACCEPT_SOCKET        106
# define BIO_R_GETHOSTBYNAME_ADDR_IS_NOT_AF_INET          107
# define BIO_R_INVALID_ARGUMENT                           125
# define BIO_R_INVALID_IP_ADDRESS                         108
# define BIO_R_IN_USE                                     123
# define BIO_R_KEEPALIVE                                  109
# define BIO_R_NBIO_CONNECT_ERROR                         110
# define BIO_R_NO_ACCEPT_PORT_SPECIFIED                   111
# define BIO_R_NO_HOSTNAME_SPECIFIED                      112
# define BIO_R_NO_PORT_DEFINED                            113
# define BIO_R_NO_PORT_SPECIFIED                          114
# define BIO_R_NO_SUCH_FILE                               128
# define BIO_R_NULL_PARAMETER                             115
# define BIO_R_TAG_MISMATCH                               116
# define BIO_R_UNABLE_TO_BIND_SOCKET                      117
# define BIO_R_UNABLE_TO_CREATE_SOCKET                    118
# define BIO_R_UNABLE_TO_LISTEN_SOCKET                    119
# define BIO_R_UNINITIALIZED                              120
# define BIO_R_UNSUPPORTED_METHOD                         121
# define BIO_R_WRITE_TO_READ_ONLY_BIO                     126
# define BIO_R_WSASTARTUP                                 122

#ifdef  __cplusplus
}
#endif
#endif
