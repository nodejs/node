/* crypto/pem/pem_info.c */
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
#include "cryptlib.h"
#include <openssl/buffer.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#ifndef OPENSSL_NO_RSA
# include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DSA
# include <openssl/dsa.h>
#endif

#ifndef OPENSSL_NO_FP_API
STACK_OF(X509_INFO) *PEM_X509_INFO_read(FILE *fp, STACK_OF(X509_INFO) *sk,
                                        pem_password_cb *cb, void *u)
{
    BIO *b;
    STACK_OF(X509_INFO) *ret;

    if ((b = BIO_new(BIO_s_file())) == NULL) {
        PEMerr(PEM_F_PEM_X509_INFO_READ, ERR_R_BUF_LIB);
        return (0);
    }
    BIO_set_fp(b, fp, BIO_NOCLOSE);
    ret = PEM_X509_INFO_read_bio(b, sk, cb, u);
    BIO_free(b);
    return (ret);
}
#endif

STACK_OF(X509_INFO) *PEM_X509_INFO_read_bio(BIO *bp, STACK_OF(X509_INFO) *sk,
                                            pem_password_cb *cb, void *u)
{
    X509_INFO *xi = NULL;
    char *name = NULL, *header = NULL;
    void *pp;
    unsigned char *data = NULL;
    const unsigned char *p;
    long len, error = 0;
    int ok = 0;
    STACK_OF(X509_INFO) *ret = NULL;
    unsigned int i, raw, ptype;
    d2i_of_void *d2i = 0;

    if (sk == NULL) {
        if ((ret = sk_X509_INFO_new_null()) == NULL) {
            PEMerr(PEM_F_PEM_X509_INFO_READ_BIO, ERR_R_MALLOC_FAILURE);
            goto err;
        }
    } else
        ret = sk;

    if ((xi = X509_INFO_new()) == NULL)
        goto err;
    for (;;) {
        raw = 0;
        ptype = 0;
        i = PEM_read_bio(bp, &name, &header, &data, &len);
        if (i == 0) {
            error = ERR_GET_REASON(ERR_peek_last_error());
            if (error == PEM_R_NO_START_LINE) {
                ERR_clear_error();
                break;
            }
            goto err;
        }
 start:
        if ((strcmp(name, PEM_STRING_X509) == 0) ||
            (strcmp(name, PEM_STRING_X509_OLD) == 0)) {
            d2i = (D2I_OF(void)) d2i_X509;
            if (xi->x509 != NULL) {
                if (!sk_X509_INFO_push(ret, xi))
                    goto err;
                if ((xi = X509_INFO_new()) == NULL)
                    goto err;
                goto start;
            }
            pp = &(xi->x509);
        } else if ((strcmp(name, PEM_STRING_X509_TRUSTED) == 0)) {
            d2i = (D2I_OF(void)) d2i_X509_AUX;
            if (xi->x509 != NULL) {
                if (!sk_X509_INFO_push(ret, xi))
                    goto err;
                if ((xi = X509_INFO_new()) == NULL)
                    goto err;
                goto start;
            }
            pp = &(xi->x509);
        } else if (strcmp(name, PEM_STRING_X509_CRL) == 0) {
            d2i = (D2I_OF(void)) d2i_X509_CRL;
            if (xi->crl != NULL) {
                if (!sk_X509_INFO_push(ret, xi))
                    goto err;
                if ((xi = X509_INFO_new()) == NULL)
                    goto err;
                goto start;
            }
            pp = &(xi->crl);
        } else
#ifndef OPENSSL_NO_RSA
        if (strcmp(name, PEM_STRING_RSA) == 0) {
            d2i = (D2I_OF(void)) d2i_RSAPrivateKey;
            if (xi->x_pkey != NULL) {
                if (!sk_X509_INFO_push(ret, xi))
                    goto err;
                if ((xi = X509_INFO_new()) == NULL)
                    goto err;
                goto start;
            }

            xi->enc_data = NULL;
            xi->enc_len = 0;

            xi->x_pkey = X509_PKEY_new();
            if (xi->x_pkey == NULL)
                goto err;
            ptype = EVP_PKEY_RSA;
            pp = &xi->x_pkey->dec_pkey;
            if ((int)strlen(header) > 10) /* assume encrypted */
                raw = 1;
        } else
#endif
#ifndef OPENSSL_NO_DSA
        if (strcmp(name, PEM_STRING_DSA) == 0) {
            d2i = (D2I_OF(void)) d2i_DSAPrivateKey;
            if (xi->x_pkey != NULL) {
                if (!sk_X509_INFO_push(ret, xi))
                    goto err;
                if ((xi = X509_INFO_new()) == NULL)
                    goto err;
                goto start;
            }

            xi->enc_data = NULL;
            xi->enc_len = 0;

            xi->x_pkey = X509_PKEY_new();
            if (xi->x_pkey == NULL)
                goto err;
            ptype = EVP_PKEY_DSA;
            pp = &xi->x_pkey->dec_pkey;
            if ((int)strlen(header) > 10) /* assume encrypted */
                raw = 1;
        } else
#endif
#ifndef OPENSSL_NO_EC
        if (strcmp(name, PEM_STRING_ECPRIVATEKEY) == 0) {
            d2i = (D2I_OF(void)) d2i_ECPrivateKey;
            if (xi->x_pkey != NULL) {
                if (!sk_X509_INFO_push(ret, xi))
                    goto err;
                if ((xi = X509_INFO_new()) == NULL)
                    goto err;
                goto start;
            }

            xi->enc_data = NULL;
            xi->enc_len = 0;

            xi->x_pkey = X509_PKEY_new();
            if (xi->x_pkey == NULL)
                goto err;
            ptype = EVP_PKEY_EC;
            pp = &xi->x_pkey->dec_pkey;
            if ((int)strlen(header) > 10) /* assume encrypted */
                raw = 1;
        } else
#endif
        {
            d2i = NULL;
            pp = NULL;
        }

        if (d2i != NULL) {
            if (!raw) {
                EVP_CIPHER_INFO cipher;

                if (!PEM_get_EVP_CIPHER_INFO(header, &cipher))
                    goto err;
                if (!PEM_do_header(&cipher, data, &len, cb, u))
                    goto err;
                p = data;
                if (ptype) {
                    if (!d2i_PrivateKey(ptype, pp, &p, len)) {
                        PEMerr(PEM_F_PEM_X509_INFO_READ_BIO, ERR_R_ASN1_LIB);
                        goto err;
                    }
                } else if (d2i(pp, &p, len) == NULL) {
                    PEMerr(PEM_F_PEM_X509_INFO_READ_BIO, ERR_R_ASN1_LIB);
                    goto err;
                }
            } else {            /* encrypted RSA data */
                if (!PEM_get_EVP_CIPHER_INFO(header, &xi->enc_cipher))
                    goto err;
                xi->enc_data = (char *)data;
                xi->enc_len = (int)len;
                data = NULL;
            }
        } else {
            /* unknown */
        }
        if (name != NULL)
            OPENSSL_free(name);
        if (header != NULL)
            OPENSSL_free(header);
        if (data != NULL)
            OPENSSL_free(data);
        name = NULL;
        header = NULL;
        data = NULL;
    }

    /*
     * if the last one hasn't been pushed yet and there is anything in it
     * then add it to the stack ...
     */
    if ((xi->x509 != NULL) || (xi->crl != NULL) ||
        (xi->x_pkey != NULL) || (xi->enc_data != NULL)) {
        if (!sk_X509_INFO_push(ret, xi))
            goto err;
        xi = NULL;
    }
    ok = 1;
 err:
    if (xi != NULL)
        X509_INFO_free(xi);
    if (!ok) {
        for (i = 0; ((int)i) < sk_X509_INFO_num(ret); i++) {
            xi = sk_X509_INFO_value(ret, i);
            X509_INFO_free(xi);
        }
        if (ret != sk)
            sk_X509_INFO_free(ret);
        ret = NULL;
    }

    if (name != NULL)
        OPENSSL_free(name);
    if (header != NULL)
        OPENSSL_free(header);
    if (data != NULL)
        OPENSSL_free(data);
    return (ret);
}

/* A TJH addition */
int PEM_X509_INFO_write_bio(BIO *bp, X509_INFO *xi, EVP_CIPHER *enc,
                            unsigned char *kstr, int klen,
                            pem_password_cb *cb, void *u)
{
    EVP_CIPHER_CTX ctx;
    int i, ret = 0;
    unsigned char *data = NULL;
    const char *objstr = NULL;
    char buf[PEM_BUFSIZE];
    unsigned char *iv = NULL;

    if (enc != NULL) {
        objstr = OBJ_nid2sn(EVP_CIPHER_nid(enc));
        if (objstr == NULL) {
            PEMerr(PEM_F_PEM_X509_INFO_WRITE_BIO, PEM_R_UNSUPPORTED_CIPHER);
            goto err;
        }
    }

    /*
     * now for the fun part ... if we have a private key then we have to be
     * able to handle a not-yet-decrypted key being written out correctly ...
     * if it is decrypted or it is non-encrypted then we use the base code
     */
    if (xi->x_pkey != NULL) {
        if ((xi->enc_data != NULL) && (xi->enc_len > 0)) {
            if (enc == NULL) {
                PEMerr(PEM_F_PEM_X509_INFO_WRITE_BIO, PEM_R_CIPHER_IS_NULL);
                goto err;
            }

            /* copy from weirdo names into more normal things */
            iv = xi->enc_cipher.iv;
            data = (unsigned char *)xi->enc_data;
            i = xi->enc_len;

            /*
             * we take the encryption data from the internal stuff rather
             * than what the user has passed us ... as we have to match
             * exactly for some strange reason
             */
            objstr = OBJ_nid2sn(EVP_CIPHER_nid(xi->enc_cipher.cipher));
            if (objstr == NULL) {
                PEMerr(PEM_F_PEM_X509_INFO_WRITE_BIO,
                       PEM_R_UNSUPPORTED_CIPHER);
                goto err;
            }

            /* create the right magic header stuff */
            OPENSSL_assert(strlen(objstr) + 23 + 2 * enc->iv_len + 13 <=
                           sizeof buf);
            buf[0] = '\0';
            PEM_proc_type(buf, PEM_TYPE_ENCRYPTED);
            PEM_dek_info(buf, objstr, enc->iv_len, (char *)iv);

            /* use the normal code to write things out */
            i = PEM_write_bio(bp, PEM_STRING_RSA, buf, data, i);
            if (i <= 0)
                goto err;
        } else {
            /* Add DSA/DH */
#ifndef OPENSSL_NO_RSA
            /* normal optionally encrypted stuff */
            if (PEM_write_bio_RSAPrivateKey(bp,
                                            xi->x_pkey->dec_pkey->pkey.rsa,
                                            enc, kstr, klen, cb, u) <= 0)
                goto err;
#endif
        }
    }

    /* if we have a certificate then write it out now */
    if ((xi->x509 != NULL) && (PEM_write_bio_X509(bp, xi->x509) <= 0))
        goto err;

    /*
     * we are ignoring anything else that is loaded into the X509_INFO
     * structure for the moment ... as I don't need it so I'm not coding it
     * here and Eric can do it when this makes it into the base library --tjh
     */

    ret = 1;

 err:
    OPENSSL_cleanse((char *)&ctx, sizeof(ctx));
    OPENSSL_cleanse(buf, PEM_BUFSIZE);
    return (ret);
}
