/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <openssl/safestack.h>

#include "ssltestlib.h"

#define SSL_IS_DTLS(s)  (s->method->version == DTLS_ANY_VERSION \
    || s->method->version == DTLS1_2_VERSION \
    || s->method->version == DTLS1_VERSION)

static int tls_dump_new(BIO *bi);
static int tls_dump_free(BIO *a);
static int tls_dump_read(BIO *b, char *out, int outl);
static int tls_dump_write(BIO *b, const char *in, int inl);
static long tls_dump_ctrl(BIO *b, int cmd, long num, void *ptr);
static int tls_dump_gets(BIO *bp, char *buf, int size);
static int tls_dump_puts(BIO *bp, const char *str);

/* Choose a sufficiently large type likely to be unused for this custom BIO */
# define BIO_TYPE_TLS_DUMP_FILTER  (0x80 | BIO_TYPE_FILTER)

# define BIO_TYPE_MEMPACKET_TEST      0x81

static BIO_METHOD method_tls_dump = {
    BIO_TYPE_TLS_DUMP_FILTER,
    "TLS dump filter",
    tls_dump_write,
    tls_dump_read,
    tls_dump_puts,
    tls_dump_gets,
    tls_dump_ctrl,
    tls_dump_new,
    tls_dump_free
};

BIO_METHOD *bio_f_tls_dump_filter(void)
{
    return &method_tls_dump;
}

static int tls_dump_new(BIO *bio)
{
    bio->init = 1;
    return 1;
}

static int tls_dump_free(BIO *bio)
{
    bio->init = 0;

    return 1;
}

static void copy_flags(BIO *bio)
{
    int flags;
    BIO *next = BIO_next(bio);

    flags = BIO_test_flags(next, BIO_FLAGS_SHOULD_RETRY | BIO_FLAGS_RWS);
    BIO_clear_flags(bio, BIO_FLAGS_SHOULD_RETRY | BIO_FLAGS_RWS);
    BIO_set_flags(bio, flags);
}

#define RECORD_CONTENT_TYPE     0
#define RECORD_VERSION_HI       1
#define RECORD_VERSION_LO       2
#define RECORD_EPOCH_HI         3
#define RECORD_EPOCH_LO         4
#define RECORD_SEQUENCE_START   5
#define RECORD_SEQUENCE_END     10
#define RECORD_LEN_HI           11
#define RECORD_LEN_LO           12

#define MSG_TYPE                0
#define MSG_LEN_HI              1
#define MSG_LEN_MID             2
#define MSG_LEN_LO              3
#define MSG_SEQ_HI              4
#define MSG_SEQ_LO              5
#define MSG_FRAG_OFF_HI         6
#define MSG_FRAG_OFF_MID        7
#define MSG_FRAG_OFF_LO         8
#define MSG_FRAG_LEN_HI         9
#define MSG_FRAG_LEN_MID        10
#define MSG_FRAG_LEN_LO         11


static void dump_data(const char *data, int len)
{
    int rem, i, content, reclen, msglen, fragoff, fraglen, epoch;
    unsigned char *rec;

    printf("---- START OF PACKET ----\n");

    rem = len;
    rec = (unsigned char *)data;

    while (rem > 0) {
        if (rem != len)
            printf("*\n");
        printf("*---- START OF RECORD ----\n");
        if (rem < DTLS1_RT_HEADER_LENGTH) {
            printf("*---- RECORD TRUNCATED ----\n");
            break;
        }
        content = rec[RECORD_CONTENT_TYPE];
        printf("** Record Content-type: %d\n", content);
        printf("** Record Version: %02x%02x\n",
               rec[RECORD_VERSION_HI], rec[RECORD_VERSION_LO]);
        epoch = (rec[RECORD_EPOCH_HI] << 8) | rec[RECORD_EPOCH_LO];
        printf("** Record Epoch: %d\n", epoch);
        printf("** Record Sequence: ");
        for (i = RECORD_SEQUENCE_START; i <= RECORD_SEQUENCE_END; i++)
            printf("%02x", rec[i]);
        reclen = (rec[RECORD_LEN_HI] << 8) | rec[RECORD_LEN_LO];
        printf("\n** Record Length: %d\n", reclen);

        /* Now look at message */
        rec += DTLS1_RT_HEADER_LENGTH;
        rem -= DTLS1_RT_HEADER_LENGTH;
        if (content == SSL3_RT_HANDSHAKE) {
            printf("**---- START OF HANDSHAKE MESSAGE FRAGMENT ----\n");
            if (epoch > 0) {
                printf("**---- HANDSHAKE MESSAGE FRAGMENT ENCRYPTED ----\n");
            } else if (rem < DTLS1_HM_HEADER_LENGTH
                    || reclen < DTLS1_HM_HEADER_LENGTH) {
                printf("**---- HANDSHAKE MESSAGE FRAGMENT TRUNCATED ----\n");
            } else {
                printf("*** Message Type: %d\n", rec[MSG_TYPE]);
                msglen = (rec[MSG_LEN_HI] << 16) | (rec[MSG_LEN_MID] << 8)
                         | rec[MSG_LEN_LO];
                printf("*** Message Length: %d\n", msglen);
                printf("*** Message sequence: %d\n",
                       (rec[MSG_SEQ_HI] << 8) | rec[MSG_SEQ_LO]);
                fragoff = (rec[MSG_FRAG_OFF_HI] << 16)
                          | (rec[MSG_FRAG_OFF_MID] << 8)
                          | rec[MSG_FRAG_OFF_LO];
                printf("*** Message Fragment offset: %d\n", fragoff);
                fraglen = (rec[MSG_FRAG_LEN_HI] << 16)
                          | (rec[MSG_FRAG_LEN_MID] << 8)
                          | rec[MSG_FRAG_LEN_LO];
                printf("*** Message Fragment len: %d\n", fraglen);
                if (fragoff + fraglen > msglen)
                    printf("***---- HANDSHAKE MESSAGE FRAGMENT INVALID ----\n");
                else if(reclen < fraglen)
                    printf("**---- HANDSHAKE MESSAGE FRAGMENT TRUNCATED ----\n");
                else
                    printf("**---- END OF HANDSHAKE MESSAGE FRAGMENT ----\n");
            }
        }
        if (rem < reclen) {
            printf("*---- RECORD TRUNCATED ----\n");
            rem = 0;
        } else {
            rec += reclen;
            rem -= reclen;
            printf("*---- END OF RECORD ----\n");
        }
    }
    printf("---- END OF PACKET ----\n\n");
    fflush(stdout);
}

static int tls_dump_read(BIO *bio, char *out, int outl)
{
    int ret;
    BIO *next = BIO_next(bio);

    ret = BIO_read(next, out, outl);
    copy_flags(bio);

    if (ret > 0) {
        dump_data(out, ret);
    }

    return ret;
}

static int tls_dump_write(BIO *bio, const char *in, int inl)
{
    int ret;
    BIO *next = BIO_next(bio);

    ret = BIO_write(next, in, inl);
    copy_flags(bio);

    return ret;
}

static long tls_dump_ctrl(BIO *bio, int cmd, long num, void *ptr)
{
    long ret;
    BIO *next = BIO_next(bio);

    if (next == NULL)
        return 0;

    switch (cmd) {
    case BIO_CTRL_DUP:
        ret = 0L;
        break;
    default:
        ret = BIO_ctrl(next, cmd, num, ptr);
        break;
    }
    return ret;
}

static int tls_dump_gets(BIO *bio, char *buf, int size)
{
    /* We don't support this - not needed anyway */
    return -1;
}

static int tls_dump_puts(BIO *bio, const char *str)
{
    return tls_dump_write(bio, str, strlen(str));
}


typedef struct mempacket_st {
    unsigned char *data;
    int len;
    unsigned int num;
    unsigned int type;
} MEMPACKET;

/*
 * These defines would normally be auto-generated and in safestack.h...but this
 * is just for tests so its probably not an appropriate place
 */
# define sk_MEMPACKET_new(cmp) SKM_sk_new(MEMPACKET, (cmp))
# define sk_MEMPACKET_new_null() SKM_sk_new_null(MEMPACKET)
# define sk_MEMPACKET_free(st) SKM_sk_free(MEMPACKET, (st))
# define sk_MEMPACKET_num(st) SKM_sk_num(MEMPACKET, (st))
# define sk_MEMPACKET_value(st, i) SKM_sk_value(MEMPACKET, (st), (i))
# define sk_MEMPACKET_set(st, i, val) SKM_sk_set(MEMPACKET, (st), (i), (val))
# define sk_MEMPACKET_zero(st) SKM_sk_zero(MEMPACKET, (st))
# define sk_MEMPACKET_push(st, val) SKM_sk_push(MEMPACKET, (st), (val))
# define sk_MEMPACKET_unshift(st, val) SKM_sk_unshift(MEMPACKET, (st), (val))
# define sk_MEMPACKET_find(st, val) SKM_sk_find(MEMPACKET, (st), (val))
# define sk_MEMPACKET_find_ex(st, val) SKM_sk_find_ex(MEMPACKET, (st), (val))
# define sk_MEMPACKET_delete(st, i) SKM_sk_delete(MEMPACKET, (st), (i))
# define sk_MEMPACKET_delete_ptr(st, ptr) SKM_sk_delete_ptr(MEMPACKET, (st), (ptr))
# define sk_MEMPACKET_insert(st, val, i) SKM_sk_insert(MEMPACKET, (st), (val), (i))
# define sk_MEMPACKET_set_cmp_func(st, cmp) SKM_sk_set_cmp_func(MEMPACKET, (st), (cmp))
# define sk_MEMPACKET_dup(st) SKM_sk_dup(MEMPACKET, st)
# define sk_MEMPACKET_pop_free(st, free_func) SKM_sk_pop_free(MEMPACKET, (st), (free_func))
# define sk_MEMPACKET_deep_copy(st, copy_func, free_func) SKM_sk_deep_copy(MEMPACKET, (st), (copy_func), (free_func))
# define sk_MEMPACKET_shift(st) SKM_sk_shift(MEMPACKET, (st))
# define sk_MEMPACKET_pop(st) SKM_sk_pop(MEMPACKET, (st))
# define sk_MEMPACKET_sort(st) SKM_sk_sort(MEMPACKET, (st))
# define sk_MEMPACKET_is_sorted(st) SKM_sk_is_sorted(MEMPACKET, (st))

static void mempacket_free(MEMPACKET *pkt)
{
    if (pkt->data != NULL)
        OPENSSL_free(pkt->data);
    OPENSSL_free(pkt);
}

typedef struct mempacket_test_ctx_st {
    STACK_OF(MEMPACKET) *pkts;
    unsigned int epoch;
    unsigned int currrec;
    unsigned int currpkt;
    unsigned int lastpkt;
    unsigned int noinject;
} MEMPACKET_TEST_CTX;

static int mempacket_test_new(BIO *bi);
static int mempacket_test_free(BIO *a);
static int mempacket_test_read(BIO *b, char *out, int outl);
static int mempacket_test_write(BIO *b, const char *in, int inl);
static long mempacket_test_ctrl(BIO *b, int cmd, long num, void *ptr);
static int mempacket_test_gets(BIO *bp, char *buf, int size);
static int mempacket_test_puts(BIO *bp, const char *str);

static BIO_METHOD method_mempacket_test = {
    BIO_TYPE_MEMPACKET_TEST,
    "Mem Packet Test",
    mempacket_test_write,
    mempacket_test_read,
    mempacket_test_puts,
    mempacket_test_gets,
    mempacket_test_ctrl,
    mempacket_test_new,
    mempacket_test_free
};

BIO_METHOD *bio_s_mempacket_test(void)
{
    return &method_mempacket_test;
}

static int mempacket_test_new(BIO *bio)
{
    MEMPACKET_TEST_CTX *ctx = OPENSSL_malloc(sizeof(*ctx));
    if (ctx == NULL)
        return 0;
    memset(ctx, 0, sizeof(*ctx));

    ctx->pkts = sk_MEMPACKET_new_null();
    if (ctx->pkts == NULL) {
        OPENSSL_free(ctx);
        return 0;
    }
    bio->init = 1;
    bio->ptr = ctx;
    return 1;
}

static int mempacket_test_free(BIO *bio)
{
    MEMPACKET_TEST_CTX *ctx = bio->ptr;

    sk_MEMPACKET_pop_free(ctx->pkts, mempacket_free);
    OPENSSL_free(ctx);
    bio->ptr = NULL;
    bio->init = 0;

    return 1;
}

/* Record Header values */
#define EPOCH_HI        4
#define EPOCH_LO        5
#define RECORD_SEQUENCE 10
#define RECORD_LEN_HI   11
#define RECORD_LEN_LO   12

#define STANDARD_PACKET                 0

static int mempacket_test_read(BIO *bio, char *out, int outl)
{
    MEMPACKET_TEST_CTX *ctx = bio->ptr;
    MEMPACKET *thispkt;
    unsigned char *rec;
    int rem;
    unsigned int seq, offset, len, epoch;

    BIO_clear_retry_flags(bio);

    thispkt = sk_MEMPACKET_value(ctx->pkts, 0);
    if (thispkt == NULL || thispkt->num != ctx->currpkt) {
        /* Probably run out of data */
        BIO_set_retry_read(bio);
        return -1;
    }
    (void)sk_MEMPACKET_shift(ctx->pkts);
    ctx->currpkt++;

    if (outl > thispkt->len)
        outl = thispkt->len;

    if (thispkt->type != INJECT_PACKET_IGNORE_REC_SEQ) {
        /*
         * Overwrite the record sequence number. We strictly number them in
         * the order received. Since we are actually a reliable transport
         * we know that there won't be any re-ordering. We overwrite to deal
         * with any packets that have been injected
         */
        rem = thispkt->len;
        rec = thispkt->data;
        while (rem > 0) {
            if (rem < DTLS1_RT_HEADER_LENGTH) {
                return -1;
            }
            epoch = (rec[EPOCH_HI] << 8) | rec[EPOCH_LO];
            if (epoch != ctx->epoch) {
                ctx->epoch = epoch;
                ctx->currrec = 0;
            }
            seq = ctx->currrec;
            offset = 0;
            do {
                rec[RECORD_SEQUENCE - offset] = seq & 0xFF;
                seq >>= 8;
                offset++;
            } while (seq > 0);
            ctx->currrec++;

            len = ((rec[RECORD_LEN_HI] << 8) | rec[RECORD_LEN_LO])
                  + DTLS1_RT_HEADER_LENGTH;

            rec += len;
            rem -= len;
        }
    }

    memcpy(out, thispkt->data, outl);

    mempacket_free(thispkt);

    return outl;
}

int mempacket_test_inject(BIO *bio, const char *in, int inl, int pktnum,
                          int type)
{
    MEMPACKET_TEST_CTX *ctx = bio->ptr;
    MEMPACKET *thispkt, *looppkt, *nextpkt;
    int i;

    if (ctx == NULL)
        return -1;

    /* We only allow injection before we've started writing any data */
    if (pktnum >= 0) {
        if (ctx->noinject)
            return -1;
    } else {
        ctx->noinject = 1;
    }

    thispkt = OPENSSL_malloc(sizeof(MEMPACKET));
    if (thispkt == NULL)
        return -1;

    thispkt->data = OPENSSL_malloc(inl);
    if (thispkt->data == NULL) {
        mempacket_free(thispkt);
        return -1;
    }

    memcpy(thispkt->data, in, inl);
    thispkt->len = inl;
    thispkt->num = (pktnum >= 0) ? (unsigned int)pktnum : ctx->lastpkt;
    thispkt->type = type;

    for(i = 0; (looppkt = sk_MEMPACKET_value(ctx->pkts, i)) != NULL; i++) {
        /* Check if we found the right place to insert this packet */
        if (looppkt->num > thispkt->num) {
            if (sk_MEMPACKET_insert(ctx->pkts, thispkt, i) == 0) {
                mempacket_free(thispkt);
                return -1;
            }
            /* If we're doing up front injection then we're done */
            if (pktnum >= 0)
                return inl;
            /*
             * We need to do some accounting on lastpkt. We increment it first,
             * but it might now equal the value of injected packets, so we need
             * to skip over those
             */
            ctx->lastpkt++;
            do {
                i++;
                nextpkt = sk_MEMPACKET_value(ctx->pkts, i);
                if (nextpkt != NULL && nextpkt->num == ctx->lastpkt)
                    ctx->lastpkt++;
                else
                    return inl;
            } while(1);
        } else if(looppkt->num == thispkt->num) {
            if (!ctx->noinject) {
                /* We injected two packets with the same packet number! */
                return -1;
            }
            ctx->lastpkt++;
            thispkt->num++;
        }
    }
    /*
     * We didn't find any packets with a packet number equal to or greater than
     * this one, so we just add it onto the end
     */
    if (!sk_MEMPACKET_push(ctx->pkts, thispkt)) {
        mempacket_free(thispkt);
        return -1;
    }

    if (pktnum < 0)
        ctx->lastpkt++;

    return inl;
}

static int mempacket_test_write(BIO *bio, const char *in, int inl)
{
    return mempacket_test_inject(bio, in, inl, -1, STANDARD_PACKET);
}

static long mempacket_test_ctrl(BIO *bio, int cmd, long num, void *ptr)
{
    long ret = 1;
    MEMPACKET_TEST_CTX *ctx = bio->ptr;
    MEMPACKET *thispkt;

    switch (cmd) {
    case BIO_CTRL_EOF:
        ret = (long)(sk_MEMPACKET_num(ctx->pkts) == 0);
        break;
    case BIO_CTRL_GET_CLOSE:
        ret = bio->shutdown;
        break;
    case BIO_CTRL_SET_CLOSE:
        bio->shutdown = (int)num;
        break;
    case BIO_CTRL_WPENDING:
        ret = 0L;
        break;
    case BIO_CTRL_PENDING:
        thispkt = sk_MEMPACKET_value(ctx->pkts, 0);
        if (thispkt == NULL)
            ret = 0;
        else
            ret = thispkt->len;
        break;
    case BIO_CTRL_FLUSH:
        ret = 1;
        break;
    case BIO_CTRL_RESET:
    case BIO_CTRL_DUP:
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
    default:
        ret = 0;
        break;
    }
    return ret;
}

static int mempacket_test_gets(BIO *bio, char *buf, int size)
{
    /* We don't support this - not needed anyway */
    return -1;
}

static int mempacket_test_puts(BIO *bio, const char *str)
{
    return mempacket_test_write(bio, str, strlen(str));
}

int create_ssl_ctx_pair(const SSL_METHOD *sm, const SSL_METHOD *cm,
                        SSL_CTX **sctx, SSL_CTX **cctx, char *certfile,
                        char *privkeyfile)
{
    SSL_CTX *serverctx = NULL;
    SSL_CTX *clientctx = NULL;

    serverctx = SSL_CTX_new(sm);
    clientctx = SSL_CTX_new(cm);
    if (serverctx == NULL || clientctx == NULL) {
        printf("Failed to create SSL_CTX\n");
        goto err;
    }

    if (SSL_CTX_use_certificate_file(serverctx, certfile,
                                     SSL_FILETYPE_PEM) <= 0) {
        printf("Failed to load server certificate\n");
        goto err;
    }
    if (SSL_CTX_use_PrivateKey_file(serverctx, privkeyfile,
                                    SSL_FILETYPE_PEM) <= 0) {
        printf("Failed to load server private key\n");
    }
    if (SSL_CTX_check_private_key(serverctx) <= 0) {
        printf("Failed to check private key\n");
        goto err;
    }

    *sctx = serverctx;
    *cctx = clientctx;

    return 1;
 err:
    SSL_CTX_free(serverctx);
    SSL_CTX_free(clientctx);
    return 0;
}

#define MAXLOOPS    100000

/*
 * NOTE: Transfers control of the BIOs - this function will free them on error
 */
int create_ssl_objects(SSL_CTX *serverctx, SSL_CTX *clientctx, SSL **sssl,
                          SSL **cssl, BIO *s_to_c_fbio, BIO *c_to_s_fbio)
{
    SSL *serverssl, *clientssl;
    BIO *s_to_c_bio = NULL, *c_to_s_bio = NULL;

    serverssl = SSL_new(serverctx);
    clientssl = SSL_new(clientctx);

    if (serverssl == NULL || clientssl == NULL) {
        printf("Failed to create SSL object\n");
        goto error;
    }

    if (SSL_IS_DTLS(clientssl)) {
        s_to_c_bio = BIO_new(bio_s_mempacket_test());
        c_to_s_bio = BIO_new(bio_s_mempacket_test());;
    } else {
        s_to_c_bio = BIO_new(BIO_s_mem());
        c_to_s_bio = BIO_new(BIO_s_mem());
    }
    if (s_to_c_bio == NULL || c_to_s_bio == NULL) {
        printf("Failed to create mem BIOs\n");
        goto error;
    }

    if (s_to_c_fbio != NULL)
        s_to_c_bio = BIO_push(s_to_c_fbio, s_to_c_bio);
    if (c_to_s_fbio != NULL)
        c_to_s_bio = BIO_push(c_to_s_fbio, c_to_s_bio);
    if (s_to_c_bio == NULL || c_to_s_bio == NULL) {
        printf("Failed to create chained BIOs\n");
        goto error;
    }

    /* Set Non-blocking IO behaviour */
    BIO_set_mem_eof_return(s_to_c_bio, -1);
    BIO_set_mem_eof_return(c_to_s_bio, -1);

    /* Up ref these as we are passing them to two SSL objects */
    CRYPTO_add(&s_to_c_bio->references, 1, CRYPTO_LOCK_BIO);
    CRYPTO_add(&c_to_s_bio->references, 1, CRYPTO_LOCK_BIO);

    SSL_set_bio(serverssl, c_to_s_bio, s_to_c_bio);
    SSL_set_bio(clientssl, s_to_c_bio, c_to_s_bio);

    /* BIOs will now be freed when SSL objects are freed */
    s_to_c_bio = c_to_s_bio = NULL;
    s_to_c_fbio = c_to_s_fbio = NULL;

    *sssl = serverssl;
    *cssl = clientssl;

    return 1;

 error:
    SSL_free(serverssl);
    SSL_free(clientssl);
    BIO_free(s_to_c_bio);
    BIO_free(c_to_s_bio);
    BIO_free(s_to_c_fbio);
    BIO_free(c_to_s_fbio);

    return 0;
}

int create_ssl_connection(SSL *serverssl, SSL *clientssl)
{
    int retc = -1, rets = -1, err, abortctr = 0;

    do {
        err = SSL_ERROR_WANT_WRITE;
        while (retc <= 0 && err == SSL_ERROR_WANT_WRITE) {
            retc = SSL_connect(clientssl);
            if (retc <= 0)
                err = SSL_get_error(clientssl, retc);
        }

        if (retc <= 0 && err != SSL_ERROR_WANT_READ) {
            printf("SSL_connect() failed %d, %d\n", retc, err);
            return 0;
        }

        err = SSL_ERROR_WANT_WRITE;
        while (rets <= 0 && err == SSL_ERROR_WANT_WRITE) {
            rets = SSL_accept(serverssl);
            if (rets <= 0)
                err = SSL_get_error(serverssl, rets);
        }

        if (rets <= 0 && err != SSL_ERROR_WANT_READ) {
            printf("SSL_accept() failed %d, %d\n", retc, err);
            return 0;
        }
        if (++abortctr == MAXLOOPS) {
            printf("No progress made\n");
            return 0;
        }
    } while (retc <=0 || rets <= 0);

    return 1;
}
