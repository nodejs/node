/* crypto/err/err.h */
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
/* ====================================================================
 * Copyright (c) 1998-2019 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#ifndef HEADER_ERR_H
# define HEADER_ERR_H

# include <openssl/e_os2.h>

# ifndef OPENSSL_NO_FP_API
#  include <stdio.h>
#  include <stdlib.h>
# endif

# include <openssl/ossl_typ.h>
# ifndef OPENSSL_NO_BIO
#  include <openssl/bio.h>
# endif
# ifndef OPENSSL_NO_LHASH
#  include <openssl/lhash.h>
# endif

#ifdef  __cplusplus
extern "C" {
#endif

# ifndef OPENSSL_NO_ERR
#  define ERR_PUT_error(a,b,c,d,e)        ERR_put_error(a,b,c,d,e)
# else
#  define ERR_PUT_error(a,b,c,d,e)        ERR_put_error(a,b,c,NULL,0)
# endif

# include <errno.h>

# define ERR_TXT_MALLOCED        0x01
# define ERR_TXT_STRING          0x02

# define ERR_FLAG_MARK           0x01
# define ERR_FLAG_CLEAR          0x02

# define ERR_NUM_ERRORS  16
typedef struct err_state_st {
    CRYPTO_THREADID tid;
    int err_flags[ERR_NUM_ERRORS];
    unsigned long err_buffer[ERR_NUM_ERRORS];
    char *err_data[ERR_NUM_ERRORS];
    int err_data_flags[ERR_NUM_ERRORS];
    const char *err_file[ERR_NUM_ERRORS];
    int err_line[ERR_NUM_ERRORS];
    int top, bottom;
} ERR_STATE;

/* library */
# define ERR_LIB_NONE            1
# define ERR_LIB_SYS             2
# define ERR_LIB_BN              3
# define ERR_LIB_RSA             4
# define ERR_LIB_DH              5
# define ERR_LIB_EVP             6
# define ERR_LIB_BUF             7
# define ERR_LIB_OBJ             8
# define ERR_LIB_PEM             9
# define ERR_LIB_DSA             10
# define ERR_LIB_X509            11
/* #define ERR_LIB_METH         12 */
# define ERR_LIB_ASN1            13
# define ERR_LIB_CONF            14
# define ERR_LIB_CRYPTO          15
# define ERR_LIB_EC              16
# define ERR_LIB_SSL             20
/* #define ERR_LIB_SSL23        21 */
/* #define ERR_LIB_SSL2         22 */
/* #define ERR_LIB_SSL3         23 */
/* #define ERR_LIB_RSAREF       30 */
/* #define ERR_LIB_PROXY        31 */
# define ERR_LIB_BIO             32
# define ERR_LIB_PKCS7           33
# define ERR_LIB_X509V3          34
# define ERR_LIB_PKCS12          35
# define ERR_LIB_RAND            36
# define ERR_LIB_DSO             37
# define ERR_LIB_ENGINE          38
# define ERR_LIB_OCSP            39
# define ERR_LIB_UI              40
# define ERR_LIB_COMP            41
# define ERR_LIB_ECDSA           42
# define ERR_LIB_ECDH            43
# define ERR_LIB_STORE           44
# define ERR_LIB_FIPS            45
# define ERR_LIB_CMS             46
# define ERR_LIB_TS              47
# define ERR_LIB_HMAC            48
# define ERR_LIB_JPAKE           49

# define ERR_LIB_USER            128

# define SYSerr(f,r)  ERR_PUT_error(ERR_LIB_SYS,(f),(r),__FILE__,__LINE__)
# define BNerr(f,r)   ERR_PUT_error(ERR_LIB_BN,(f),(r),__FILE__,__LINE__)
# define RSAerr(f,r)  ERR_PUT_error(ERR_LIB_RSA,(f),(r),__FILE__,__LINE__)
# define DHerr(f,r)   ERR_PUT_error(ERR_LIB_DH,(f),(r),__FILE__,__LINE__)
# define EVPerr(f,r)  ERR_PUT_error(ERR_LIB_EVP,(f),(r),__FILE__,__LINE__)
# define BUFerr(f,r)  ERR_PUT_error(ERR_LIB_BUF,(f),(r),__FILE__,__LINE__)
# define OBJerr(f,r)  ERR_PUT_error(ERR_LIB_OBJ,(f),(r),__FILE__,__LINE__)
# define PEMerr(f,r)  ERR_PUT_error(ERR_LIB_PEM,(f),(r),__FILE__,__LINE__)
# define DSAerr(f,r)  ERR_PUT_error(ERR_LIB_DSA,(f),(r),__FILE__,__LINE__)
# define X509err(f,r) ERR_PUT_error(ERR_LIB_X509,(f),(r),__FILE__,__LINE__)
# define ASN1err(f,r) ERR_PUT_error(ERR_LIB_ASN1,(f),(r),__FILE__,__LINE__)
# define CONFerr(f,r) ERR_PUT_error(ERR_LIB_CONF,(f),(r),__FILE__,__LINE__)
# define CRYPTOerr(f,r) ERR_PUT_error(ERR_LIB_CRYPTO,(f),(r),__FILE__,__LINE__)
# define ECerr(f,r)   ERR_PUT_error(ERR_LIB_EC,(f),(r),__FILE__,__LINE__)
# define SSLerr(f,r)  ERR_PUT_error(ERR_LIB_SSL,(f),(r),__FILE__,__LINE__)
# define BIOerr(f,r)  ERR_PUT_error(ERR_LIB_BIO,(f),(r),__FILE__,__LINE__)
# define PKCS7err(f,r) ERR_PUT_error(ERR_LIB_PKCS7,(f),(r),__FILE__,__LINE__)
# define X509V3err(f,r) ERR_PUT_error(ERR_LIB_X509V3,(f),(r),__FILE__,__LINE__)
# define PKCS12err(f,r) ERR_PUT_error(ERR_LIB_PKCS12,(f),(r),__FILE__,__LINE__)
# define RANDerr(f,r) ERR_PUT_error(ERR_LIB_RAND,(f),(r),__FILE__,__LINE__)
# define DSOerr(f,r) ERR_PUT_error(ERR_LIB_DSO,(f),(r),__FILE__,__LINE__)
# define ENGINEerr(f,r) ERR_PUT_error(ERR_LIB_ENGINE,(f),(r),__FILE__,__LINE__)
# define OCSPerr(f,r) ERR_PUT_error(ERR_LIB_OCSP,(f),(r),__FILE__,__LINE__)
# define UIerr(f,r) ERR_PUT_error(ERR_LIB_UI,(f),(r),__FILE__,__LINE__)
# define COMPerr(f,r) ERR_PUT_error(ERR_LIB_COMP,(f),(r),__FILE__,__LINE__)
# define ECDSAerr(f,r)  ERR_PUT_error(ERR_LIB_ECDSA,(f),(r),__FILE__,__LINE__)
# define ECDHerr(f,r)  ERR_PUT_error(ERR_LIB_ECDH,(f),(r),__FILE__,__LINE__)
# define STOREerr(f,r) ERR_PUT_error(ERR_LIB_STORE,(f),(r),__FILE__,__LINE__)
# define FIPSerr(f,r) ERR_PUT_error(ERR_LIB_FIPS,(f),(r),__FILE__,__LINE__)
# define CMSerr(f,r) ERR_PUT_error(ERR_LIB_CMS,(f),(r),__FILE__,__LINE__)
# define TSerr(f,r) ERR_PUT_error(ERR_LIB_TS,(f),(r),__FILE__,__LINE__)
# define HMACerr(f,r) ERR_PUT_error(ERR_LIB_HMAC,(f),(r),__FILE__,__LINE__)
# define JPAKEerr(f,r) ERR_PUT_error(ERR_LIB_JPAKE,(f),(r),__FILE__,__LINE__)

/*
 * Borland C seems too stupid to be able to shift and do longs in the
 * pre-processor :-(
 */
# define ERR_PACK(l,f,r)         (((((unsigned long)l)&0xffL)*0x1000000)| \
                                ((((unsigned long)f)&0xfffL)*0x1000)| \
                                ((((unsigned long)r)&0xfffL)))
# define ERR_GET_LIB(l)          (int)((((unsigned long)l)>>24L)&0xffL)
# define ERR_GET_FUNC(l)         (int)((((unsigned long)l)>>12L)&0xfffL)
# define ERR_GET_REASON(l)       (int)((l)&0xfffL)
# define ERR_FATAL_ERROR(l)      (int)((l)&ERR_R_FATAL)

/* OS functions */
# define SYS_F_FOPEN             1
# define SYS_F_CONNECT           2
# define SYS_F_GETSERVBYNAME     3
# define SYS_F_SOCKET            4
# define SYS_F_IOCTLSOCKET       5
# define SYS_F_BIND              6
# define SYS_F_LISTEN            7
# define SYS_F_ACCEPT            8
# define SYS_F_WSASTARTUP        9/* Winsock stuff */
# define SYS_F_OPENDIR           10
# define SYS_F_FREAD             11
# define SYS_F_FFLUSH            18

/* reasons */
# define ERR_R_SYS_LIB   ERR_LIB_SYS/* 2 */
# define ERR_R_BN_LIB    ERR_LIB_BN/* 3 */
# define ERR_R_RSA_LIB   ERR_LIB_RSA/* 4 */
# define ERR_R_DH_LIB    ERR_LIB_DH/* 5 */
# define ERR_R_EVP_LIB   ERR_LIB_EVP/* 6 */
# define ERR_R_BUF_LIB   ERR_LIB_BUF/* 7 */
# define ERR_R_OBJ_LIB   ERR_LIB_OBJ/* 8 */
# define ERR_R_PEM_LIB   ERR_LIB_PEM/* 9 */
# define ERR_R_DSA_LIB   ERR_LIB_DSA/* 10 */
# define ERR_R_X509_LIB  ERR_LIB_X509/* 11 */
# define ERR_R_ASN1_LIB  ERR_LIB_ASN1/* 13 */
# define ERR_R_CONF_LIB  ERR_LIB_CONF/* 14 */
# define ERR_R_CRYPTO_LIB ERR_LIB_CRYPTO/* 15 */
# define ERR_R_EC_LIB    ERR_LIB_EC/* 16 */
# define ERR_R_SSL_LIB   ERR_LIB_SSL/* 20 */
# define ERR_R_BIO_LIB   ERR_LIB_BIO/* 32 */
# define ERR_R_PKCS7_LIB ERR_LIB_PKCS7/* 33 */
# define ERR_R_X509V3_LIB ERR_LIB_X509V3/* 34 */
# define ERR_R_PKCS12_LIB ERR_LIB_PKCS12/* 35 */
# define ERR_R_RAND_LIB  ERR_LIB_RAND/* 36 */
# define ERR_R_DSO_LIB   ERR_LIB_DSO/* 37 */
# define ERR_R_ENGINE_LIB ERR_LIB_ENGINE/* 38 */
# define ERR_R_OCSP_LIB  ERR_LIB_OCSP/* 39 */
# define ERR_R_UI_LIB    ERR_LIB_UI/* 40 */
# define ERR_R_COMP_LIB  ERR_LIB_COMP/* 41 */
# define ERR_R_ECDSA_LIB ERR_LIB_ECDSA/* 42 */
# define ERR_R_ECDH_LIB  ERR_LIB_ECDH/* 43 */
# define ERR_R_STORE_LIB ERR_LIB_STORE/* 44 */
# define ERR_R_TS_LIB    ERR_LIB_TS/* 45 */

# define ERR_R_NESTED_ASN1_ERROR                 58
# define ERR_R_BAD_ASN1_OBJECT_HEADER            59
# define ERR_R_BAD_GET_ASN1_OBJECT_CALL          60
# define ERR_R_EXPECTING_AN_ASN1_SEQUENCE        61
# define ERR_R_ASN1_LENGTH_MISMATCH              62
# define ERR_R_MISSING_ASN1_EOS                  63

/* fatal error */
# define ERR_R_FATAL                             64
# define ERR_R_MALLOC_FAILURE                    (1|ERR_R_FATAL)
# define ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED       (2|ERR_R_FATAL)
# define ERR_R_PASSED_NULL_PARAMETER             (3|ERR_R_FATAL)
# define ERR_R_INTERNAL_ERROR                    (4|ERR_R_FATAL)
# define ERR_R_DISABLED                          (5|ERR_R_FATAL)

/*
 * 99 is the maximum possible ERR_R_... code, higher values are reserved for
 * the individual libraries
 */

typedef struct ERR_string_data_st {
    unsigned long error;
    const char *string;
} ERR_STRING_DATA;

void ERR_put_error(int lib, int func, int reason, const char *file, int line);
void ERR_set_error_data(char *data, int flags);

unsigned long ERR_get_error(void);
unsigned long ERR_get_error_line(const char **file, int *line);
unsigned long ERR_get_error_line_data(const char **file, int *line,
                                      const char **data, int *flags);
unsigned long ERR_peek_error(void);
unsigned long ERR_peek_error_line(const char **file, int *line);
unsigned long ERR_peek_error_line_data(const char **file, int *line,
                                       const char **data, int *flags);
unsigned long ERR_peek_last_error(void);
unsigned long ERR_peek_last_error_line(const char **file, int *line);
unsigned long ERR_peek_last_error_line_data(const char **file, int *line,
                                            const char **data, int *flags);
void ERR_clear_error(void);
char *ERR_error_string(unsigned long e, char *buf);
void ERR_error_string_n(unsigned long e, char *buf, size_t len);
const char *ERR_lib_error_string(unsigned long e);
const char *ERR_func_error_string(unsigned long e);
const char *ERR_reason_error_string(unsigned long e);
void ERR_print_errors_cb(int (*cb) (const char *str, size_t len, void *u),
                         void *u);
# ifndef OPENSSL_NO_FP_API
void ERR_print_errors_fp(FILE *fp);
# endif
# ifndef OPENSSL_NO_BIO
void ERR_print_errors(BIO *bp);
# endif
void ERR_add_error_data(int num, ...);
void ERR_add_error_vdata(int num, va_list args);
void ERR_load_strings(int lib, ERR_STRING_DATA str[]);
void ERR_unload_strings(int lib, ERR_STRING_DATA str[]);
void ERR_load_ERR_strings(void);
void ERR_load_crypto_strings(void);
void ERR_free_strings(void);

void ERR_remove_thread_state(const CRYPTO_THREADID *tid);
# ifndef OPENSSL_NO_DEPRECATED
void ERR_remove_state(unsigned long pid); /* if zero we look it up */
# endif
ERR_STATE *ERR_get_state(void);

# ifndef OPENSSL_NO_LHASH
LHASH_OF(ERR_STRING_DATA) *ERR_get_string_table(void);
LHASH_OF(ERR_STATE) *ERR_get_err_state_table(void);
void ERR_release_err_state_table(LHASH_OF(ERR_STATE) **hash);
# endif

int ERR_get_next_error_library(void);

int ERR_set_mark(void);
int ERR_pop_to_mark(void);

/* Already defined in ossl_typ.h */
/* typedef struct st_ERR_FNS ERR_FNS; */
/*
 * An application can use this function and provide the return value to
 * loaded modules that should use the application's ERR state/functionality
 */
const ERR_FNS *ERR_get_implementation(void);
/*
 * A loaded module should call this function prior to any ERR operations
 * using the application's "ERR_FNS".
 */
int ERR_set_implementation(const ERR_FNS *fns);

#ifdef  __cplusplus
}
#endif

#endif
