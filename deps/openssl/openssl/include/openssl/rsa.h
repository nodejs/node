/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef HEADER_RSA_H
# define HEADER_RSA_H

# include <openssl/opensslconf.h>

# ifndef OPENSSL_NO_RSA
# include <openssl/asn1.h>
# include <openssl/bio.h>
# include <openssl/crypto.h>
# include <openssl/ossl_typ.h>
# if OPENSSL_API_COMPAT < 0x10100000L
#  include <openssl/bn.h>
# endif
# ifdef  __cplusplus
extern "C" {
# endif

/* The types RSA and RSA_METHOD are defined in ossl_typ.h */

# ifndef OPENSSL_RSA_MAX_MODULUS_BITS
#  define OPENSSL_RSA_MAX_MODULUS_BITS   16384
# endif

# define OPENSSL_RSA_FIPS_MIN_MODULUS_BITS 1024

# ifndef OPENSSL_RSA_SMALL_MODULUS_BITS
#  define OPENSSL_RSA_SMALL_MODULUS_BITS 3072
# endif
# ifndef OPENSSL_RSA_MAX_PUBEXP_BITS

/* exponent limit enforced for "large" modulus only */
#  define OPENSSL_RSA_MAX_PUBEXP_BITS    64
# endif

# define RSA_3   0x3L
# define RSA_F4  0x10001L

# define RSA_METHOD_FLAG_NO_CHECK        0x0001/* don't check pub/private
                                                * match */

# define RSA_FLAG_CACHE_PUBLIC           0x0002
# define RSA_FLAG_CACHE_PRIVATE          0x0004
# define RSA_FLAG_BLINDING               0x0008
# define RSA_FLAG_THREAD_SAFE            0x0010
/*
 * This flag means the private key operations will be handled by rsa_mod_exp
 * and that they do not depend on the private key components being present:
 * for example a key stored in external hardware. Without this flag
 * bn_mod_exp gets called when private key components are absent.
 */
# define RSA_FLAG_EXT_PKEY               0x0020

/*
 * new with 0.9.6j and 0.9.7b; the built-in
 * RSA implementation now uses blinding by
 * default (ignoring RSA_FLAG_BLINDING),
 * but other engines might not need it
 */
# define RSA_FLAG_NO_BLINDING            0x0080
# if OPENSSL_API_COMPAT < 0x10100000L
/*
 * Does nothing. Previously this switched off constant time behaviour.
 */
#  define RSA_FLAG_NO_CONSTTIME           0x0000
# endif
# if OPENSSL_API_COMPAT < 0x00908000L
/* deprecated name for the flag*/
/*
 * new with 0.9.7h; the built-in RSA
 * implementation now uses constant time
 * modular exponentiation for secret exponents
 * by default. This flag causes the
 * faster variable sliding window method to
 * be used for all exponents.
 */
#  define RSA_FLAG_NO_EXP_CONSTTIME RSA_FLAG_NO_CONSTTIME
# endif

# define EVP_PKEY_CTX_set_rsa_padding(ctx, pad) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, -1, EVP_PKEY_CTRL_RSA_PADDING, \
                                pad, NULL)

# define EVP_PKEY_CTX_get_rsa_padding(ctx, ppad) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, -1, \
                                EVP_PKEY_CTRL_GET_RSA_PADDING, 0, ppad)

# define EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, len) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, \
                                (EVP_PKEY_OP_SIGN|EVP_PKEY_OP_VERIFY), \
                                EVP_PKEY_CTRL_RSA_PSS_SALTLEN, \
                                len, NULL)

# define EVP_PKEY_CTX_get_rsa_pss_saltlen(ctx, plen) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, \
                                (EVP_PKEY_OP_SIGN|EVP_PKEY_OP_VERIFY), \
                                EVP_PKEY_CTRL_GET_RSA_PSS_SALTLEN, \
                                0, plen)

# define EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_KEYGEN, \
                                EVP_PKEY_CTRL_RSA_KEYGEN_BITS, bits, NULL)

# define EVP_PKEY_CTX_set_rsa_keygen_pubexp(ctx, pubexp) \
        EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_KEYGEN, \
                                EVP_PKEY_CTRL_RSA_KEYGEN_PUBEXP, 0, pubexp)

# define  EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, md)  \
                EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, \
                        EVP_PKEY_OP_TYPE_SIG | EVP_PKEY_OP_TYPE_CRYPT, \
                                EVP_PKEY_CTRL_RSA_MGF1_MD, 0, (void *)md)

# define  EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md)  \
                EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_TYPE_CRYPT,  \
                                EVP_PKEY_CTRL_RSA_OAEP_MD, 0, (void *)md)

# define  EVP_PKEY_CTX_get_rsa_mgf1_md(ctx, pmd) \
                EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, \
                        EVP_PKEY_OP_TYPE_SIG | EVP_PKEY_OP_TYPE_CRYPT, \
                                EVP_PKEY_CTRL_GET_RSA_MGF1_MD, 0, (void *)pmd)

# define  EVP_PKEY_CTX_get_rsa_oaep_md(ctx, pmd) \
                EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_TYPE_CRYPT,  \
                                EVP_PKEY_CTRL_GET_RSA_OAEP_MD, 0, (void *)pmd)

# define  EVP_PKEY_CTX_set0_rsa_oaep_label(ctx, l, llen) \
                EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_TYPE_CRYPT,  \
                                EVP_PKEY_CTRL_RSA_OAEP_LABEL, llen, (void *)l)

# define  EVP_PKEY_CTX_get0_rsa_oaep_label(ctx, l)       \
                EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_TYPE_CRYPT,  \
                                EVP_PKEY_CTRL_GET_RSA_OAEP_LABEL, 0, (void *)l)

# define EVP_PKEY_CTRL_RSA_PADDING       (EVP_PKEY_ALG_CTRL + 1)
# define EVP_PKEY_CTRL_RSA_PSS_SALTLEN   (EVP_PKEY_ALG_CTRL + 2)

# define EVP_PKEY_CTRL_RSA_KEYGEN_BITS   (EVP_PKEY_ALG_CTRL + 3)
# define EVP_PKEY_CTRL_RSA_KEYGEN_PUBEXP (EVP_PKEY_ALG_CTRL + 4)
# define EVP_PKEY_CTRL_RSA_MGF1_MD       (EVP_PKEY_ALG_CTRL + 5)

# define EVP_PKEY_CTRL_GET_RSA_PADDING           (EVP_PKEY_ALG_CTRL + 6)
# define EVP_PKEY_CTRL_GET_RSA_PSS_SALTLEN       (EVP_PKEY_ALG_CTRL + 7)
# define EVP_PKEY_CTRL_GET_RSA_MGF1_MD           (EVP_PKEY_ALG_CTRL + 8)

# define EVP_PKEY_CTRL_RSA_OAEP_MD       (EVP_PKEY_ALG_CTRL + 9)
# define EVP_PKEY_CTRL_RSA_OAEP_LABEL    (EVP_PKEY_ALG_CTRL + 10)

# define EVP_PKEY_CTRL_GET_RSA_OAEP_MD   (EVP_PKEY_ALG_CTRL + 11)
# define EVP_PKEY_CTRL_GET_RSA_OAEP_LABEL (EVP_PKEY_ALG_CTRL + 12)

# define RSA_PKCS1_PADDING       1
# define RSA_SSLV23_PADDING      2
# define RSA_NO_PADDING          3
# define RSA_PKCS1_OAEP_PADDING  4
# define RSA_X931_PADDING        5
/* EVP_PKEY_ only */
# define RSA_PKCS1_PSS_PADDING   6

# define RSA_PKCS1_PADDING_SIZE  11

# define RSA_set_app_data(s,arg)         RSA_set_ex_data(s,0,arg)
# define RSA_get_app_data(s)             RSA_get_ex_data(s,0)

RSA *RSA_new(void);
RSA *RSA_new_method(ENGINE *engine);
int RSA_bits(const RSA *rsa);
int RSA_size(const RSA *rsa);
int RSA_security_bits(const RSA *rsa);

int RSA_set0_key(RSA *r, BIGNUM *n, BIGNUM *e, BIGNUM *d);
int RSA_set0_factors(RSA *r, BIGNUM *p, BIGNUM *q);
int RSA_set0_crt_params(RSA *r,BIGNUM *dmp1, BIGNUM *dmq1, BIGNUM *iqmp);
void RSA_get0_key(const RSA *r,
                  const BIGNUM **n, const BIGNUM **e, const BIGNUM **d);
void RSA_get0_factors(const RSA *r, const BIGNUM **p, const BIGNUM **q);
void RSA_get0_crt_params(const RSA *r,
                         const BIGNUM **dmp1, const BIGNUM **dmq1,
                         const BIGNUM **iqmp);
void RSA_clear_flags(RSA *r, int flags);
int RSA_test_flags(const RSA *r, int flags);
void RSA_set_flags(RSA *r, int flags);
ENGINE *RSA_get0_engine(const RSA *r);

/* Deprecated version */
DEPRECATEDIN_0_9_8(RSA *RSA_generate_key(int bits, unsigned long e, void
                                         (*callback) (int, int, void *),
                                         void *cb_arg))

/* New version */
int RSA_generate_key_ex(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb);

int RSA_X931_derive_ex(RSA *rsa, BIGNUM *p1, BIGNUM *p2, BIGNUM *q1,
                       BIGNUM *q2, const BIGNUM *Xp1, const BIGNUM *Xp2,
                       const BIGNUM *Xp, const BIGNUM *Xq1, const BIGNUM *Xq2,
                       const BIGNUM *Xq, const BIGNUM *e, BN_GENCB *cb);
int RSA_X931_generate_key_ex(RSA *rsa, int bits, const BIGNUM *e,
                             BN_GENCB *cb);

int RSA_check_key(const RSA *);
int RSA_check_key_ex(const RSA *, BN_GENCB *cb);
        /* next 4 return -1 on error */
int RSA_public_encrypt(int flen, const unsigned char *from,
                       unsigned char *to, RSA *rsa, int padding);
int RSA_private_encrypt(int flen, const unsigned char *from,
                        unsigned char *to, RSA *rsa, int padding);
int RSA_public_decrypt(int flen, const unsigned char *from,
                       unsigned char *to, RSA *rsa, int padding);
int RSA_private_decrypt(int flen, const unsigned char *from,
                        unsigned char *to, RSA *rsa, int padding);
void RSA_free(RSA *r);
/* "up" the RSA object's reference count */
int RSA_up_ref(RSA *r);

int RSA_flags(const RSA *r);

void RSA_set_default_method(const RSA_METHOD *meth);
const RSA_METHOD *RSA_get_default_method(void);
const RSA_METHOD *RSA_get_method(const RSA *rsa);
int RSA_set_method(RSA *rsa, const RSA_METHOD *meth);

/* these are the actual RSA functions */
const RSA_METHOD *RSA_PKCS1_OpenSSL(void);

const RSA_METHOD *RSA_null_method(void);

DECLARE_ASN1_ENCODE_FUNCTIONS_const(RSA, RSAPublicKey)
DECLARE_ASN1_ENCODE_FUNCTIONS_const(RSA, RSAPrivateKey)

typedef struct rsa_pss_params_st {
    X509_ALGOR *hashAlgorithm;
    X509_ALGOR *maskGenAlgorithm;
    ASN1_INTEGER *saltLength;
    ASN1_INTEGER *trailerField;
} RSA_PSS_PARAMS;

DECLARE_ASN1_FUNCTIONS(RSA_PSS_PARAMS)

typedef struct rsa_oaep_params_st {
    X509_ALGOR *hashFunc;
    X509_ALGOR *maskGenFunc;
    X509_ALGOR *pSourceFunc;
} RSA_OAEP_PARAMS;

DECLARE_ASN1_FUNCTIONS(RSA_OAEP_PARAMS)

# ifndef OPENSSL_NO_STDIO
int RSA_print_fp(FILE *fp, const RSA *r, int offset);
# endif

int RSA_print(BIO *bp, const RSA *r, int offset);

/*
 * The following 2 functions sign and verify a X509_SIG ASN1 object inside
 * PKCS#1 padded RSA encryption
 */
int RSA_sign(int type, const unsigned char *m, unsigned int m_length,
             unsigned char *sigret, unsigned int *siglen, RSA *rsa);
int RSA_verify(int type, const unsigned char *m, unsigned int m_length,
               const unsigned char *sigbuf, unsigned int siglen, RSA *rsa);

/*
 * The following 2 function sign and verify a ASN1_OCTET_STRING object inside
 * PKCS#1 padded RSA encryption
 */
int RSA_sign_ASN1_OCTET_STRING(int type,
                               const unsigned char *m, unsigned int m_length,
                               unsigned char *sigret, unsigned int *siglen,
                               RSA *rsa);
int RSA_verify_ASN1_OCTET_STRING(int type, const unsigned char *m,
                                 unsigned int m_length, unsigned char *sigbuf,
                                 unsigned int siglen, RSA *rsa);

int RSA_blinding_on(RSA *rsa, BN_CTX *ctx);
void RSA_blinding_off(RSA *rsa);
BN_BLINDING *RSA_setup_blinding(RSA *rsa, BN_CTX *ctx);

int RSA_padding_add_PKCS1_type_1(unsigned char *to, int tlen,
                                 const unsigned char *f, int fl);
int RSA_padding_check_PKCS1_type_1(unsigned char *to, int tlen,
                                   const unsigned char *f, int fl,
                                   int rsa_len);
int RSA_padding_add_PKCS1_type_2(unsigned char *to, int tlen,
                                 const unsigned char *f, int fl);
int RSA_padding_check_PKCS1_type_2(unsigned char *to, int tlen,
                                   const unsigned char *f, int fl,
                                   int rsa_len);
int PKCS1_MGF1(unsigned char *mask, long len, const unsigned char *seed,
               long seedlen, const EVP_MD *dgst);
int RSA_padding_add_PKCS1_OAEP(unsigned char *to, int tlen,
                               const unsigned char *f, int fl,
                               const unsigned char *p, int pl);
int RSA_padding_check_PKCS1_OAEP(unsigned char *to, int tlen,
                                 const unsigned char *f, int fl, int rsa_len,
                                 const unsigned char *p, int pl);
int RSA_padding_add_PKCS1_OAEP_mgf1(unsigned char *to, int tlen,
                                    const unsigned char *from, int flen,
                                    const unsigned char *param, int plen,
                                    const EVP_MD *md, const EVP_MD *mgf1md);
int RSA_padding_check_PKCS1_OAEP_mgf1(unsigned char *to, int tlen,
                                      const unsigned char *from, int flen,
                                      int num, const unsigned char *param,
                                      int plen, const EVP_MD *md,
                                      const EVP_MD *mgf1md);
int RSA_padding_add_SSLv23(unsigned char *to, int tlen,
                           const unsigned char *f, int fl);
int RSA_padding_check_SSLv23(unsigned char *to, int tlen,
                             const unsigned char *f, int fl, int rsa_len);
int RSA_padding_add_none(unsigned char *to, int tlen, const unsigned char *f,
                         int fl);
int RSA_padding_check_none(unsigned char *to, int tlen,
                           const unsigned char *f, int fl, int rsa_len);
int RSA_padding_add_X931(unsigned char *to, int tlen, const unsigned char *f,
                         int fl);
int RSA_padding_check_X931(unsigned char *to, int tlen,
                           const unsigned char *f, int fl, int rsa_len);
int RSA_X931_hash_id(int nid);

int RSA_verify_PKCS1_PSS(RSA *rsa, const unsigned char *mHash,
                         const EVP_MD *Hash, const unsigned char *EM,
                         int sLen);
int RSA_padding_add_PKCS1_PSS(RSA *rsa, unsigned char *EM,
                              const unsigned char *mHash, const EVP_MD *Hash,
                              int sLen);

int RSA_verify_PKCS1_PSS_mgf1(RSA *rsa, const unsigned char *mHash,
                              const EVP_MD *Hash, const EVP_MD *mgf1Hash,
                              const unsigned char *EM, int sLen);

int RSA_padding_add_PKCS1_PSS_mgf1(RSA *rsa, unsigned char *EM,
                                   const unsigned char *mHash,
                                   const EVP_MD *Hash, const EVP_MD *mgf1Hash,
                                   int sLen);

#define RSA_get_ex_new_index(l, p, newf, dupf, freef) \
    CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_RSA, l, p, newf, dupf, freef)
int RSA_set_ex_data(RSA *r, int idx, void *arg);
void *RSA_get_ex_data(const RSA *r, int idx);

RSA *RSAPublicKey_dup(RSA *rsa);
RSA *RSAPrivateKey_dup(RSA *rsa);

/*
 * If this flag is set the RSA method is FIPS compliant and can be used in
 * FIPS mode. This is set in the validated module method. If an application
 * sets this flag in its own methods it is its responsibility to ensure the
 * result is compliant.
 */

# define RSA_FLAG_FIPS_METHOD                    0x0400

/*
 * If this flag is set the operations normally disabled in FIPS mode are
 * permitted it is then the applications responsibility to ensure that the
 * usage is compliant.
 */

# define RSA_FLAG_NON_FIPS_ALLOW                 0x0400
/*
 * Application has decided PRNG is good enough to generate a key: don't
 * check.
 */
# define RSA_FLAG_CHECKED                        0x0800

RSA_METHOD *RSA_meth_new(const char *name, int flags);
void RSA_meth_free(RSA_METHOD *meth);
RSA_METHOD *RSA_meth_dup(const RSA_METHOD *meth);
const char *RSA_meth_get0_name(const RSA_METHOD *meth);
int RSA_meth_set1_name(RSA_METHOD *meth, const char *name);
int RSA_meth_get_flags(const RSA_METHOD *meth);
int RSA_meth_set_flags(RSA_METHOD *meth, int flags);
void *RSA_meth_get0_app_data(const RSA_METHOD *meth);
int RSA_meth_set0_app_data(RSA_METHOD *meth, void *app_data);
int (*RSA_meth_get_pub_enc(const RSA_METHOD *meth))
    (int flen, const unsigned char *from,
     unsigned char *to, RSA *rsa, int padding);
int RSA_meth_set_pub_enc(RSA_METHOD *rsa,
                         int (*pub_enc) (int flen, const unsigned char *from,
                                         unsigned char *to, RSA *rsa,
                                         int padding));
int (*RSA_meth_get_pub_dec(const RSA_METHOD *meth))
    (int flen, const unsigned char *from,
     unsigned char *to, RSA *rsa, int padding);
int RSA_meth_set_pub_dec(RSA_METHOD *rsa,
                         int (*pub_dec) (int flen, const unsigned char *from,
                                         unsigned char *to, RSA *rsa,
                                         int padding));
int (*RSA_meth_get_priv_enc(const RSA_METHOD *meth))
    (int flen, const unsigned char *from,
     unsigned char *to, RSA *rsa, int padding);
int RSA_meth_set_priv_enc(RSA_METHOD *rsa,
                          int (*priv_enc) (int flen, const unsigned char *from,
                                           unsigned char *to, RSA *rsa,
                                           int padding));
int (*RSA_meth_get_priv_dec(const RSA_METHOD *meth))
    (int flen, const unsigned char *from,
     unsigned char *to, RSA *rsa, int padding);
int RSA_meth_set_priv_dec(RSA_METHOD *rsa,
                          int (*priv_dec) (int flen, const unsigned char *from,
                                           unsigned char *to, RSA *rsa,
                                           int padding));
int (*RSA_meth_get_mod_exp(const RSA_METHOD *meth))
    (BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx);
int RSA_meth_set_mod_exp(RSA_METHOD *rsa,
                         int (*mod_exp) (BIGNUM *r0, const BIGNUM *I, RSA *rsa,
                                         BN_CTX *ctx));
int (*RSA_meth_get_bn_mod_exp(const RSA_METHOD *meth))
    (BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
     const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
int RSA_meth_set_bn_mod_exp(RSA_METHOD *rsa,
                            int (*bn_mod_exp) (BIGNUM *r,
                                               const BIGNUM *a,
                                               const BIGNUM *p,
                                               const BIGNUM *m,
                                               BN_CTX *ctx,
                                               BN_MONT_CTX *m_ctx));
int (*RSA_meth_get_init(const RSA_METHOD *meth)) (RSA *rsa);
int RSA_meth_set_init(RSA_METHOD *rsa, int (*init) (RSA *rsa));
int (*RSA_meth_get_finish(const RSA_METHOD *meth)) (RSA *rsa);
int RSA_meth_set_finish(RSA_METHOD *rsa, int (*finish) (RSA *rsa));
int (*RSA_meth_get_sign(const RSA_METHOD *meth))
    (int type,
     const unsigned char *m, unsigned int m_length,
     unsigned char *sigret, unsigned int *siglen,
     const RSA *rsa);
int RSA_meth_set_sign(RSA_METHOD *rsa,
                      int (*sign) (int type, const unsigned char *m,
                                   unsigned int m_length,
                                   unsigned char *sigret, unsigned int *siglen,
                                   const RSA *rsa));
int (*RSA_meth_get_verify(const RSA_METHOD *meth))
    (int dtype, const unsigned char *m,
     unsigned int m_length, const unsigned char *sigbuf,
     unsigned int siglen, const RSA *rsa);
int RSA_meth_set_verify(RSA_METHOD *rsa,
                        int (*verify) (int dtype, const unsigned char *m,
                                       unsigned int m_length,
                                       const unsigned char *sigbuf,
                                       unsigned int siglen, const RSA *rsa));
int (*RSA_meth_get_keygen(const RSA_METHOD *meth))
    (RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb);
int RSA_meth_set_keygen(RSA_METHOD *rsa,
                        int (*keygen) (RSA *rsa, int bits, BIGNUM *e,
                                       BN_GENCB *cb));

/* BEGIN ERROR CODES */
/*
 * The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */

int ERR_load_RSA_strings(void);

/* Error codes for the RSA functions. */

/* Function codes. */
# define RSA_F_CHECK_PADDING_MD                           140
# define RSA_F_ENCODE_PKCS1                               146
# define RSA_F_INT_RSA_VERIFY                             145
# define RSA_F_OLD_RSA_PRIV_DECODE                        147
# define RSA_F_PKEY_RSA_CTRL                              143
# define RSA_F_PKEY_RSA_CTRL_STR                          144
# define RSA_F_PKEY_RSA_SIGN                              142
# define RSA_F_PKEY_RSA_VERIFY                            149
# define RSA_F_PKEY_RSA_VERIFYRECOVER                     141
# define RSA_F_RSA_ALGOR_TO_MD                            156
# define RSA_F_RSA_BUILTIN_KEYGEN                         129
# define RSA_F_RSA_CHECK_KEY                              123
# define RSA_F_RSA_CHECK_KEY_EX                           160
# define RSA_F_RSA_CMS_DECRYPT                            159
# define RSA_F_RSA_ITEM_VERIFY                            148
# define RSA_F_RSA_METH_DUP                               161
# define RSA_F_RSA_METH_NEW                               162
# define RSA_F_RSA_METH_SET1_NAME                         163
# define RSA_F_RSA_MGF1_TO_MD                             157
# define RSA_F_RSA_NEW_METHOD                             106
# define RSA_F_RSA_NULL                                   124
# define RSA_F_RSA_NULL_PRIVATE_DECRYPT                   132
# define RSA_F_RSA_NULL_PRIVATE_ENCRYPT                   133
# define RSA_F_RSA_NULL_PUBLIC_DECRYPT                    134
# define RSA_F_RSA_NULL_PUBLIC_ENCRYPT                    135
# define RSA_F_RSA_OSSL_PRIVATE_DECRYPT                   101
# define RSA_F_RSA_OSSL_PRIVATE_ENCRYPT                   102
# define RSA_F_RSA_OSSL_PUBLIC_DECRYPT                    103
# define RSA_F_RSA_OSSL_PUBLIC_ENCRYPT                    104
# define RSA_F_RSA_PADDING_ADD_NONE                       107
# define RSA_F_RSA_PADDING_ADD_PKCS1_OAEP                 121
# define RSA_F_RSA_PADDING_ADD_PKCS1_OAEP_MGF1            154
# define RSA_F_RSA_PADDING_ADD_PKCS1_PSS                  125
# define RSA_F_RSA_PADDING_ADD_PKCS1_PSS_MGF1             152
# define RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_1               108
# define RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_2               109
# define RSA_F_RSA_PADDING_ADD_SSLV23                     110
# define RSA_F_RSA_PADDING_ADD_X931                       127
# define RSA_F_RSA_PADDING_CHECK_NONE                     111
# define RSA_F_RSA_PADDING_CHECK_PKCS1_OAEP               122
# define RSA_F_RSA_PADDING_CHECK_PKCS1_OAEP_MGF1          153
# define RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1             112
# define RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_2             113
# define RSA_F_RSA_PADDING_CHECK_SSLV23                   114
# define RSA_F_RSA_PADDING_CHECK_X931                     128
# define RSA_F_RSA_PRINT                                  115
# define RSA_F_RSA_PRINT_FP                               116
# define RSA_F_RSA_PRIV_ENCODE                            138
# define RSA_F_RSA_PSS_TO_CTX                             155
# define RSA_F_RSA_PUB_DECODE                             139
# define RSA_F_RSA_SETUP_BLINDING                         136
# define RSA_F_RSA_SIGN                                   117
# define RSA_F_RSA_SIGN_ASN1_OCTET_STRING                 118
# define RSA_F_RSA_VERIFY                                 119
# define RSA_F_RSA_VERIFY_ASN1_OCTET_STRING               120
# define RSA_F_RSA_VERIFY_PKCS1_PSS_MGF1                  126

/* Reason codes. */
# define RSA_R_ALGORITHM_MISMATCH                         100
# define RSA_R_BAD_E_VALUE                                101
# define RSA_R_BAD_FIXED_HEADER_DECRYPT                   102
# define RSA_R_BAD_PAD_BYTE_COUNT                         103
# define RSA_R_BAD_SIGNATURE                              104
# define RSA_R_BLOCK_TYPE_IS_NOT_01                       106
# define RSA_R_BLOCK_TYPE_IS_NOT_02                       107
# define RSA_R_DATA_GREATER_THAN_MOD_LEN                  108
# define RSA_R_DATA_TOO_LARGE                             109
# define RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE                110
# define RSA_R_DATA_TOO_LARGE_FOR_MODULUS                 132
# define RSA_R_DATA_TOO_SMALL                             111
# define RSA_R_DATA_TOO_SMALL_FOR_KEY_SIZE                122
# define RSA_R_DIGEST_DOES_NOT_MATCH                      158
# define RSA_R_DIGEST_TOO_BIG_FOR_RSA_KEY                 112
# define RSA_R_DMP1_NOT_CONGRUENT_TO_D                    124
# define RSA_R_DMQ1_NOT_CONGRUENT_TO_D                    125
# define RSA_R_D_E_NOT_CONGRUENT_TO_1                     123
# define RSA_R_FIRST_OCTET_INVALID                        133
# define RSA_R_ILLEGAL_OR_UNSUPPORTED_PADDING_MODE        144
# define RSA_R_INVALID_DIGEST                             157
# define RSA_R_INVALID_DIGEST_LENGTH                      143
# define RSA_R_INVALID_HEADER                             137
# define RSA_R_INVALID_LABEL                              160
# define RSA_R_INVALID_MESSAGE_LENGTH                     131
# define RSA_R_INVALID_MGF1_MD                            156
# define RSA_R_INVALID_OAEP_PARAMETERS                    161
# define RSA_R_INVALID_PADDING                            138
# define RSA_R_INVALID_PADDING_MODE                       141
# define RSA_R_INVALID_PSS_PARAMETERS                     149
# define RSA_R_INVALID_PSS_SALTLEN                        146
# define RSA_R_INVALID_SALT_LENGTH                        150
# define RSA_R_INVALID_TRAILER                            139
# define RSA_R_INVALID_X931_DIGEST                        142
# define RSA_R_IQMP_NOT_INVERSE_OF_Q                      126
# define RSA_R_KEY_SIZE_TOO_SMALL                         120
# define RSA_R_LAST_OCTET_INVALID                         134
# define RSA_R_MODULUS_TOO_LARGE                          105
# define RSA_R_NO_PUBLIC_EXPONENT                         140
# define RSA_R_NULL_BEFORE_BLOCK_MISSING                  113
# define RSA_R_N_DOES_NOT_EQUAL_P_Q                       127
# define RSA_R_OAEP_DECODING_ERROR                        121
# define RSA_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE   148
# define RSA_R_PADDING_CHECK_FAILED                       114
# define RSA_R_PKCS_DECODING_ERROR                        159
# define RSA_R_P_NOT_PRIME                                128
# define RSA_R_Q_NOT_PRIME                                129
# define RSA_R_RSA_OPERATIONS_NOT_SUPPORTED               130
# define RSA_R_SLEN_CHECK_FAILED                          136
# define RSA_R_SLEN_RECOVERY_FAILED                       135
# define RSA_R_SSLV3_ROLLBACK_ATTACK                      115
# define RSA_R_THE_ASN1_OBJECT_IDENTIFIER_IS_NOT_KNOWN_FOR_THIS_MD 116
# define RSA_R_UNKNOWN_ALGORITHM_TYPE                     117
# define RSA_R_UNKNOWN_DIGEST                             166
# define RSA_R_UNKNOWN_MASK_DIGEST                        151
# define RSA_R_UNKNOWN_PADDING_TYPE                       118
# define RSA_R_UNSUPPORTED_ENCRYPTION_TYPE                162
# define RSA_R_UNSUPPORTED_LABEL_SOURCE                   163
# define RSA_R_UNSUPPORTED_MASK_ALGORITHM                 153
# define RSA_R_UNSUPPORTED_MASK_PARAMETER                 154
# define RSA_R_UNSUPPORTED_SIGNATURE_TYPE                 155
# define RSA_R_VALUE_MISSING                              147
# define RSA_R_WRONG_SIGNATURE_LENGTH                     119

#  ifdef  __cplusplus
}
#  endif
# endif
#endif
