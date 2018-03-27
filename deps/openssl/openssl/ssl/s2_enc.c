/* ssl/s2_enc.c */
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

#include "ssl_locl.h"
#ifndef OPENSSL_NO_SSL2
# include <stdio.h>

int ssl2_enc_init(SSL *s, int client)
{
    /* Max number of bytes needed */
    EVP_CIPHER_CTX *rs, *ws;
    const EVP_CIPHER *c;
    const EVP_MD *md;
    int num;

    if (!ssl_cipher_get_evp(s->session, &c, &md, NULL, NULL, NULL)) {
        ssl2_return_error(s, SSL2_PE_NO_CIPHER);
        SSLerr(SSL_F_SSL2_ENC_INIT, SSL_R_PROBLEMS_MAPPING_CIPHER_FUNCTIONS);
        return (0);
    }
    ssl_replace_hash(&s->read_hash, md);
    ssl_replace_hash(&s->write_hash, md);

    if ((s->enc_read_ctx == NULL) && ((s->enc_read_ctx = (EVP_CIPHER_CTX *)
                                       OPENSSL_malloc(sizeof(EVP_CIPHER_CTX)))
                                      == NULL))
        goto err;

    /*
     * make sure it's intialized in case the malloc for enc_write_ctx fails
     * and we exit with an error
     */
    rs = s->enc_read_ctx;
    EVP_CIPHER_CTX_init(rs);

    if ((s->enc_write_ctx == NULL) && ((s->enc_write_ctx = (EVP_CIPHER_CTX *)
                                        OPENSSL_malloc(sizeof
                                                       (EVP_CIPHER_CTX))) ==
                                       NULL))
        goto err;

    ws = s->enc_write_ctx;
    EVP_CIPHER_CTX_init(ws);

    num = c->key_len;
    s->s2->key_material_length = num * 2;
    OPENSSL_assert(s->s2->key_material_length <= sizeof(s->s2->key_material));

    if (ssl2_generate_key_material(s) <= 0)
        return 0;

    OPENSSL_assert(c->iv_len <= (int)sizeof(s->session->key_arg));
    EVP_EncryptInit_ex(ws, c, NULL,
                       &(s->s2->key_material[(client) ? num : 0]),
                       s->session->key_arg);
    EVP_DecryptInit_ex(rs, c, NULL,
                       &(s->s2->key_material[(client) ? 0 : num]),
                       s->session->key_arg);
    s->s2->read_key = &(s->s2->key_material[(client) ? 0 : num]);
    s->s2->write_key = &(s->s2->key_material[(client) ? num : 0]);
    return (1);
 err:
    SSLerr(SSL_F_SSL2_ENC_INIT, ERR_R_MALLOC_FAILURE);
    return (0);
}

/*
 * read/writes from s->s2->mac_data using length for encrypt and decrypt.
 * It sets s->s2->padding and s->[rw]length if we are encrypting Returns 0 on
 * error and 1 on success
 */
int ssl2_enc(SSL *s, int send)
{
    EVP_CIPHER_CTX *ds;
    unsigned long l;
    int bs;

    if (send) {
        ds = s->enc_write_ctx;
        l = s->s2->wlength;
    } else {
        ds = s->enc_read_ctx;
        l = s->s2->rlength;
    }

    /* check for NULL cipher */
    if (ds == NULL)
        return 1;

    bs = ds->cipher->block_size;
    /*
     * This should be using (bs-1) and bs instead of 7 and 8, but what the
     * hell.
     */
    if (bs == 8)
        l = (l + 7) / 8 * 8;

    if (EVP_Cipher(ds, s->s2->mac_data, s->s2->mac_data, l) < 1)
        return 0;

    return 1;
}

void ssl2_mac(SSL *s, unsigned char *md, int send)
{
    EVP_MD_CTX c;
    unsigned char sequence[4], *p, *sec, *act;
    unsigned long seq;
    unsigned int len;

    if (send) {
        seq = s->s2->write_sequence;
        sec = s->s2->write_key;
        len = s->s2->wact_data_length;
        act = s->s2->wact_data;
    } else {
        seq = s->s2->read_sequence;
        sec = s->s2->read_key;
        len = s->s2->ract_data_length;
        act = s->s2->ract_data;
    }

    p = &(sequence[0]);
    l2n(seq, p);

    /* There has to be a MAC algorithm. */
    EVP_MD_CTX_init(&c);
    EVP_MD_CTX_copy(&c, s->read_hash);
    EVP_DigestUpdate(&c, sec, EVP_CIPHER_CTX_key_length(s->enc_read_ctx));
    EVP_DigestUpdate(&c, act, len);
    /* the above line also does the pad data */
    EVP_DigestUpdate(&c, sequence, 4);
    EVP_DigestFinal_ex(&c, md, NULL);
    EVP_MD_CTX_cleanup(&c);
}
#else                           /* !OPENSSL_NO_SSL2 */

# if PEDANTIC
static void *dummy = &dummy;
# endif

#endif
