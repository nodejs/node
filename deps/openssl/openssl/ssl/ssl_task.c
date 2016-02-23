/* ssl/ssl_task.c */
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

/* VMS */
/*-
 * DECnet object for servicing SSL.  We accept the inbound and speak a
 * simple protocol for multiplexing the 2 data streams (application and
 * ssl data) over this logical link.
 *
 * Logical names:
 *    SSL_CIPHER        Defines a list of cipher specifications the server
 *                      will support in order of preference.
 *    SSL_SERVER_CERTIFICATE
 *                      Points to PEM (privacy enhanced mail) file that
 *                      contains the server certificate and private password.
 *    SYS$NET           Logical created by netserver.exe as hook for completing
 *                      DECnet logical link.
 *
 * Each NSP message sent over the DECnet link has the following structure:
 *    struct rpc_msg {
 *      char channel;
 *      char function;
 *      short length;
 *      char data[MAX_DATA];
 *    } msg;
 *
 * The channel field designates the virtual data stream this message applies
 * to and is one of:
 *   A - Application data (payload).
 *   R - Remote client connection that initiated the SSL connection.  Encrypted
 *       data is sent over this connection.
 *   G - General data, reserved for future use.
 *
 * The data streams are half-duplex read/write and have following functions:
 *   G - Get, requests that up to msg.length bytes of data be returned.  The
 *       data is returned in the next 'C' function response that matches the
 *       requesting channel.
 *   P - Put, requests that the first msg.length bytes of msg.data be appended
 *       to the designated stream.
 *   C - Confirms a get or put.  Every get and put will get a confirm response,
 *       you cannot initiate another function on a channel until the previous
 *       operation has been confirmed.
 *
 *  The 2 channels may interleave their operations, for example:
 *        Server msg           Client msg
 *         A, Get, 4092          ---->
 *                               <----  R, get, 4092
 *         R, Confirm, {hello}   ---->
 *                               <----  R, put, {srv hello}
 *         R, Confirm, 0         ---->
 *                               .              (SSL handshake completed)
 *                               .              (read first app data).
 *                               <----  A, confirm, {http data}
 *         A, Put, {http data}   ---->
 *                               <----  A, confirm, 0
 *
 *  The length field is not permitted to be larger that 4092 bytes.
 *
 * Author: Dave Jones
 * Date:   22-JUL-1996
 */
#include <stdlib.h>
#include <stdio.h>
#include <iodef.h>              /* VMS IO$_ definitions */
#include <descrip.h>            /* VMS string descriptors */
extern int SYS$QIOW(), SYS$ASSIGN();
int LIB$INIT_TIMER(), LIB$SHOW_TIMER();

#include <string.h>             /* from ssltest.c */
#include <errno.h>

#include "e_os.h"

#include <openssl/buffer.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

int MS_CALLBACK verify_callback(int ok, X509 *xs, X509 *xi, int depth,
                                int error);
BIO *bio_err = NULL;
BIO *bio_stdout = NULL;
BIO_METHOD *BIO_s_rtcp();

static char *cipher = NULL;
int verbose = 1;
#ifdef FIONBIO
static int s_nbio = 0;
#endif
#define TEST_SERVER_CERT "SSL_SERVER_CERTIFICATE"
/*************************************************************************/
/* Should have member alignment inhibited */
struct rpc_msg {
    /* 'A'-app data. 'R'-remote client 'G'-global */
    char channel;
    /* 'G'-get, 'P'-put, 'C'-confirm, 'X'-close */
    char function;
    /* Amount of data returned or max to return */
    unsigned short int length;
    /* variable data */
    char data[4092];
};
#define RPC_HDR_SIZE (sizeof(struct rpc_msg) - 4092)

static $DESCRIPTOR(sysnet, "SYS$NET");
typedef unsigned short io_channel;

struct io_status {
    unsigned short status;
    unsigned short count;
    unsigned long stsval;
};
int doit(io_channel chan, SSL_CTX *s_ctx);
/*****************************************************************************/
/*
 * Decnet I/O routines.
 */
static int get(io_channel chan, char *buffer, int maxlen, int *length)
{
    int status;
    struct io_status iosb;
    status = SYS$QIOW(0, chan, IO$_READVBLK, &iosb, 0, 0,
                      buffer, maxlen, 0, 0, 0, 0);
    if ((status & 1) == 1)
        status = iosb.status;
    if ((status & 1) == 1)
        *length = iosb.count;
    return status;
}

static int put(io_channel chan, char *buffer, int length)
{
    int status;
    struct io_status iosb;
    status = SYS$QIOW(0, chan, IO$_WRITEVBLK, &iosb, 0, 0,
                      buffer, length, 0, 0, 0, 0);
    if ((status & 1) == 1)
        status = iosb.status;
    return status;
}

/***************************************************************************/
/*
 * Handle operations on the 'G' channel.
 */
static int general_request(io_channel chan, struct rpc_msg *msg, int length)
{
    return 48;
}

/***************************************************************************/
int main(int argc, char **argv)
{
    int status, length;
    io_channel chan;
    struct rpc_msg msg;

    char *CApath = NULL, *CAfile = NULL;
    int badop = 0;
    int ret = 1;
    int client_auth = 0;
    int server_auth = 0;
    SSL_CTX *s_ctx = NULL;
    /*
     * Confirm logical link with initiating client.
     */
    LIB$INIT_TIMER();
    status = SYS$ASSIGN(&sysnet, &chan, 0, 0, 0);
    printf("status of assign to SYS$NET: %d\n", status);
    /*
     * Initialize standard out and error files.
     */
    if (bio_err == NULL)
        if ((bio_err = BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_err, stderr, BIO_NOCLOSE);
    if (bio_stdout == NULL)
        if ((bio_stdout = BIO_new(BIO_s_file())) != NULL)
            BIO_set_fp(bio_stdout, stdout, BIO_NOCLOSE);
    /*
     * get the preferred cipher list and other initialization
     */
    if (cipher == NULL)
        cipher = getenv("SSL_CIPHER");
    printf("cipher list: %s\n", cipher ? cipher : "{undefined}");

    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    /*
     * DRM, this was the original, but there is no such thing as SSLv2()
     * s_ctx=SSL_CTX_new(SSLv2());
     */
    s_ctx = SSL_CTX_new(SSLv2_server_method());

    if (s_ctx == NULL)
        goto end;

    SSL_CTX_use_certificate_file(s_ctx, TEST_SERVER_CERT, SSL_FILETYPE_PEM);
    SSL_CTX_use_RSAPrivateKey_file(s_ctx, TEST_SERVER_CERT, SSL_FILETYPE_PEM);
    printf("Loaded server certificate: '%s'\n", TEST_SERVER_CERT);

    /*
     * Take commands from client until bad status.
     */
    LIB$SHOW_TIMER();
    status = doit(chan, s_ctx);
    LIB$SHOW_TIMER();
    /*
     * do final cleanup and exit.
     */
 end:
    if (s_ctx != NULL)
        SSL_CTX_free(s_ctx);
    LIB$SHOW_TIMER();
    return 1;
}

int doit(io_channel chan, SSL_CTX *s_ctx)
{
    int status, length, link_state;
    struct rpc_msg msg;

    SSL *s_ssl = NULL;
    BIO *c_to_s = NULL;
    BIO *s_to_c = NULL;
    BIO *c_bio = NULL;
    BIO *s_bio = NULL;
    int i;
    int done = 0;

    s_ssl = SSL_new(s_ctx);
    if (s_ssl == NULL)
        goto err;

    c_to_s = BIO_new(BIO_s_rtcp());
    s_to_c = BIO_new(BIO_s_rtcp());
    if ((s_to_c == NULL) || (c_to_s == NULL))
        goto err;
/*- original, DRM 24-SEP-1997
    BIO_set_fd ( c_to_s, "", chan );
    BIO_set_fd ( s_to_c, "", chan );
*/
    BIO_set_fd(c_to_s, 0, chan);
    BIO_set_fd(s_to_c, 0, chan);

    c_bio = BIO_new(BIO_f_ssl());
    s_bio = BIO_new(BIO_f_ssl());
    if ((c_bio == NULL) || (s_bio == NULL))
        goto err;

    SSL_set_accept_state(s_ssl);
    SSL_set_bio(s_ssl, c_to_s, s_to_c);
    BIO_set_ssl(s_bio, s_ssl, BIO_CLOSE);

    /* We can always do writes */
    printf("Begin doit main loop\n");
    /*
     * Link states: 0-idle, 1-read pending, 2-write pending, 3-closed.
     */
    for (link_state = 0; link_state < 3;) {
        /*
         * Wait for remote end to request data action on A channel.
         */
        while (link_state == 0) {
            status = get(chan, (char *)&msg, sizeof(msg), &length);
            if ((status & 1) == 0) {
                printf("Error in main loop get: %d\n", status);
                link_state = 3;
                break;
            }
            if (length < RPC_HDR_SIZE) {
                printf("Error in main loop get size: %d\n", length);
                break;
                link_state = 3;
            }
            if (msg.channel != 'A') {
                printf("Error in main loop, unexpected channel: %c\n",
                       msg.channel);
                break;
                link_state = 3;
            }
            if (msg.function == 'G') {
                link_state = 1;
            } else if (msg.function == 'P') {
                link_state = 2; /* write pending */
            } else if (msg.function == 'X') {
                link_state = 3;
            } else {
                link_state = 3;
            }
        }
        if (link_state == 1) {
            i = BIO_read(s_bio, msg.data, msg.length);
            if (i < 0)
                link_state = 3;
            else {
                msg.channel = 'A';
                msg.function = 'C'; /* confirm */
                msg.length = i;
                status = put(chan, (char *)&msg, i + RPC_HDR_SIZE);
                if ((status & 1) == 0)
                    break;
                link_state = 0;
            }
        } else if (link_state == 2) {
            i = BIO_write(s_bio, msg.data, msg.length);
            if (i < 0)
                link_state = 3;
            else {
                msg.channel = 'A';
                msg.function = 'C'; /* confirm */
                msg.length = 0;
                status = put(chan, (char *)&msg, RPC_HDR_SIZE);
                if ((status & 1) == 0)
                    break;
                link_state = 0;
            }
        }
    }
    fprintf(stdout, "DONE\n");
 err:
    /*
     * We have to set the BIO's to NULL otherwise they will be free()ed
     * twice.  Once when th s_ssl is SSL_free()ed and again when c_ssl is
     * SSL_free()ed. This is a hack required because s_ssl and c_ssl are
     * sharing the same BIO structure and SSL_set_bio() and SSL_free()
     * automatically BIO_free non NULL entries. You should not normally do
     * this or be required to do this
     */
    s_ssl->rbio = NULL;
    s_ssl->wbio = NULL;

    if (c_to_s != NULL)
        BIO_free(c_to_s);
    if (s_to_c != NULL)
        BIO_free(s_to_c);
    if (c_bio != NULL)
        BIO_free(c_bio);
    if (s_bio != NULL)
        BIO_free(s_bio);
    return (0);
}
