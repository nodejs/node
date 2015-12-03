/* crypto/srp/srp.h */
/*
 * Written by Christophe Renou (christophe.renou@edelweb.fr) with the
 * precious help of Peter Sylvester (peter.sylvester@edelweb.fr) for the
 * EdelKey project and contributed to the OpenSSL project 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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
#ifndef __SRP_H__
# define __SRP_H__

# ifndef OPENSSL_NO_SRP

#  include <stdio.h>
#  include <string.h>

#ifdef  __cplusplus
extern "C" {
#endif

#  include <openssl/safestack.h>
#  include <openssl/bn.h>
#  include <openssl/crypto.h>

typedef struct SRP_gN_cache_st {
    char *b64_bn;
    BIGNUM *bn;
} SRP_gN_cache;


DECLARE_STACK_OF(SRP_gN_cache)

typedef struct SRP_user_pwd_st {
    char *id;
    BIGNUM *s;
    BIGNUM *v;
    const BIGNUM *g;
    const BIGNUM *N;
    char *info;
} SRP_user_pwd;

DECLARE_STACK_OF(SRP_user_pwd)

typedef struct SRP_VBASE_st {
    STACK_OF(SRP_user_pwd) *users_pwd;
    STACK_OF(SRP_gN_cache) *gN_cache;
/* to simulate a user */
    char *seed_key;
    BIGNUM *default_g;
    BIGNUM *default_N;
} SRP_VBASE;

/*
 * Structure interne pour retenir les couples N et g
 */
typedef struct SRP_gN_st {
    char *id;
    BIGNUM *g;
    BIGNUM *N;
} SRP_gN;

DECLARE_STACK_OF(SRP_gN)

SRP_VBASE *SRP_VBASE_new(char *seed_key);
int SRP_VBASE_free(SRP_VBASE *vb);
int SRP_VBASE_init(SRP_VBASE *vb, char *verifier_file);
SRP_user_pwd *SRP_VBASE_get_by_user(SRP_VBASE *vb, char *username);
char *SRP_create_verifier(const char *user, const char *pass, char **salt,
                          char **verifier, const char *N, const char *g);
int SRP_create_verifier_BN(const char *user, const char *pass, BIGNUM **salt,
                           BIGNUM **verifier, BIGNUM *N, BIGNUM *g);

#  define SRP_NO_ERROR 0
#  define SRP_ERR_VBASE_INCOMPLETE_FILE 1
#  define SRP_ERR_VBASE_BN_LIB 2
#  define SRP_ERR_OPEN_FILE 3
#  define SRP_ERR_MEMORY 4

#  define DB_srptype      0
#  define DB_srpverifier  1
#  define DB_srpsalt      2
#  define DB_srpid        3
#  define DB_srpgN        4
#  define DB_srpinfo      5
#  undef  DB_NUMBER
#  define DB_NUMBER       6

#  define DB_SRP_INDEX    'I'
#  define DB_SRP_VALID    'V'
#  define DB_SRP_REVOKED  'R'
#  define DB_SRP_MODIF    'v'

/* see srp.c */
char *SRP_check_known_gN_param(BIGNUM *g, BIGNUM *N);
SRP_gN *SRP_get_default_gN(const char *id);

/* server side .... */
BIGNUM *SRP_Calc_server_key(BIGNUM *A, BIGNUM *v, BIGNUM *u, BIGNUM *b,
                            BIGNUM *N);
BIGNUM *SRP_Calc_B(BIGNUM *b, BIGNUM *N, BIGNUM *g, BIGNUM *v);
int SRP_Verify_A_mod_N(BIGNUM *A, BIGNUM *N);
BIGNUM *SRP_Calc_u(BIGNUM *A, BIGNUM *B, BIGNUM *N);

/* client side .... */
BIGNUM *SRP_Calc_x(BIGNUM *s, const char *user, const char *pass);
BIGNUM *SRP_Calc_A(BIGNUM *a, BIGNUM *N, BIGNUM *g);
BIGNUM *SRP_Calc_client_key(BIGNUM *N, BIGNUM *B, BIGNUM *g, BIGNUM *x,
                            BIGNUM *a, BIGNUM *u);
int SRP_Verify_B_mod_N(BIGNUM *B, BIGNUM *N);

#  define SRP_MINIMAL_N 1024

#ifdef  __cplusplus
}
#endif

# endif
#endif
